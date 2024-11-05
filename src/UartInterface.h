// UartInterface.h

#ifndef _UART_INTERFACE_h
#define _UART_INTERFACE_h

#include <stdint.h>
#include <UartInterfaceListener.h>

#include "Codec/UartInterfaceCrc.h"
#include "Codec/UartCobsCodec.h"

namespace UartInterface
{
	struct ExampleUartDefinitions
	{
		static const uint32_t Baudrate = 115200;
		static constexpr uint32_t MessageSizeMax = 32;
		static constexpr uint8_t MaxSerialStepOut = 16;
		static constexpr uint8_t MaxSerialStepIn = 16;

		static constexpr uint32_t ReadPollPeriodMillis = 50;
		static constexpr uint32_t WriteTimeoutMillis = 10;
	};

	struct MessageDefinition
	{
		enum class FieldIndexEnum : uint8_t
		{
			Crc0 = 0,
			Crc1 = (uint8_t)Crc0 + 1,
			Header = UartInterfaceCrc::CrcSize,
			Payload = (uint8_t)Header + 1
		};

		static constexpr uint8_t CrcSize = UartInterfaceCrc::CrcSize;
		static constexpr uint8_t MessageEnd = UartCobsCodec::MessageEnd; // COBS delimeter.
		static constexpr uint8_t MessageSizeMin = (uint8_t)FieldIndexEnum::Payload;

		static constexpr uint8_t GetPayloadSize(const uint8_t messageSize)
		{
			return ((messageSize >= (uint8_t)FieldIndexEnum::Payload) * (messageSize - (uint8_t)FieldIndexEnum::Payload));
		}

		static constexpr uint8_t GetMessageSize(const uint8_t payloadSize)
		{
			return (uint8_t)FieldIndexEnum::Payload + payloadSize;
		}
	};
}
#endif