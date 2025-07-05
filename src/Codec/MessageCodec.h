#ifndef _UART_INTERFACE_MESSAGE_CODEC_h
#define _UART_INTERFACE_MESSAGE_CODEC_h

#include "UartCobsCodec.h"
#include "KeyedCrc.h"
#include "../Model/UartInterface.h"

namespace UartInterface
{
	template<uint8_t PayloadSizeMax = MessageDefinition::PayloadSizeMax>
	class MessageCodec
	{
	private:
		using MessageDefinition = UartInterface::MessageDefinition;

	public:
		static constexpr uint16_t BufferSize = MessageDefinition::GetBufferSizeFromPayload(PayloadSizeMax);

	private:
		uint8_t Copy[BufferSize]{};
		KeyedCrc Crc;

	public:
		MessageCodec(const uint8_t* key, const uint8_t keySize)
			: Crc(key, keySize)
		{
		}

		bool Setup()
		{
			return Crc.Setup();
		}

		uint16_t EncodeMessageAndCrcInPlace(uint8_t* message, const uint16_t messageSize)
		{
			if (messageSize < uint8_t(MessageDefinition::FieldIndexEnum::Header))
			{
				return 0;
			}

			const uint16_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
			const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);

			if (dataSize > 0)
			{
				memcpy(&Copy[uint8_t(MessageDefinition::FieldIndexEnum::Header)],
					&message[uint8_t(MessageDefinition::FieldIndexEnum::Header)],
					dataSize);
			}
			Copy[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0] = (uint8_t)crc;
			Copy[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1] = (uint8_t)((crc >> 8) & UINT8_MAX);

			const uint16_t outSize = UartCobsCodec::GetBufferSize(messageSize);

			if (UartCobsCodec::Encode(Copy, message, messageSize) == outSize)
			{
				return outSize;
			}
			else
			{
				return 0;
			}
		}

		bool MessageValid(uint8_t* message, const uint16_t messageSize)
		{
			if (messageSize < (uint8_t)MessageDefinition::FieldIndexEnum::Header)
			{
				return 0;
			}

			const uint16_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
			const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);
			const uint16_t matchCrc = (uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0]
				| (((uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1]) << 8);

			return matchCrc == crc;
		}

		bool DecodeMessageInPlaceIfValid(uint8_t* buffer, const uint16_t bufferSize)
		{
			const uint16_t messageSize = UartCobsCodec::GetDataSize(bufferSize);

			if (messageSize >= MessageDefinition::MessageSizeMin
				&& UartCobsCodec::DecodeInPlace(buffer, bufferSize) == messageSize)
			{
				return MessageValid(buffer, messageSize);
			}
			else
			{
				return false;
			}
		}
	};
}
#endif