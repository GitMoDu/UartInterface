#ifndef _UART_INTERFACE_CODEC_h
#define _UART_INTERFACE_CODEC_h

#include "UartInterface.h"
#include "UartInterfaceCrc.h"
#include "UartCobsCodec.h"

template<uint8_t PayloadSizeMax>
class UartInterfaceCodec
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

public:
	static constexpr size_t BufferMaxSize = UartCobsCodec::GetBufferSize(MessageDefinition::GetMessageSize(PayloadSizeMax));

private:
	uint8_t Copy[BufferMaxSize]{};
	UartInterfaceCrc Crc;

public:
	UartInterfaceCodec(const uint8_t* key,
		const uint8_t keySize)
		: Crc(key, keySize)
	{
	}

	const bool Setup() const
	{
		return Crc.Setup();
	}

	const uint8_t EncodeMessageAndCrcInPlace(uint8_t* message, const uint8_t messageSize)
	{
		const uint8_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
		const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);

		memcpy(&Copy[(uint8_t)MessageDefinition::FieldIndexEnum::Header], &message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);
		Copy[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0] = (uint8_t)crc;
		Copy[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1] = (uint8_t)((crc >> 8) & UINT8_MAX);

		if (UartCobsCodec::Encode(Copy, message, messageSize) == UartCobsCodec::GetBufferSize(messageSize))
		{
			return UartCobsCodec::GetBufferSize(messageSize);
		}
		else
		{
			return 0;
		}
	}

	const bool MessageValid(uint8_t* message, const uint8_t messageSize)
	{
		const uint8_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
		const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);
		const uint16_t matchCrc = (uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0]
			| (((uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1]) << 8);

		return matchCrc == crc;
	}

	const bool DecodeMessageInPlaceIfValid(uint8_t* buffer, const uint8_t bufferSize)
	{
		if (UartCobsCodec::DecodeInPlace(buffer, bufferSize) == UartCobsCodec::GetDataSize(bufferSize))
		{
			return MessageValid(buffer, UartCobsCodec::GetDataSize(bufferSize));
		}
		else
		{
			return false;
		}
	}
};
#endif