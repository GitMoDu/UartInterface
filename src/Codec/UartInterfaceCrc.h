#ifndef _UART_INTERFACE_CRC_h
#define _UART_INTERFACE_CRC_h

#include <Fletcher16.h>

/// <summary>
/// Depends on Fletcher16 (https://github.com/RobTillaart/Fletcher).
/// </summary>
class UartInterfaceCrc
{
public:
	static constexpr uint8_t CrcSize = 2;// sizeof(uint16_t);

private:
	Fletcher16 Hasher{};

private:
	const uint8_t* Key;
	const uint8_t KeySize;

public:
	UartInterfaceCrc(const uint8_t* key, const uint8_t keySize)
		: Key(key)
		, KeySize(keySize)
	{}

	const uint16_t GetCrc(const uint8_t* data, const uint8_t dataSize)
	{
		Hasher.begin();
		Hasher.add(dataSize);
		Hasher.add(Key, (uint16_t)KeySize);
		Hasher.add(data, (uint16_t)dataSize);

		return Hasher.getFletcher();
	}
};
#endif