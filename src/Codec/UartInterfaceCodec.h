// UartInterfaceCodec.h

#ifndef _UART_INTERFACE_CODEC_h
#define _UART_INTERFACE_CODEC_h

#include "UartInterface.h"
#include "UartInterfaceCrc.h"
#include "UartCobsCodec.h"

template<uint8_t MessageSizeMax>
class UartInterfaceCodec
{
private:
	using MessageDefinition = UartInterface::MessageDefinition;

private:
	UartCobsInPlaceCodec<MessageSizeMax> Cobs{};
	UartInterfaceCrc Crc;

public:
	UartInterfaceCodec(const uint8_t* key,
		const uint8_t keySize)
		: Crc(key, keySize)
	{}

	const bool Setup() const
	{
		return Crc.Setup();
	}

	const uint8_t EncodeMessageAndCrcInPlace(uint8_t* message, const uint8_t messageSize)
	{
		const uint8_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
		const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);

		message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0] = (uint8_t)crc;
		message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1] = (uint8_t)((crc >> 8) & UINT8_MAX);

		if (Cobs.EncodeInPlace(message, messageSize) == (messageSize + 1))
		{
			return messageSize + 1;
		}
		else
		{
			return 0;
		}
	}

	const bool MessageValid(uint8_t* message, const uint8_t messageSize)
	{
		const uint8_t dataSize = messageSize - (uint8_t)MessageDefinition::FieldIndexEnum::Header;
		const uint16_t matchCrc = (uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0]
			| (((uint16_t)message[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1]) << 8);

		const uint16_t crc = Crc.GetCrc(&message[(uint8_t)MessageDefinition::FieldIndexEnum::Header], dataSize);

		return matchCrc == crc;
	}

	const bool DecodeMessageInPlaceIfValid(uint8_t* buffer, const uint8_t bufferSize)
	{
		if (Cobs.DecodeInPlace(buffer, bufferSize) == (bufferSize - 1))
		{
			return MessageValid(buffer, bufferSize - 1);
		}
		else
		{
			return false;
		}
	}
};
#endif