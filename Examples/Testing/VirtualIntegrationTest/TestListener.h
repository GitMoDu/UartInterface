#ifndef _TEST_LISTENER_h
#define _TEST_LISTENER_h

#include <UartInterface.h>

struct TestListener : UartInterface::IUartListener
{
public:
	static constexpr uint8_t MaxPayloadSize = 16;
	static constexpr uint8_t ExpectedMessageCount = MaxPayloadSize + 1;

private:
	const uint8_t DeviceId;
	uint8_t LastHeader = 0;
	uint8_t LastPayloadSize = 0;
	uint8_t LastPayload[MaxPayloadSize]{};

public:
	uint8_t RxCount = 0;
	uint8_t TxCount = 0;
	uint8_t ConnectedEvents = 0;
	uint8_t DisconnectedEvents = 0;
	uint8_t RxErrorCount = 0;
	uint8_t TxErrorCount = 0;
	bool Connected = false;
	bool Failed = false;
	bool LifecycleExpected = false;

	TestListener(const uint8_t deviceId)
		: UartInterface::IUartListener()
		, DeviceId(deviceId)
	{
	}

public:
	void OnUartStateChange(const bool connected) final
	{
		PrintName();
		Serial.print(F("OnUartStateChange - "));
		if (connected)
		{
			Serial.println(F("Connected"));
			ConnectedEvents++;
			Connected = true;
			if (ConnectedEvents != (LifecycleExpected ? 2 : 1)
				|| (LifecycleExpected && DisconnectedEvents != 1))
			{
				Fail(F("unexpected connection event"));
			}
		}
		else
		{
			Serial.println(F("Not connected"));
			DisconnectedEvents++;
			Connected = false;
			if (!LifecycleExpected || DisconnectedEvents != 1
				|| RxCount != ExpectedMessageCount || TxCount != ExpectedMessageCount)
			{
				Fail(F("unexpected disconnect event"));
			}
		}
	}

	void OnUartRx(const uint8_t header) final
	{
		RecordRx(header, nullptr, 0);
	}

	void OnUartRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize) final
	{
		RecordRx(header, payload, payloadSize);
	}

	void OnUartTx(const uint8_t txSize) final
	{
		PrintName();
		Serial.print(F("OnUartTx - frame size "));
		Serial.println(txSize);
		TxCount++;
		if (TxCount > ExpectedMessageCount)
		{
			Fail(F("unexpected extra TX frame"));
		}
	}

	void OnUartRxError(const UartInterface::RxErrorEnum error) final
	{
		RxErrorCount++;
		PrintName();
		Serial.print(F("FAIL: OnUartRxError - "));
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
		Failed = true;
	}

	void OnUartTxError(const UartInterface::TxErrorEnum error) final
	{
		TxErrorCount++;
		PrintName();
		Serial.print(F("FAIL: OnUartTxError - "));
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
		Failed = true;
	}

public:
	bool IsComplete() const
	{
		return !Failed && Connected && ConnectedEvents == 1 && DisconnectedEvents == 0
			&& RxCount == ExpectedMessageCount && TxCount == ExpectedMessageCount
			&& RxErrorCount == 0 && TxErrorCount == 0;
	}

	void ExpectDisconnectReconnect()
	{
		LifecycleExpected = true;
	}

	bool IsDisconnectedForLifecycle() const
	{
		return LifecycleExpected && !Failed && !Connected
			&& ConnectedEvents == 1 && DisconnectedEvents == 1
			&& RxCount == ExpectedMessageCount && TxCount == ExpectedMessageCount;
	}

	bool IsLifecycleComplete() const
	{
		return LifecycleExpected && !Failed && Connected
			&& ConnectedEvents == 2 && DisconnectedEvents == 1
			&& RxCount == ExpectedMessageCount && TxCount == ExpectedMessageCount
			&& RxErrorCount == 0 && TxErrorCount == 0;
	}

	uint8_t GetLastHeader() const { return LastHeader; }
	uint8_t GetLastPayloadSize() const { return LastPayloadSize; }
	const uint8_t* GetLastPayload() const { return LastPayload; }

private:
	void RecordRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize)
	{
		PrintName();
		Serial.print(F("OnUartRx - ("));
		Serial.print(header);
		Serial.print(F(")("));
		Serial.print(payloadSize);
		Serial.println(F(")"));

		if (RxCount >= ExpectedMessageCount)
		{
			Fail(F("unexpected extra RX frame"));
			return;
		}

		const uint8_t expectedIndex = RxCount;
		const uint8_t expectedHeader = static_cast<uint8_t>(123 + expectedIndex);
		const uint8_t expectedSize = expectedIndex;
		LastHeader = header;
		LastPayloadSize = payloadSize;

		if (header != expectedHeader)
		{
			PrintName();
			Serial.print(F("FAIL: Header mismatch; expected "));
			Serial.print(expectedHeader);
			Serial.print(F(", received "));
			Serial.println(header);
			Failed = true;
		}
		if (payloadSize != expectedSize)
		{
			PrintName();
			Serial.print(F("FAIL: Payload length mismatch; expected "));
			Serial.print(expectedSize);
			Serial.print(F(", received "));
			Serial.println(payloadSize);
			Failed = true;
		}
		if (payloadSize > MaxPayloadSize || (payloadSize > 0 && payload == nullptr))
		{
			Fail(F("invalid RX payload buffer/length"));
			RxCount++;
			return;
		}

		for (uint8_t i = 0; i < payloadSize; i++)
		{
			LastPayload[i] = payload[i];
			const uint8_t expectedByte = static_cast<uint8_t>(expectedIndex * (i + 1));
			if (payload[i] != expectedByte)
			{
				PrintName();
				Serial.print(F("FAIL: Payload byte "));
				Serial.print(i);
				Serial.print(F(" expected "));
				Serial.print(expectedByte);
				Serial.print(F(", received "));
				Serial.println(payload[i]);
				Failed = true;
				break;
			}
		}
		RxCount++;
	}

	void Fail(const __FlashStringHelper* message)
	{
		Failed = true;
		PrintName();
		Serial.print(F("FAIL: "));
		Serial.println(message);
	}

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
