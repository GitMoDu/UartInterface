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
		uint16_t KeyState = 0;
		bool IsSetup = false;

	public:
		KeyedCrc(const uint8_t* key, const uint8_t keySize)
		{
			if (key != nullptr && keySize > 0)
			{
				Hasher.begin();
				Hasher.add(key, keySize);
				KeyState = Hasher.getFletcher();
				IsSetup = true;
			}
		}

		/// <summary>
		/// Creates a keyed CRC using the uint32_t key's big-endian bytes.
		/// </summary>
		KeyedCrc(const uint32_t key)
			: IsSetup(true)
		{
			const uint8_t keyBytes[] = {
				(uint8_t)(key >> 24),
				(uint8_t)(key >> 16),
				(uint8_t)(key >> 8),
				(uint8_t)key
			};

			Hasher.begin();
			Hasher.add(keyBytes, sizeof(keyBytes));
			KeyState = Hasher.getFletcher();
		}

		bool Setup() const
		{
			return IsSetup;
		}

		uint16_t GetCrc(const uint8_t* data, const uint16_t dataSize)
		{
			Hasher.begin((uint8_t)KeyState, (uint8_t)(KeyState >> 8));
			Hasher.add(data, dataSize);

			return Hasher.getFletcher();
		}
	};
}
#endif