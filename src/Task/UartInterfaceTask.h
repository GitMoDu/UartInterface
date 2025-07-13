#ifndef _UART_INTERFACE_TASK_h
#define _UART_INTERFACE_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>
#include "UartOutTask.h"

namespace UartInterface
{
	template<typename SerialType,
		typename UartDefinitions = UartInterface::TemplateUartDefinitions<>>
		class UartInterfaceTask : public TS::Task
	{
	private:
		using MessageDefinition = UartInterface::MessageDefinition;

	private:
		enum class StateEnum
		{
			Disabled,
			WaitingForSerial,
			PassiveWaitPoll,
			ActiveWaitPoll,
			Accumulating,
			DeliveringMessage
		};

	private:
		static constexpr size_t BufferSize = MessageDefinition::GetBufferSizeFromPayload(UartDefinitions::MaxPayloadSize);

		UartOut::UartOutTask<SerialType, UartDefinitions::MaxSerialStepOut, UartDefinitions::WriteTimeoutMillis> UartWriter;

		MessageCodec<BufferSize> Codec;

	private:
		SerialType& SerialInstance;
		UartListener* Listener;

	private:

		uint8_t OutBuffer[BufferSize]{};
		uint8_t InBuffer[BufferSize]{};

		uint32_t PollStart = 0;
		uint32_t LastIn = 0;
		uint16_t InSize = 0;

	private:
		StateEnum State = StateEnum::Disabled;

	public:
		UartInterfaceTask(TS::Scheduler& scheduler, SerialType& serialInstance, UartListener* listener,
			const uint8_t* key,
			const uint8_t keySize)
			: TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
			, UartWriter(scheduler, serialInstance, OutBuffer, listener)
			, Codec(key, keySize)
			, SerialInstance(serialInstance)
			, Listener(listener)
		{
		}

		bool Setup()
		{
			return Codec.Setup() && UartWriter.Setup();
		}

		void Start()
		{
			UartWriter.Clear();
			State = StateEnum::WaitingForSerial;

			SerialInstance.end();
			SerialInstance.begin(UartDefinitions::Baudrate);

			TS::Task::enableDelayed(0);
		}

		void Stop()
		{
			switch (State)
			{
			case StateEnum::WaitingForSerial:
			case StateEnum::Disabled:
				UartWriter.Clear();
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(false);
				}
			default:
				break;
			}

			State = StateEnum::Disabled;
			TS::Task::disable();
		}

		bool CanSendMessage() const
		{
			switch (State)
			{
			case StateEnum::Disabled:
			case StateEnum::WaitingForSerial:
				return false;
			default:
				break;
			}

			return UartWriter.CanSend();
		}

		bool IsSerialConnected()
		{
			return SerialInstance;
		}

		bool SendMessage(const uint8_t header)
		{
			OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

			const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(0));

