#ifndef _UART_INTERFACE_KEYED_CRC_h
#define _UART_INTERFACE_KEYED_CRC_h

#include <Fletcher16.h>

namespace UartInterface
{
	/// <summary>
	/// Keyed CRC based on Fletcher16 CRC.
	/// Depends on Fletcher16 (https://github.com/RobTillaart/Fletcher).
	/// </summary>
	class KeyedCrc
	{
	public:
		/// <summary>
		/// Fletcher16 sizeof(uint16_t).
		/// </summary>
		static constexpr uint8_t CrcSize = 2;

	private:
		Fletcher16 Hasher{};

	private:
		const uint8_t* Key;
		const uint8_t KeySize;

	public:
		KeyedCrc(const uint8_t* key, const uint8_t keySize)
			: Key(key)
			, KeySize(keySize)
		{
		}

		bool Setup() const
		{
			return Key != nullptr && KeySize > 0;
		}

		uint16_t GetCrc(const uint8_t* data, const uint16_t dataSize)
		{
			Hasher.begin();
			Hasher.add(Key, (uint16_t)KeySize);
			Hasher.add(data, dataSize);

			return Hasher.getFletcher();
		}
	};
}
#endif