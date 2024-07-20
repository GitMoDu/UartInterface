// UartInterface.h

#ifndef _UART_INTERFACE_LISTENER_h
#define _UART_INTERFACE_LISTENER_h

#include <stdint.h>

struct UartInterfaceListener
{
	virtual void OnUartStateChange(const bool connected) {}

	virtual void OnMessageReceived(const uint8_t header) {}

	virtual void OnMessageReceived(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) {}
};
#endif