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
	UartInterface::RxErrorEnum ExpectedRxError = UartInterface::RxErrorEnum::Crc;
	UartInterface::TxErrorEnum ExpectedTxError = UartInterface::TxErrorEnum::StartTimeout;
	uint8_t ExpectedAdditionalHeader = 0;
	uint8_t ExpectedAdditionalSize = 0;
	uint8_t ExpectedAdditionalPayload[MaxPayloadSize]{};
	bool ExpectingRxError = false;
	bool ExpectedRxErrorReceived = false;
	bool ExpectingTxError = false;
	bool ExpectedTxErrorReceived = false;
	bool AdditionalRxExpected = false;
	bool AdditionalRxReceived = false;

public:
	uint8_t RxCount = 0;
	uint8_t TxCount = 0;
	uint8_t ConnectedEvents = 0;
	uint8_t DisconnectedEvents = 0;
	uint8_t RxErrorCount = 0;
	uint8_t TxErrorCount = 0;
	uint8_t RxErrorCounts[5]{};
	uint8_t TxErrorCounts[3]{};
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
		RxErrorCounts[static_cast<uint8_t>(error)]++;
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
		if (ExpectingRxError && error == ExpectedRxError && !ExpectedRxErrorReceived)
			ExpectedRxErrorReceived = true;
		else
			Fail(F("unexpected RX error"));
	}

	void OnUartTxError(const UartInterface::TxErrorEnum error) final
	{
		TxErrorCount++;
		TxErrorCounts[static_cast<uint8_t>(error)]++;
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
		if (ExpectingTxError && error == ExpectedTxError && !ExpectedTxErrorReceived)
			ExpectedTxErrorReceived = true;
		else
			Fail(F("unexpected TX error"));
	}

public:
	bool IsComplete() const
	{
		return !Failed && Connected && ConnectedEvents == 1 && DisconnectedEvents == 0
			&& RxCount == ExpectedMessageCount && TxCount == ExpectedMessageCount
			&& RxErrorCount == 0 && TxErrorCount == 0;
	}

	void ExpectRxError(const UartInterface::RxErrorEnum error)
	{
		ExpectedRxError = error;
		ExpectingRxError = true;
		ExpectedRxErrorReceived = false;
	}

	bool HasExpectedRxError() const { return ExpectedRxErrorReceived; }
	void ClearExpectedRxError() { ExpectingRxError = false; }
	uint8_t GetRxErrorCount(const UartInterface::RxErrorEnum error) const
	{
		return RxErrorCounts[static_cast<uint8_t>(error)];
	}

	void ExpectTxError(const UartInterface::TxErrorEnum error)
	{
		ExpectedTxError = error;
		ExpectingTxError = true;
		ExpectedTxErrorReceived = false;
	}

	bool HasExpectedTxError() const { return ExpectedTxErrorReceived; }
	void ClearExpectedTxError() { ExpectingTxError = false; }
	uint8_t GetTxErrorCount(const UartInterface::TxErrorEnum error) const
	{
		return TxErrorCounts[static_cast<uint8_t>(error)];
	}

	void ExpectAdditionalRx(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize)
	{
		if (payloadSize > MaxPayloadSize || (payloadSize > 0 && payload == nullptr))
		{
			Fail(F("invalid expected recovery payload"));
			return;
		}
		ExpectedAdditionalHeader = header;
		ExpectedAdditionalSize = payloadSize;
		for (uint8_t i = 0; i < payloadSize; i++)
			ExpectedAdditionalPayload[i] = payload[i];
		AdditionalRxExpected = true;
		AdditionalRxReceived = false;
	}

	bool HasAdditionalRx() const { return AdditionalRxReceived; }

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
			if (!AdditionalRxExpected || AdditionalRxReceived)
			{
				Fail(F("unexpected extra RX frame"));
				return;
			}
			CheckReceivedFrame(header, payload, payloadSize, ExpectedAdditionalHeader,
				ExpectedAdditionalSize, ExpectedAdditionalPayload);
			AdditionalRxReceived = true;
			return;
		}

		const uint8_t expectedIndex = RxCount;
		const uint8_t expectedHeader = static_cast<uint8_t>(123 + expectedIndex);
		const uint8_t expectedSize = expectedIndex;
		uint8_t expectedPayload[MaxPayloadSize]{};
		for (uint8_t i = 0; i < expectedSize; i++)
			expectedPayload[i] = static_cast<uint8_t>(expectedIndex * (i + 1));
		CheckReceivedFrame(header, payload, payloadSize, expectedHeader, expectedSize, expectedPayload);
		RxCount++;
	}

	void CheckReceivedFrame(const uint8_t header, const uint8_t* payload, const uint8_t payloadSize,
		const uint8_t expectedHeader, const uint8_t expectedSize, const uint8_t* expectedPayload)
	{
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
			return;
		}
		for (uint8_t i = 0; i < payloadSize; i++)
		{
			LastPayload[i] = payload[i];
			if (payload[i] != expectedPayload[i])
			{
				PrintName();
				Serial.print(F("FAIL: Payload byte "));
				Serial.print(i);
				Serial.print(F(" expected "));
				Serial.print(expectedPayload[i]);
				Serial.print(F(", received "));
				Serial.println(payload[i]);
				Failed = true;
			}
		}
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
