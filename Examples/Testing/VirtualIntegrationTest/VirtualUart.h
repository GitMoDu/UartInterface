#ifndef _VIRTUAL_UART_h
#define _VIRTUAL_UART_h

#include <CircularBuffer.hpp> // https://github.com/rlogiacco/CircularBuffer

/// <summary>
/// Virtual UART Serial driver, for testing.
/// </summary>
namespace VirtualUart
{
	/// <summary>
	/// Listener interface for partner endpoint.
	/// </summary>
	struct ISerial
	{
		virtual void ReceiveBytes(const uint8_t* data, size_t size) {}
		virtual size_t AvailableReceive() {}
	};

	/// <summary>
	/// Virtual UART serial driver with the required Uart interface.
	/// Implements bi-directional uart message passing in ring-buffer fashion.
	/// </summary>
	template<size_t BufferSize = 128>
	class UartSerial : public virtual ISerial
	{
	private:
		enum class StateEnum
		{
			Disabled,
			Enabled
		} State = StateEnum::Disabled;

	private:
		CircularBuffer<uint8_t, BufferSize> RxBuffer;

		const uint8_t DeviceId;

	public:
		ISerial* Receiver = nullptr;

	public:
		UartSerial(const uint8_t deviceId)
			: DeviceId(deviceId)
		{
		}

		size_t AvailableReceive() final
		{
			if (State == StateEnum::Enabled)
			{
				return RxBuffer.capacity - RxBuffer.size();
			}
			else
			{
				return 0;
			}
		}

		void ReceiveBytes(const uint8_t* data, size_t size) final
		{
			if (State == StateEnum::Enabled)
			{
				for (size_t i = 0; i < size; i++)
				{
					RxBuffer.push(data[i]);
				}
			}
			else
			{
				PrintName();
				Serial.println(F("Rejected, disabled."));
			}
		}

	public:
		operator bool() { return Receiver != nullptr; }

		void begin(unsigned long baudRate)
		{
			State = StateEnum::Enabled;
		}

		void end()
		{
			State = StateEnum::Disabled;
		}

		int available()
		{
			return RxBuffer.size() * (State == StateEnum::Enabled);
		}

		int availableForWrite()
		{
			if (State == StateEnum::Enabled)
			{
				if (Receiver != nullptr)
				{
					return Receiver->AvailableReceive();
				}
			}
			else
			{
				return 0;
			}
		}

		void clearWriteError()
		{
		}

		int peek(void)
		{
			if (State == StateEnum::Enabled)
			{
				return RxBuffer.first();
			}
			else
			{
				return 0;
			}
		}

		int read()
		{
			if (State == StateEnum::Enabled)
			{
				return RxBuffer.shift();
			}
			else
			{
				return 0;
			}
		}

		void flush()
		{
		}

		size_t write(const uint8_t data)
		{
			if (State == StateEnum::Enabled)
			{
				if (Receiver != nullptr)
				{
					Receiver->ReceiveBytes(&data, 1);
				}
				else
				{
					PrintName();
					Serial.println(F("Rejected, no receiver."));
				}
			}
			else
			{
				PrintName();
				Serial.println(F("Rejected, disabled."));
			}

			return 1;
		}


		size_t write(const uint8_t* buffer, size_t size)
		{
			if (State == StateEnum::Enabled)
			{
				if (Receiver != nullptr)
				{
					Receiver->ReceiveBytes(buffer, size);

					return size;
				}
				else
				{
					PrintName();
					Serial.println(F("Rejected, no receiver."));
					return 0;
				}
			}
			else
			{
				PrintName();
				Serial.println(F("Rejected, disabled."));
			}
		}

	private:
		void PrintName()
		{
			Serial.print(millis());
			Serial.print(':');
			Serial.print(F("VSerial "));
			Serial.print(DeviceId);
			Serial.print(F(": "));
		}

		void PrintBytes(const uint8_t* buffer, size_t size)
		{
			Serial.print('[');
			for (size_t i = 0; i < size; i++)
			{
				Serial.print(buffer[i]);
				if (i < size - 1)
				{
					Serial.print(',');
				}
			}
			Serial.println(']');
		}
	};
}
#endif
