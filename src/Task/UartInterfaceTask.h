// UartInterfaceTask.h

#ifndef _UART_INTERFACE_TASK_h
#define _UART_INTERFACE_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>

#include "UartOutTask.h"

#include "../Codec/UartInterfaceCodec.h"

template<typename SerialType,
	typename UartDefinitions = UartInterface::TemplateUartDefinitions<>>
	class UartInterfaceTask : private TS::Task
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

private:
	enum class StateEnum
	{
		Disabled,
		WaitingForSerial,
		ActiveWaitPoll,
		PassiveWaitPoll,
		ReadWaitingForStart,
		ReadWaitingForData,
		ReadingData,
		DeliveringMessage
	};

private:
	UartOutTask<SerialType, UartDefinitions::MaxSerialStepOut, UartDefinitions::WriteTimeoutMillis> UartWriter;

	UartInterfaceCodec<UartDefinitions::MessageSizeMax> Codec;

private:
	SerialType& SerialInstance;
	UartInterfaceListener* Listener;

private:
	uint8_t OutBuffer[UartDefinitions::MessageSizeMax]{};
	uint8_t InBuffer[UartDefinitions::MessageSizeMax]{};

	uint32_t PollStart = 0;
	uint32_t LastIn = 0;
	uint8_t InSize = 0;

private:
	StateEnum State = StateEnum::Disabled;

public:
	UartInterfaceTask(TS::Scheduler& scheduler, SerialType& serialInstance, UartInterfaceListener* listener,
		const uint8_t* key,
		const uint8_t keySize)
		: TS::Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, UartWriter(scheduler, serialInstance, OutBuffer, listener)
		, Codec(key, keySize)
		, SerialInstance(serialInstance)
		, Listener(listener)
	{}

	const bool Setup() const
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

	const bool CanSendMessage() const
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

	const bool IsSerialConnected() const
	{
		return SerialInstance;
	}

	const bool SendMessage(const uint8_t header)
	{
		OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

		const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(0));

		return UartWriter.SendMessage(outSize);
	}

	const bool SendMessage(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize)
	{
		if (!CanSendMessage()
			|| payloadSize > MessageDefinition::GetPayloadSize(UartDefinitions::MessageSizeMax)
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
					State = StateEnum::PassiveWaitPoll;
					TS::Task::delay(TASK_IMMEDIATE);
					if (Listener != nullptr)
					{
						Listener->OnUartStateChange(true);
					}
				}
				else
				{
					if (Listener != nullptr)
					{
						Listener->OnUartError(UartInterfaceListener::UartErrorEnum::SetupError);
					}
				}
			}
			break;
		case StateEnum::PassiveWaitPoll:
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
				State = StateEnum::ActiveWaitPoll;
				PollStart = millis();
			}
			else if (millis() - PollStart > UartDefinitions::ReadTimeoutMillis)
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
				State = StateEnum::ReadWaitingForStart;
			}
			else if (millis() - PollStart > UartDefinitions::ReadTimeoutMillis)
			{
				State = StateEnum::PassiveWaitPoll;
			}
			break;
		case StateEnum::ReadWaitingForStart:
			TS::Task::delay(TASK_IMMEDIATE);
			if (!SerialInstance)
			{
				State = StateEnum::WaitingForSerial;
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(false);
				}
			}
			else
			{
				PullUntilMessageStart();
			}
			break;
		case StateEnum::ReadWaitingForData:
			TS::Task::delay(0);
			if (!SerialInstance)
			{
				State = StateEnum::WaitingForSerial;
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(false);
				}
			}
			else
			{
				PullUntilDataStart();
			}
			break;
		case StateEnum::ReadingData:
			TS::Task::delay(TASK_IMMEDIATE);
			PullDataIn();
			break;
		case StateEnum::DeliveringMessage:
			TS::Task::delay(TASK_IMMEDIATE);
			DeliverMessage();
			State = StateEnum::PassiveWaitPoll;
			PollStart = millis();
			break;
		case StateEnum::Disabled:
		default:
			TS::Task::disable();
			break;
		}

		return true;
	}

private:
	void PullUntilMessageStart()
	{
		if (millis() - PollStart > UartDefinitions::ReadTimeoutMillis)
		{
			State = StateEnum::PassiveWaitPoll;
		}
		else if (SerialInstance.available()
			&& SerialInstance.read() == MessageDefinition::MessageEnd)
		{
			InSize = 0;
			State = StateEnum::ReadWaitingForData;
			LastIn = millis();
		}
	}

	void PullUntilDataStart()
	{
		if (millis() - LastIn > UartDefinitions::ReadTimeoutMillis)
		{
			State = StateEnum::PassiveWaitPoll;
			PollStart = millis();
			if (Listener != nullptr)
			{
				Listener->OnUartError(UartInterfaceListener::UartErrorEnum::RxStartTimeout);
			}
		}
		else if (SerialInstance.available())
		{
			if (SerialInstance.peek() == MessageDefinition::MessageEnd)
			{
				SerialInstance.read(); // Repeated delimiter detected.
			}
			else
			{
				State = StateEnum::ReadingData;
				InSize = 0;
				InBuffer[InSize++] = SerialInstance.read();
				LastIn = millis();
			}
		}
	}

	void PullDataIn()
	{
		uint8_t readSteps = 0;
		while (readSteps <= UartDefinitions::MaxSerialStepIn)
		{
			readSteps++;
			if (!SerialInstance)
			{
				State = StateEnum::WaitingForSerial;
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(false);
				}
				break;
			}
			else if (millis() - LastIn > UartDefinitions::ReadTimeoutMillis)
			{
				State = StateEnum::PassiveWaitPoll;
				PollStart = millis();
				if (Listener != nullptr)
				{
					Listener->OnUartError(UartInterfaceListener::UartErrorEnum::RxTimeout);
				}
				break;
			}
			else if (SerialInstance.available())
			{
				if (SerialInstance.peek() == MessageDefinition::MessageEnd)
				{
					SerialInstance.read();
					if (InSize > MessageDefinition::MessageSizeMin)
					{
						State = StateEnum::DeliveringMessage;
						break;
					}
					else
					{
						State = StateEnum::PassiveWaitPoll;
						PollStart = millis();
						break;
					}
				}
				else
				{
					InBuffer[InSize++] = SerialInstance.read();
					if (InSize >= UartDefinitions::MessageSizeMax)
					{
						State = StateEnum::PassiveWaitPoll;
						PollStart = millis();
						break;
					}
				}
			}
			else
			{
				break;
			}
		}
	}

	void DeliverMessage()
	{
		if (InSize > MessageDefinition::MessageSizeMin)
		{
			if (Codec.DecodeMessageInPlaceIfValid(InBuffer, InSize))
			{
				if (Listener != nullptr)
				{
					if (MessageDefinition::GetPayloadSize(InSize - 1) > 0)
					{
						Listener->OnMessageReceived(InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header],
							&InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Payload], MessageDefinition::GetPayloadSize(InSize - 1));
					}
					else
					{
						Listener->OnMessageReceived(InBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header]);
					}
				}
			}
			else if (Listener != nullptr)
			{
				Listener->OnUartError(UartInterfaceListener::UartErrorEnum::RxRejected);
			}
		}
	}
};

#endif