/*
	Library Unit Testing Project.
	Tests COBS and UartInterface::Message codecs.
*/

#define SERIAL_BAUD_RATE 115200

#include <UartInterface.h>
#include <UartInterfaceTask.h>

using namespace UartInterface;

static constexpr uint8_t Key[]{ 0, 1, 2, 3, 4, 5, 6, 7, 8 ,9 ,10, 11 ,12, 13 , 14, 15 };
static constexpr uint8_t KeySize = sizeof(Key);

MessageCodec<> Codec(Key, KeySize);

static constexpr size_t BufferSize = MessageDefinition::GetBufferSizeFromPayload(MessageDefinition::PayloadSizeMax);
uint8_t testBuffer[BufferSize]{};
uint8_t testOutMessage[BufferSize]{};
uint8_t testInMessage[BufferSize]{};

void loop()
{
}

void PrintStorage(const uint16_t index, const uint16_t address, const uint16_t size)
{
	Serial.print(F("\tStorage"));
	Serial.print(index + 1);
	Serial.print(F(" @ "));
	Serial.print(address);
	Serial.print(F("\t("));
	Serial.print(size);
	Serial.print(F(" bytes)"));
	Serial.println();
}

void OnFail()
{
	Serial.println();
	Serial.println(F("Test Failed!"));
	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println(F("--------------------"));

	while (true);;
}

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;

	Serial.println();
	Serial.println(F("Uart Interface Codec Unit Test Start"));
	Serial.println();

	if (!Codec.Setup())
	{
		OnFail();
	}

	CobsEncodeAndDecodeMatch();
	MessageEncodeAndDecodeMatch();

	Serial.println();
	Serial.println(F("All tests passed."));
	Serial.println();

	CobsBenchmark();
	MessageEncodeAndDecodeBenchmark();

	Serial.println(F("All benchmarks complete."));
	Serial.println();
}

void MessageEncodeAndDecodeBenchmark()
{
	Serial.print(F("Codec Key Size: "));
	Serial.print(KeySize);
	Serial.println(F(" bytes"));
	MessageBenchmark(1);
	MessageBenchmark(16);
	MessageBenchmark(32);
	MessageBenchmark(64);
	MessageBenchmark(MessageDefinition::PayloadSizeMax);
	Serial.println();
}

void CobsBenchmark()
{
	CobsBenchmark(1);
	CobsBenchmark(16);
	CobsBenchmark(32);
	CobsBenchmark(64);
	CobsBenchmark(UartCobsCodec::DataSizeMax);
	Serial.println();
}

void MessageEncodeAndDecodeMatch()
{
	// Invalid size should fail.
	for (uint16_t i = size_t(MessageDefinition::PayloadSizeMax) + 1; i <= UINT8_MAX; i++)
	{
		if (MessageEncodeAndDecodeMatch(i))
		{
			Serial.print(F("Message false invalid at "));
			Serial.println(i);
			OnFail();
		}
	}

	// Valid sizes should match.
	for (size_t i = 0; i <= MessageDefinition::PayloadSizeMax; i++)
	{
		if (!MessageEncodeAndDecodeMatch(i))
		{
			Serial.print(F("Message false valid at "));
			Serial.println(i);
			OnFail();
		}
	}
}


void CobsEncodeAndDecodeMatch()
{
	// Invalid size should fail.
	if (CobsEncodeDecodeMatch(0))
	{
		OnFail();
		Serial.print(F("COBS false invalid at "));
		Serial.println(0);
	}

	for (size_t i = UartCobsCodec::DataSizeMax + 1; i <= UINT8_MAX; i++)
	{
		if (CobsEncodeDecodeMatch(i))
		{
			OnFail();
			Serial.print(F("COBS false valid at "));
			Serial.println(i);
		}
	}

	// Valid sizes should match.
	for (size_t i = 1; i <= UartCobsCodec::DataSizeMax; i++)
	{
		if (!CobsEncodeDecodeMatch(i))
		{
			Serial.print(F("COBS false valid at "));
			Serial.println(i);
			OnFail();
		}
	}
}

void MessageBenchmark(const uint8_t payloadSize)
{
	uint32_t start = 0;
	uint32_t end = 0;

	testOutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = 1;

	start = micros();
	const uint8_t outSize = Codec.EncodeMessageAndCrcInPlace(testOutMessage, MessageDefinition::GetMessageSize(payloadSize));
	end = micros();

	Serial.print(F("Message Encode "));
	Serial.print(MessageDefinition::GetMessageSize(payloadSize));
	Serial.print(F(" bytes took "));
	Serial.print(end - start);
	Serial.println(F(" us"));

	if (outSize > 2)
	{
		memcpy(testBuffer, testOutMessage, (size_t)outSize);

		start = micros();
		const bool valid = Codec.DecodeMessageInPlaceIfValid(testBuffer, outSize);
		end = micros();

		Serial.print(F("Message Decode "));
		Serial.print(MessageDefinition::GetMessageSize(payloadSize));
		Serial.print(F(" bytes took "));
		Serial.print(end - start);
		Serial.println(F(" us"));
	}
}

