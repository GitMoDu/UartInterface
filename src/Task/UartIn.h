// UartIn.h

#ifndef _UART_IN_h
#define _UART_IN_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>

/// <summary>
/// Transforms a stream of delimited data bytes, into a data buffer and size.
/// Overflows reset the stream tracking.
/// </summary>
/// <typeparam name="MessageSizeMax">Max raw size of buffer, including framing and crc.</typeparam>
template<uint8_t MessageSizeMax>
class UartIn
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

	enum class StateEnum
	{
		WaitingForStart,
		WaitingForData,
		ReadingData
	};

private:
	uint8_t* InBuffer;

private:
	StateEnum State = StateEnum::WaitingForStart;
	uint8_t InSize = 0;

public:
	UartIn(uint8_t* inBuffer)
		: InBuffer(inBuffer)
	{}

	void Clear()
	{
		State = StateEnum::WaitingForStart;
		InSize = 0;
	}

	const uint8_t ParseIn(const uint8_t value)
	{
		switch (State)
		{
		case StateEnum::WaitingForStart:
			if (value == MessageDefinition::MessageEnd)
			{
				State = StateEnum::WaitingForData;
				InSize = 0;
			}
			break;
		case StateEnum::WaitingForData:
			if (value == MessageDefinition::MessageEnd)
			{
				// Repeated delimiter detected.
			}
			else
			{
				State = StateEnum::ReadingData;
				InSize = 0;
				InBuffer[InSize++] = value;
			}
			break;
		case StateEnum::ReadingData:
			if (value == MessageDefinition::MessageEnd)
			{
				State = StateEnum::WaitingForStart;
				return InSize;
			}
			else
			{
				InBuffer[InSize++] = value;
				if (InSize >= MessageSizeMax) // Sync error.
				{
					State = StateEnum::WaitingForStart;
					InSize = 0;
				}
			}
			break;
		default:
			break;
		}

		return 0;
	}
};
#endif