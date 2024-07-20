// UartOutTask.h

#ifndef _UART_OUT_TASK_h
#define _UART_OUT_TASK_h

#define _TASK_OO_CALLBACKS
#include <TaskSchedulerDeclarations.h>

#include <UartInterface.h>

template<typename SerialType,
	uint8_t MaxSerialStepOut,
	uint8_t MessageSizeMax
>
class UartOutTask : private Task
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

private:
	SerialType& SerialInstance;
	uint8_t* OutBuffer;

private:
	uint8_t OutSize = 0;
	uint8_t OutIndex = 0;

	bool MarkerPending = false;

public:
	UartOutTask(Scheduler& scheduler, SerialType& serialInstance, uint8_t* outBuffer)
		: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
		, SerialInstance(serialInstance)
		, OutBuffer(outBuffer)
	{}

	void Clear()
	{
		OutSize = 0;
		MarkerPending = false;
		OutIndex = 0;
	}

	const bool Start()
	{
		if (SerialInstance
			&& OutBuffer != nullptr)
		{
			//SerialInstance.disableBlockingTx();
			Clear();
			Task::disable();

			return true;
		}
		else
		{
			return false;
		}
	}

	const bool CanSend() const
	{
		return OutSize == 0 && !MarkerPending;
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
		MarkerPending = true;

		Task::enable();

		return true;
	}

	bool Callback() final
	{
		if (!SerialInstance)
		{
			Clear();
			Task::disable();

			return true;
		}

		if (OutSize > 0)
		{
			uint8_t steps = 0;

			while (OutIndex < OutSize
				&& steps < MaxSerialStepOut
				&& SerialInstance
				&& SerialInstance.availableForWrite())
			{
#if defined(MOCK_UART_INTERFACE)
				SerialInstance.write('0' + OutBuffer[OutIndex++]);
#else
				SerialInstance.write(OutBuffer[OutIndex++]);
#endif

				if (OutIndex >= OutSize)
				{
					OutSize = 0;
					OutIndex = 0;
					MarkerPending = true;
					break;
				}
				else
				{
					steps++;
				}
			}
		}
		else if (MarkerPending)
		{
			if (SerialInstance.availableForWrite())
			{
				SerialInstance.write((uint8_t)(MessageDefinition::MessageEnd));
				//SerialInstance.flush();

				MarkerPending = false;
			}
		}
		else
		{
			Task::disable();

			return false;
		}

		return true;
	}
};
#endif