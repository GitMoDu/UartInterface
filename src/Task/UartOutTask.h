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
template<typename SerialType,
	uint8_t MaxSerialStepOut,
	uint32_t WriteTimeoutMillis>
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
	UartInterfaceListener* Listener;

private:
	uint32_t OutStart = 0;
	uint8_t OutSize = 0;
	uint8_t OutIndex = 0;

public:
	UartOutTask(TS::Scheduler& scheduler, SerialType& serialInstance, uint8_t* outBuffer, UartInterfaceListener* listener)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SerialInstance(serialInstance)
		, OutBuffer(outBuffer)
		, Listener(listener)
	{}

	const bool Setup() const
	{
		return OutBuffer != nullptr;
	}

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
		return SendState == StateEnum::NotSending && SerialInstance.availableForWrite() >= MaxSerialStepOut;
	}

	const bool SendMessage(const uint8_t messageSize)
	{
		if (!CanSend()
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

		const bool timedOut = (millis() - OutStart) >= WriteTimeoutMillis;

		switch (SendState)
		{
		case StateEnum::SendingStartDelimiter:
			if (!SerialInstance || timedOut)
			{
				SendState = StateEnum::NotSending;
				if (timedOut && Listener != nullptr)
				{
					Listener->OnUartError(UartInterfaceListener::UartErrorEnum::TxTimeout);
				}
			}
			else if (SerialInstance.availableForWrite() > MessageDefinition::MessageSizeMin)
			{
				SerialInstance.write((uint8_t)(MessageDefinition::MessageEnd));
				SendState = StateEnum::SendingData;
			}
			break;
		case StateEnum::SendingData:
			if (OutIndex < OutSize)
			{
				if (!SerialInstance || timedOut)
				{
					SendState = StateEnum::NotSending;
					if (timedOut && Listener != nullptr)
					{
						Listener->OnUartError(UartInterfaceListener::UartErrorEnum::TxTimeout);
					}
					break;
				}
				else
				{
					OutIndex += PushOut();
					if (OutIndex >= OutSize)
					{
						SendState = StateEnum::SendingEndDelimiter;
						break;
					}
				}
			}
			else
			{
				SendState = StateEnum::SendingEndDelimiter;
			}
			break;
		case StateEnum::SendingEndDelimiter:
			if (!SerialInstance || timedOut)
			{
				SendState = StateEnum::NotSending;
				if (timedOut && Listener != nullptr)
				{
					Listener->OnUartError(UartInterfaceListener::UartErrorEnum::TxTimeout);
				}
			}
			else if (SerialInstance
				&& SerialInstance.availableForWrite())
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

private:
	const uint8_t PushOut()
	{
		uint8_t size = OutSize - OutIndex;

		if (size > MaxSerialStepOut)
		{
			size = MaxSerialStepOut;
		}

		const uint8_t available = SerialInstance.availableForWrite();
		if (size > available)
		{
			size = available;
		}

		if (size > 0)
		{
			SerialInstance.write(&OutBuffer[OutIndex], size);
		}

		return size;
	}
};
#endif