void CobsBenchmark(const uint8_t size)
{
	uint32_t start = 0;
	uint32_t end = 0;

	for (size_t i = 0; i < BufferSize; i++)
	{
		testOutMessage[i] = i;
		testBuffer[i] = i;
		testInMessage[i] = 0;
	}
	start = micros();
	const uint8_t encodedSize = UartCobsCodec::Encode(testOutMessage, testBuffer, size);
	end = micros();

	Serial.print(F("COBS Encode "));
	Serial.print(size);
	Serial.print(F(" bytes took "));
	Serial.print(end - start);
	Serial.println(F(" us"));

	if (encodedSize > 1)
	{
		memcpy(testBuffer, testOutMessage, (size_t)encodedSize);

		start = micros();
		const bool valid = UartCobsCodec::Decode(testBuffer, testInMessage, encodedSize);
		end = micros();

		Serial.print(F("COBS Decode "));
		Serial.print(size);
		Serial.print(F(" bytes took "));
		Serial.print(end - start);
		Serial.println(F(" us"));
	}
}

bool MessageEncodeAndDecodeMatch(const uint8_t payloadSize)
{
	if (payloadSize > MessageDefinition::PayloadSizeMax)
	{
		return false;
	}

	for (uint16_t i = 0; i < uint8_t(MessageDefinition::FieldIndexEnum::Header); i++)
	{
		testBuffer[i] = 0;
		testOutMessage[i] = testBuffer[i];
	}

	for (uint16_t i = uint8_t(MessageDefinition::FieldIndexEnum::Header); i < BufferSize; i++)
	{
		testBuffer[i] = (i + payloadSize);
		testOutMessage[i] = testBuffer[i];
	}

	const uint16_t encodedSize = Codec.EncodeMessageAndCrcInPlace(testBuffer, MessageDefinition::GetMessageSize(payloadSize));

	if (encodedSize != MessageDefinition::GetBufferSizeFromPayload(payloadSize))
	{
		return false;
	}

	const bool valid = Codec.DecodeMessageInPlaceIfValid(testBuffer, encodedSize);

	if (!valid)
	{
		return false;
	}

	for (uint16_t i = (uint8_t)MessageDefinition::FieldIndexEnum::Header; i < MessageDefinition::GetMessageSize(payloadSize); i++)
	{
		if (testBuffer[i] != testOutMessage[i])
		{
			return false;
		}
	}

	return true;
}

bool CobsEncodeDecodeMatch(const size_t size)
{
	if (size == 0
		|| size > UartCobsCodec::DataSizeMax)
	{
		return false;
	}

	for (size_t i = 0; i < BufferSize; i++)
	{
		testOutMessage[i] = i + size;
		testInMessage[i] = testOutMessage[i];
		testBuffer[i] = testOutMessage[i];
	}

	const uint8_t encodedSize = UartCobsCodec::Encode(testOutMessage, testBuffer, size);

	if (encodedSize != size + 1)
	{
		return false;
	}

	const uint8_t decodedSize = UartCobsCodec::Decode(testBuffer, testInMessage, encodedSize);
	const uint8_t decodedInPlaceSize = UartCobsCodec::DecodeInPlace(testBuffer, encodedSize);

	if (decodedSize != size)
	{
		return false;
	}

	if (decodedInPlaceSize != size)
	{
		return false;
	}

	for (uint8_t i = 0; i < size; i++)
	{
		if (testInMessage[i] != testOutMessage[i]
			|| testBuffer[i] != testOutMessage[i])
		{
			return false;
		}
	}

	return true;
}

void LogMessagePreAndAfterEncoding(const uint8_t header, const uint8_t payloadSize)
{
	const uint8_t messageSize = MessageDefinition::GetMessageSize(payloadSize);
	testOutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Header] = header;

	Serial.println();
	Serial.print("\t(payload ");
	Serial.print(payloadSize);
	Serial.println(" bytes)");
	Serial.print("\tmessage: ");
	for (uint8_t i = (uint8_t)MessageDefinition::FieldIndexEnum::Header; i < messageSize; i++)
	{
		Serial.print(testOutMessage[i], DEC);
		Serial.print(',');
	}
	Serial.println();
	Codec.EncodeMessageAndCrcInPlace(testOutMessage, messageSize);
	Serial.print("\tencoded: ");
	for (uint8_t i = 0; i < messageSize + 1; i++)
	{
		Serial.print(testOutMessage[i], DEC);
		if (i < messageSize - 1)
			Serial.print(',');
	}
	Serial.print(F("\t(CRC "));
	Serial.print(((uint16_t)testOutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Crc1] << 8) | testOutMessage[(uint8_t)MessageDefinition::FieldIndexEnum::Crc0]);
	Serial.println(')');
}