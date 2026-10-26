#ifndef _UART_INTERFACE_TASK_h
#define _UART_INTERFACE_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>
#include "UartOutTask.h"

namespace UartInterface
{
	template<typename SerialType,
		typename UartDefinitions = UartInterface::TemplateUartDefinitions<>
	>
	class UartInterfaceTask : public UartInterface::IUartInterface
		, public TS::Task
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
		IUartListener* Listener;

	private:

		uint8_t OutBuffer[BufferSize]{};
		uint8_t InBuffer[BufferSize]{};

		uint32_t PollStart = 0;
		uint32_t LastIn = 0;
		uint16_t InSize = 0;

	private:
		StateEnum State = StateEnum::Disabled;

	public:
		UartInterfaceTask(TS::Scheduler& scheduler, SerialType& serialInstance, IUartListener* listener,
			const uint8_t* key,
			const uint8_t keySize)
			: UartInterface::IUartInterface()
			, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
			, UartWriter(scheduler, serialInstance, OutBuffer, listener)
			, Codec(key, keySize)
			, SerialInstance(serialInstance)
			, Listener(listener)
		{}

		UartInterfaceTask(TS::Scheduler& scheduler, SerialType& serialInstance, IUartListener* listener,
			const uint32_t key)
			: UartInterface::IUartInterface()
			, TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
			, UartWriter(scheduler, serialInstance, OutBuffer, listener)
			, Codec(key)
			, SerialInstance(serialInstance)
			, Listener(listener)
		{}

		virtual void SetListener(IUartListener* listener) override
		{
			Listener = listener;
		}

		TS::Task& GetUartOutTask()
		{
			return UartWriter;
		}

		bool Setup()
		{
			return Codec.Setup() && UartWriter.Setup();
		}

		void Start()
		{
			UartWriter.Clear();
			PollStart = 0;
			LastIn = 0;
			InSize = 0;
			State = StateEnum::WaitingForSerial;
			SerialInstance.begin((unsigned long)UartDefinitions::Baudrate);

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

		virtual bool CanSendMessage() override
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

		virtual bool IsSerialConnected() override
		{
			return SerialInstance;
		}

		virtual bool SendMessage(const uint8_t header) override
		{
			OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

			const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(0));

			return CanSendMessage() && UartWriter.SendMessage(outSize);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1);
		}


		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3);
		}


		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3,
			const uint8_t* payload4, const uint8_t payloadSize4) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3, payload4, payloadSize4);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3,
			const uint8_t* payload4, const uint8_t payloadSize4,
			const uint8_t* payload5, const uint8_t payloadSize5) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3, payload4, payloadSize4, payload5, payloadSize5);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3,
			const uint8_t* payload4, const uint8_t payloadSize4,
			const uint8_t* payload5, const uint8_t payloadSize5,
			const uint8_t* payload6, const uint8_t payloadSize6)override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3, payload4, payloadSize4, payload5, payloadSize5, payload6, payloadSize6);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3,
			const uint8_t* payload4, const uint8_t payloadSize4,
			const uint8_t* payload5, const uint8_t payloadSize5,
			const uint8_t* payload6, const uint8_t payloadSize6,
			const uint8_t* payload7, const uint8_t payloadSize7) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3, payload4, payloadSize4, payload5, payloadSize5, payload6, payloadSize6, payload7, payloadSize7);
		}

		virtual bool SendMessage(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			const uint8_t* payload2, const uint8_t payloadSize2,
			const uint8_t* payload3, const uint8_t payloadSize3,
			const uint8_t* payload4, const uint8_t payloadSize4,
			const uint8_t* payload5, const uint8_t payloadSize5,
			const uint8_t* payload6, const uint8_t payloadSize6,
			const uint8_t* payload7, const uint8_t payloadSize7,
			const uint8_t* payload8, const uint8_t payloadSize8) override
		{
			return SendMessageMultiPayload(header, payload1, payloadSize1, payload2, payloadSize2, payload3, payloadSize3, payload4, payloadSize4, payload5, payloadSize5, payload6, payloadSize6, payload7, payloadSize7, payload8, payloadSize8);
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
						PollStart = millis();
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
					if (InSize > 0 && Listener != nullptr)
					{
						Listener->OnUartRxError(RxErrorEnum::EndTimeout);
					}
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

	private:
		template<typename... PayloadArgs>
		bool SendMessageMultiPayload(const uint8_t header,
			const uint8_t* payload1, const uint8_t payloadSize1,
			PayloadArgs... payloadArgs)
		{
			static_assert((sizeof...(payloadArgs) % 2) == 0,
				"SendMessage payload arguments must be pointer/size pairs.");

			uint8_t totalPayloadSize = 0;
			if (!ValidatePayloadParts(totalPayloadSize, payload1, payloadSize1, payloadArgs...))
			{
				return false;
			}

			OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;
			uint8_t payloadOffset = (uint8_t)MessageDefinition::FieldIndexEnum::Payload;
			CopyPayloadParts(payloadOffset, payload1, payloadSize1, payloadArgs...);

			const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(totalPayloadSize));

			return CanSendMessage() && UartWriter.SendMessage(outSize);
		}

		bool ValidatePayloadParts(uint8_t& totalPayloadSize)
		{
			return true;
		}

		template<typename... PayloadArgs>
		bool ValidatePayloadParts(uint8_t& totalPayloadSize,
			const uint8_t* payload, const uint8_t payloadSize,
			PayloadArgs... payloadArgs)
		{
			if ((payloadSize > 0 && payload == nullptr)
				|| totalPayloadSize + payloadSize > UartDefinitions::MaxPayloadSize)
			{
				return false;
			}

			totalPayloadSize += payloadSize;

			return ValidatePayloadParts(totalPayloadSize, payloadArgs...);
		}

		void CopyPayloadParts(uint8_t& payloadOffset)
		{}

		template<typename... PayloadArgs>
		void CopyPayloadParts(uint8_t& payloadOffset,
			const uint8_t* payload, const uint8_t payloadSize,
			PayloadArgs... payloadArgs)
		{
			if (payloadSize > 0)
			{
				memcpy(&OutBuffer[payloadOffset], payload, payloadSize);
				payloadOffset += payloadSize;
			}

			CopyPayloadParts(payloadOffset, payloadArgs...);
		}
	};
}
#endif