#ifndef _UART_INTERFACE_h
#define _UART_INTERFACE_h

#include <UartInterfaceListener.h>

#include "Codec/UartInterfaceCrc.h"
#include "Codec/UartCobsCodec.h"

namespace UartInterface
{
	template<uint32_t baudrate = 115200,
		uint32_t messageSizeMax = 64,
		uint8_t maxSerialStepOut = 32,
		uint8_t maxSerialStepIn = 32,
		uint32_t writeTimeoutMillis = 10,
		uint32_t readTimeoutMillis = 10,
		uint32_t pollPeriodMillis = 50>
	struct TemplateUartDefinitions
	{
		static constexpr uint32_t Baudrate = baudrate;
		static constexpr uint32_t MessageSizeMax = messageSizeMax;
		static constexpr uint8_t MaxSerialStepOut = maxSerialStepOut;
		static constexpr uint8_t MaxSerialStepIn = maxSerialStepIn;

		static constexpr uint32_t WriteTimeoutMillis = writeTimeoutMillis;
		static constexpr uint32_t ReadTimeoutMillis = readTimeoutMillis;
		static constexpr uint32_t PollPeriodMillis = pollPeriodMillis;
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
		static constexpr uint8_t Delimiter = UartCobsCodec::Codes::Delimiter;
		static constexpr uint8_t SizeMin = (uint8_t)FieldIndexEnum::Payload;
		static constexpr uint8_t PayloadSizeMax = UartCobsCodec::DataSizeMax - (uint8_t)FieldIndexEnum::Payload;

		static constexpr uint8_t GetPayloadSize(const size_t messageSize)
		{
			return ((messageSize >= (uint8_t)FieldIndexEnum::Payload) * (messageSize - (uint8_t)FieldIndexEnum::Payload));
		}

		static constexpr size_t GetMessageSize(const uint8_t payloadSize)
		{
			return (size_t)FieldIndexEnum::Payload + payloadSize;
		}
	};
}
#endif