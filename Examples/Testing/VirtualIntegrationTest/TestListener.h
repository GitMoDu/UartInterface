#ifndef _TEST_LISTENER_h
#define _TEST_LISTENER_h

#include <UartInterface.h>

struct TestListener : UartInterface::UartListener
{
private:
	const uint8_t DeviceId;

public:
	TestListener(const uint8_t deviceId)
		: DeviceId(deviceId)
	{
	}

public:
	void OnUartStateChange(const bool connected)
	{
		PrintName();
		Serial.println(F("OnUartStateChange - "));
		if (connected)
		{
			Serial.println(F("Connected"));
		}
		else
		{
			Serial.println(F("Not connected"));
		}
	}

	void OnUartRx(const uint8_t header) final
	{
		PrintName();
		Serial.print(F("OnUartRx - ("));
		Serial.print(header);
		Serial.print(F(")("));
		Serial.print(0);
		Serial.println(F(")"));
	}

	void OnUartRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) final
	{
		PrintName();
		Serial.print(F("OnUartRx - ("));
		Serial.print(header);
		Serial.print(F(")("));
		Serial.print(payloadSize);
		Serial.println(F(")"));
	}

	void OnUartTx() final
	{
		PrintName();
		Serial.println(F("OnUartTx"));
	}

	void OnUartRxError(const UartInterface::RxErrorEnum error) final
	{
		PrintName();
		Serial.print(F("OnUartRxError - "));
		switch (error)
		{
		case UartInterface::RxErrorEnum::StartTimeout:
			Serial.println(F("StartTimeout"));
			break;
		case UartInterface::RxErrorEnum::Crc:
			Serial.println(F("Crc"));
			break;
		case UartInterface::RxErrorEnum::TooShort:
			Serial.println(F("TooShort"));
			break;
		case UartInterface::RxErrorEnum::TooLong:
			Serial.println(F("TooLong"));
			break;
		case UartInterface::RxErrorEnum::EndTimeout:
			Serial.println(F("EndTimeout"));
			break;
		default:
			break;
		}
	}

	void OnUartTxError(const UartInterface::TxErrorEnum error) final
	{
		PrintName();
		Serial.print(F("OnUartTxError - "));
		switch (error)
		{
		case UartInterface::TxErrorEnum::StartTimeout:
			Serial.println(F("StartTimeout"));
			break;
		case UartInterface::TxErrorEnum::DataTimeout:
			Serial.println(F("DataTimeout"));
			break;
		case UartInterface::TxErrorEnum::EndTimeout:
			Serial.println(F("EndTimeout"));
			break;
		default:
			break;
		}
	}

private:
	void PrintName()
	{
		Serial.print(millis());
		Serial.print(':');
		Serial.print(F("Listener "));
		Serial.print(DeviceId);
		Serial.print(F(": "));
	}
};
#endif
