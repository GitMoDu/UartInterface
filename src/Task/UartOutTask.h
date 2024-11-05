// UartOutTask.h

#ifndef _UART_OUT_TASK_h
#define _UART_OUT_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>

/// <summary>
/// Stream writer.
/// </summary>
/// <typeparam name="SerialType"></typeparam>
/// <typeparam name="MaxSerialStepOut"></typeparam>
/// <typeparam name="MessageSizeMax"></typeparam>
template<typename SerialType,
	uint8_t MaxSerialStepOut,
	uint8_t MessageSizeMax,
	uint32_t WriteTimeoutMillis = 20>
class UartOutTask : private TS::Task
{
private:
	enum class StateEnum : uint8_t
	{
		NotSending,
		SendingStartDelimiter,
		SendingData,
		SendingEndDelimiter
	};

	StateEnum SendState = StateEnum::NotSending;

private:
	using MessageDefinition = UartInterface::MessageDefinition;

private:
	SerialType& SerialInstance;
	uint8_t* OutBuffer;

private:
	uint32_t OutStart = 0;
	uint8_t OutSize = 0;
	uint8_t OutIndex = 0;

public:
	UartOutTask(TS::Scheduler& scheduler, SerialType& serialInstance, uint8_t* outBuffer)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SerialInstance(serialInstance)
		, OutBuffer(outBuffer)
	{}

	void Clear()
	{
		OutSize = 0;
		SendState = StateEnum::NotSending;
		OutIndex = 0;
		SerialInstance.clearWriteError();
	}

	const bool Start()
	{
		if (OutBuffer != nullptr)
		{
			Clear();
			TS::Task::disable();

			return true;
		}
		else
		{
			return false;
		}
	}

	const bool CanSend() const
	{
		return SendState == StateEnum::NotSending;
	}

	const bool SendMessage(const uint8_t messageSize)
	{
		if (!CanSend()
			|| messageSize > MessageSizeMax
			|| messageSize < MessageDefinition::MessageSizeMin)
		{
			return false;
		}

		OutSize = messageSize;
		OutIndex = 0;
		SendState = StateEnum::SendingStartDelimiter;

		OutStart = millis();

		TS::Task::enableDelayed(TASK_IMMEDIATE);

		return true;
	}

	bool Callback() final
	{
		if (!SerialInstance)
		{
			Clear();
			TS::Task::disable();

			return true;
		}

		const bool writeTimeout = (millis() - OutStart) > WriteTimeoutMillis;
		uint8_t steps = 0;

		switch (SendState)
		{
		case StateEnum::SendingStartDelimiter:
			if (!SerialInstance || writeTimeout)
			{
				SendState = StateEnum::NotSending;
			}
			else if (SerialInstance.availableForWrite())
			{
				SerialInstance.write((uint8_t)(MessageDefinition::MessageEnd));
				SendState = StateEnum::SendingData;
			}
			break;
		case StateEnum::SendingData:
			if (!SerialInstance || writeTimeout)
			{
				SendState = StateEnum::NotSending;
			}
			else
			{
				while (OutIndex < OutSize
					&& steps < MaxSerialStepOut
					&& SerialInstance.availableForWrite())
				{
					SerialInstance.write(OutBuffer[OutIndex++]);

					if (OutIndex >= OutSize)
					{
						OutSize = 0;
						OutIndex = 0;
						SendState = StateEnum::SendingEndDelimiter;
						break;
					}
					else
					{
						steps++;
					}
				}
			}
			break;
		case StateEnum::SendingEndDelimiter:
			if (!SerialInstance || writeTimeout)
			{
				SendState = StateEnum::NotSending;
			}
			else if (SerialInstance.availableForWrite())
			{
				SerialInstance.write((uint8_t)(MessageDefinition::MessageEnd));
				SendState = StateEnum::NotSending;
			}
			break;
		case StateEnum::NotSending:
		default:
			TS::Task::disable();
			break;
		}

		return true;
	}
};
#endif