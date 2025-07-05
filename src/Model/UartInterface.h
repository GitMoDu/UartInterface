#ifndef _UART_INTERFACE_h
#define _UART_INTERFACE_h

#include "Codec/KeyedCrc.h"
#include "Codec/UartCobsCodec.h"

namespace UartInterface
{
	template<uint32_t baudrate = 115200,
		uint32_t maxPayloadSize = 64,
		uint8_t maxSerialStepOut = 32,
		uint8_t maxSerialStepIn = 32,
		uint32_t writeTimeoutMillis = 50,
		uint32_t readTimeoutMillis = 50,
		uint32_t pollPeriodMillis = 1>
	struct TemplateUartDefinitions
	{
		static constexpr uint32_t Baudrate = baudrate;
		static constexpr uint32_t MaxPayloadSize = maxPayloadSize;
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
			Header = KeyedCrc::CrcSize,
			Payload = (uint8_t)Header + 1
		};

		static constexpr uint8_t Delimiter = UartCobsCodec::Codes::Delimiter;
		static constexpr uint8_t CrcSize = KeyedCrc::CrcSize;
		static constexpr uint8_t MessageSizeMin = (uint8_t)FieldIndexEnum::Payload;
		static constexpr uint8_t MessageSizeMax = UartCobsCodec::DataSizeMax;
		static constexpr uint8_t PayloadSizeMax = UartCobsCodec::DataSizeMax - (uint8_t)FieldIndexEnum::Payload;

		static constexpr uint8_t GetPayloadSize(const uint8_t messageSize)
		{
			return (messageSize >= (uint8_t)FieldIndexEnum::Payload) ? (messageSize - uint8_t(FieldIndexEnum::Payload)) : 0;
		}

		static constexpr uint8_t GetMessageSize(const uint8_t payloadSize)
		{
			return (payloadSize <= PayloadSizeMax) ? (uint8_t(FieldIndexEnum::Payload) + payloadSize) : 0;
		}

		static constexpr uint8_t GetBufferSizeFromPayload(const uint8_t payloadSize)
		{
			return UartCobsCodec::GetBufferSize(GetMessageSize(payloadSize));
		}

		static constexpr uint8_t GetBufferSizeFromMessage(const uint8_t messageSize)
		{
			return UartCobsCodec::GetBufferSize(messageSize);
		}
	};

	enum class TxErrorEnum : uint8_t
	{
		StartTimeout,
		DataTimeout,
		EndTimeout
	};

	enum class RxErrorEnum : uint8_t
	{
		StartTimeout,
		Crc,
		TooShort,
		TooLong,
		EndTimeout
	};

	class UartListener
	{
	public:
		virtual void OnUartStateChange(const bool connected) {}

		virtual void OnUartRx(const uint8_t header) {}
		virtual void OnUartRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) {}

		virtual void OnUartTx() {}

		virtual void OnUartRxError(const RxErrorEnum error) {}
		virtual void OnUartTxError(const TxErrorEnum error) {}
	};
}
#endif