			return UartWriter.SendMessage(outSize);
		}

		bool SendMessage(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize)
		{
			if (!CanSendMessage()
				|| payloadSize > UartDefinitions::MaxPayloadSize
				|| payload == nullptr)
			{
				return false;
			}

			OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;
			memcpy(&OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Payload], payload, payloadSize);

			const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(payloadSize));

			return UartWriter.SendMessage(outSize);
		}

		void OnSerialEvent()
		{
			switch (State)
			{
			case StateEnum::PassiveWaitPoll:
				State = StateEnum::ActiveWaitPoll;
				PollStart = millis();
				TS::Task::delay(TASK_IMMEDIATE);
				break;
			case StateEnum::WaitingForSerial:
				TS::Task::enableDelayed(TASK_IMMEDIATE);
				break;
			default:
				break;
			}
		}

		bool Callback() final
		{
			switch (State)
			{
			case StateEnum::WaitingForSerial:
				TS::Task::delay(UartDefinitions::PollPeriodMillis);
				if (SerialInstance)
				{
					if (UartWriter.Start())
					{
						State = StateEnum::ActiveWaitPoll;
						TS::Task::delay(TASK_IMMEDIATE);
						if (Listener != nullptr)
						{
							Listener->OnUartStateChange(true);
						}
					}
				}
				break;
			case StateEnum::PassiveWaitPoll:
				if (!SerialInstance)
				{
					TS::Task::delay(TASK_IMMEDIATE);
					State = StateEnum::WaitingForSerial;
					if (Listener != nullptr)
					{
						Listener->OnUartStateChange(false);
					}
				}
				else if (SerialInstance.available())
				{
					TS::Task::delay(TASK_IMMEDIATE);
					State = StateEnum::ActiveWaitPoll;
					PollStart = millis();
				}
				else
				{
					TS::Task::delay(UartDefinitions::PollPeriodMillis);
				}
				break;
			case StateEnum::ActiveWaitPoll:
				TS::Task::delay(TASK_IMMEDIATE);
				if (!SerialInstance)
				{
					State = StateEnum::WaitingForSerial;
					if (Listener != nullptr)
					{
						Listener->OnUartStateChange(false);
					}
				}
				else if (SerialInstance.available())
				{
					PollStart = millis();
					State = StateEnum::Accumulating;
				}
				else if (millis() - PollStart > UartDefinitions::ReadTimeoutMillis)
				{
					State = StateEnum::PassiveWaitPoll;
				}
				break;
			case StateEnum::Accumulating:
				TS::Task::delay(TASK_IMMEDIATE);
				if (!SerialInstance)
				{
					State = StateEnum::WaitingForSerial;
					if (Listener != nullptr)
					{
						Listener->OnUartStateChange(false);
					}
					break;
				}
				else if (SerialInstance.available())
				{
					LastIn = millis();
					if (SerialInstance.peek() == MessageDefinition::Delimiter)
					{
						SerialInstance.read();
						if (InSize >= MessageDefinition::MessageSizeMin)
						{
							State = StateEnum::DeliveringMessage;
						}
						else
						{
							if (InSize > 0 && Listener != nullptr)
							{
								Listener->OnUartRxError(RxErrorEnum::TooShort);
							}
							InSize = 0;
						}
					}
					else
					{
						InBuffer[InSize++] = SerialInstance.read();
						if (InSize > MessageDefinition::GetBufferSizeFromPayload(UartDefinitions::MaxPayloadSize))
						{
							InSize = 0;
							State = StateEnum::ActiveWaitPoll;
							PollStart = millis();
							if (Listener != nullptr)
							{
								Listener->OnUartRxError(RxErrorEnum::TooLong);
							}
						}
					}
				}
				else if (millis() - LastIn > UartDefinitions::ReadTimeoutMillis)
				{
					InSize = 0;
					PollStart = millis();
					State = StateEnum::ActiveWaitPoll;
				}
				break;
			case StateEnum::DeliveringMessage:
				TS::Task::delay(TASK_IMMEDIATE);
				DeliverMessage();
				InSize = 0;
				State = StateEnum::Accumulating;
				break;
			case StateEnum::Disabled:
			default:
				TS::Task::disable();
				break;
			}

			return true;
		}

	private:
		void DeliverMessage()
		{
			if (InSize >= MessageDefinition::MessageSizeMin)
			{
				if (Codec.DecodeMessageInPlaceIfValid(InBuffer, InSize))
				{
					if (Listener != nullptr)
					{
						if (MessageDefinition::GetPayloadSize(InSize - 1) > 0)
						{
							Listener->OnUartRx(InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header],
								&InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Payload], MessageDefinition::GetPayloadSize(InSize - 1));
						}
						else
						{
							Listener->OnUartRx(InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header]);
						}
					}
				}
				else if (Listener != nullptr)
				{
					Listener->OnUartRxError(RxErrorEnum::Crc);
				}
			}
			else if (InSize > 0 && Listener != nullptr)
			{
				Listener->OnUartRxError(RxErrorEnum::TooShort);
			}
		}
	};
}
#endif