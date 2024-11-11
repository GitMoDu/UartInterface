// UartInterface.h

#ifndef _UART_INTERFACE_LISTENER_h
#define _UART_INTERFACE_LISTENER_h

#include <stdint.h>

class UartInterfaceListener
{
public:
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

public:
	virtual void OnUartStateChange(const bool connected) {}

	virtual void OnUartRx(const uint8_t header) {}
	virtual void OnUartRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) {}

	virtual void OnUartTx() {}

	virtual void OnUartRxError(const RxErrorEnum error) {}
	virtual void OnUartTxError(const TxErrorEnum error) {}
};
#endif