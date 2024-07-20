// UartInterfaceTask.h

#ifndef _UART_INTERFACE_TASK_h
#define _UART_INTERFACE_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <UartInterface.h>

#include "UartOutTask.h"

#include "../Codec/UartInterfaceCodec.h"


template<typename SerialType,
	typename UartDefinitions = UartInterface::ExampleUartDefinitions>
class UartInterfaceTask : private Task
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

	static constexpr uint32_t CheckPeriodMillis = 50;

private:
	enum class StateEnum
	{
		Disabled,
		WaitingForSerial,
		WaitingForMessages,
		DecodingMessage,
		DeliveringMessage
	};

private:
	UartOutTask<SerialType, UartDefinitions::MaxSerialStepOut, UartDefinitions::MessageSizeMax>UartWriter;

private:
	UartInterfaceCodec<UartDefinitions::MessageSizeMax> Codec;
	SerialType& SerialInstance;
	UartInterfaceListener* Listener;

	Print* SerialLogger;

private:
	uint8_t OutBuffer[UartDefinitions::MessageSizeMax]{};
	uint8_t InBuffer[UartDefinitions::MessageSizeMax]{};
	uint8_t InSize = 0;

private:
	StateEnum State = StateEnum::Disabled;

public:
	UartInterfaceTask(Scheduler& scheduler, SerialType& serialInstance, UartInterfaceListener* listener,
		const uint8_t* key,
		const uint8_t keySize,
		Print* serialLogger = nullptr)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, UartWriter(scheduler, serialInstance, OutBuffer)
		, Codec(key, keySize)
		, SerialInstance(serialInstance)
		, Listener(listener)
		, SerialLogger(serialLogger)
	{}

	void Start()
	{
		Clear();
		State = StateEnum::WaitingForSerial;

		SerialInstance.begin(UartDefinitions::Baudrate);

		Task::enableDelayed(0);
	}

	const bool CanSendMessage() const
	{
		switch (State)
		{
		case StateEnum::WaitingForSerial:
		case StateEnum::Disabled:
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
		if (!CanSendMessage())
		{
			return false;
		}

		OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

		const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(0));

		return UartWriter.SendMessage(outSize);
	}

	const bool SendMessage(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize)
	{
		if (!CanSendMessage() || payload == nullptr)
		{
			return false;
		}

		OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;
		memcpy(&OutBuffer[(uint8_t)MessageDefinition::FieldIndexEnum::Payload], payload, payloadSize);

		const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutBuffer, MessageDefinition::GetMessageSize(payloadSize));

		return UartWriter.SendMessage(outSize);
	}

	void Stop()
	{
		switch (State)
		{
		case StateEnum::WaitingForSerial:
		case StateEnum::Disabled:
			if (Listener != nullptr)
			{
				Listener->OnUartStateChange(false);
			}
		default:
			break;
		}

		State = StateEnum::Disabled;
		Clear();
		Task::disable();
	}

	void OnSerialEvent()
	{
		switch (State)
		{
		case StateEnum::WaitingForSerial:
			if (Task::isEnabled())
			{
				if (Task::getInterval() != 0)
				{
					Task::delay(0);
				}
			}
			else
			{
				Task::enableDelayed(0);
			}
			break;
		case StateEnum::WaitingForMessages:
			if (!Task::isEnabled())
			{
				Task::enableDelayed(0);
			}
			break;
		case StateEnum::DecodingMessage:
		case StateEnum::DeliveringMessage:
		case StateEnum::Disabled:
		default:
			break;
		}
	}

	bool Callback() final
	{
		switch (State)
		{
		case StateEnum::Disabled:
			Task::disable();
			break;
		case StateEnum::WaitingForSerial:
			if (SerialInstance && UartWriter.Start())
			{
				State = StateEnum::WaitingForMessages;
				Task::delay(0);
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(true);
				}
			}
			else
			{
				Task::delay(CheckPeriodMillis);
			}
			break;
		case StateEnum::WaitingForMessages:
			if (!SerialInstance)
			{
				State = StateEnum::WaitingForSerial;
				Task::delay(0);
				if (Listener != nullptr)
				{
					Listener->OnUartStateChange(false);
				}
			}
			else if (UpdateInSerial())
			{
				State = StateEnum::DecodingMessage;
				Task::delay(0);
			}
			else if (SerialInstance.available())
			{
				Task::delay(0);
			}
			else
			{
				Task::delay(CheckPeriodMillis);
			}
			break;
		case StateEnum::DecodingMessage:
			if (InSize > MessageDefinition::MessageSizeMin
				&& Codec.DecodeMessageInPlaceIfValid(InBuffer, InSize))
			{
				State = StateEnum::DeliveringMessage;
			}
			else
			{
				// On Rx fail.
				if (SerialLogger != nullptr) {
					SerialLogger->println(F("Rx fail"));
				}
				InSize = 0;
				State = StateEnum::WaitingForMessages;
			}
			Task::delay(0);
			break;
		case StateEnum::DeliveringMessage:
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
			InSize = 0;
			State = StateEnum::WaitingForMessages;
			Task::delay(0);
			break;
		default:
			break;
		}

		return true;
	}

private:
	void Clear()
	{
		InSize = 0;
		UartWriter.Clear();
	}

	const bool UpdateInSerial()
	{
		uint8_t steps = 0;

		while (steps < UartDefinitions::MaxSerialStepIn
			&& SerialInstance
			&& SerialInstance.available())
		{
			const uint8_t value = SerialInstance.read();
			if (value == MessageDefinition::MessageEnd)
			{
				// End of message detected.
				return true;
			}
			else
			{
				InBuffer[InSize++] = value;
				steps++;
				if (InSize >= UartDefinitions::MessageSizeMax)
				{
					InSize = 0;
				}
			}
		}

		return false;
	}

#if defined(DEBUG_LOG)
public:
	void LogMessagePreAndAfterEncoding(const uint8_t header, const uint8_t payloadSize)
	{
		uint8_t OutMessage[64];

		const uint8_t messageSize = MessageDefinition::GetMessageSize(payloadSize);
		OutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

		Serial.print("\tmessage: ");
		for (uint8_t i = (uint8_t)MessageDefinition::FieldIndexEnum::Header; i < messageSize; i++)
		{
			Serial.print(OutMessage[i], DEC);
			Serial.print(' ');
		}
		Serial.println();
		Codec.EncodeMessageAndCrcInPlace(OutMessage, messageSize);
		Serial.print("\tencoded: ");
		for (uint8_t i = 0; i < messageSize + 1; i++)
		{
			Serial.print(OutMessage[i], DEC);
			Serial.print(' ');
		}
		Serial.println();
	}


	const bool UnitTest()
	{
		uint8_t OutMessage[64];

		uint32_t start = 0;
		uint32_t end = 0;

		OutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = 1;

		start = micros();
		const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(OutMessage, MessageDefinition::GetMessageSize(40));
		end = micros();

		Serial.print(F("Encode "));
		Serial.print(MessageDefinition::GetMessageSize(40));
		Serial.print(F(" bytes took "));
		Serial.print(end - start);
		Serial.println(F(" us"));

		if (outSize > 2)
		{
			memcpy(InBuffer, OutMessage, (size_t)outSize);

			start = micros();
			const bool valid = Codec.DecodeMessageInPlaceIfValid(InBuffer, outSize);
			end = micros();

			Serial.print(F("Decode "));
			Serial.print(MessageDefinition::GetMessageSize(40));
			Serial.print(F(" bytes took "));
			Serial.print(end - start);
			Serial.println(F(" us"));

			return valid;
		}

		return false;
}
#endif
};
#endif