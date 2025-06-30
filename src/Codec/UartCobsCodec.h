#ifndef _UART_COBS_CODEC_h
#define _UART_COBS_CODEC_h

#include <stdint.h>

namespace UartCobsCodec
{
	namespace Codes
	{
		static constexpr uint8_t Delimiter = 0;
		static constexpr uint8_t EndReplacement = Delimiter + 1;
	}

	static constexpr uint8_t DataSizeMax = UINT8_MAX - 2;


	static constexpr size_t GetBufferSize(const size_t dataSize)
	{
		return dataSize + 1;
	}

	static constexpr size_t GetDataSize(const size_t bufferSize)
	{
		return bufferSize - 1;
	}

	static const uint8_t Encode(const uint8_t* buffer, uint8_t* encodedBuffer, const uint8_t size)
	{
		uint8_t read_index = 0;
		uint8_t write_index = 1;
		uint8_t code_index = 0;
		uint8_t code = 1;

		while (read_index < size)
		{
			if (buffer[read_index] == Codes::Delimiter)
			{
				encodedBuffer[code_index] = code;
				code = Codes::EndReplacement;
				code_index = write_index++;
				read_index++;
			}
			else
			{
				encodedBuffer[write_index++] = buffer[read_index++];
				code++;

				if (code == 0xFF)
				{
					encodedBuffer[code_index] = code;
					code = Codes::EndReplacement;
					code_index = write_index++;
				}
			}
		}

		encodedBuffer[code_index] = code;

		return write_index;
	}

	static const uint8_t Decode(const uint8_t* buffer, uint8_t* decodedBuffer, const uint8_t size)
	{
		if (size == 0)
		{
			return 0;
		}

		uint8_t read_index = 0;
		uint8_t write_index = 0;
		uint8_t code = 0;
		uint8_t i = 0;

		while (read_index < size)
		{
			code = buffer[read_index];

			if (read_index + code > size && code != Codes::EndReplacement)
			{
				return 0;
			}

			read_index++;

			for (i = Codes::EndReplacement; i < code; i++)
			{
				decodedBuffer[write_index++] = buffer[read_index++];
			}

			if (code != 0xFF && read_index != size)
			{
				decodedBuffer[write_index++] = Codes::Delimiter;
			}
		}

		return write_index;
	}

	static const uint8_t DecodeInPlace(uint8_t* buffer, const uint8_t size)
	{
		return Decode(buffer, buffer, size);
	}
};

#endif