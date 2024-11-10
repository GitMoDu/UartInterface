// UartInterface.h

#ifndef _UART_INTERFACE_LISTENER_h
#define _UART_INTERFACE_LISTENER_h

#include <stdint.h>

class UartInterfaceListener
{
public:
	enum class UartErrorEnum
	{
		RxStartTimeout,
		RxRejected,
		RxTimeout,
		RxOverflow,
		TxTimeout,
		SetupError
	};

public:
	virtual void OnUartStateChange(const bool connected) {}

	virtual void OnMessageReceived(const uint8_t header) {}

	virtual void OnMessageReceived(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) {}

	virtual void OnUartError(const UartErrorEnum error) {}
};
#endif