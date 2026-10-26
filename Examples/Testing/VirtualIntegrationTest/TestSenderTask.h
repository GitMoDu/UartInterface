#ifndef _TEST_SENDER_TASK_h
#define _TEST_SENDER_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

template<typename InterfaceType, uint8_t MaxPayloadSize = 16>
class TestSenderTask : public TS::Task
{
public:
	static constexpr uint32_t SendIntervalMillis = 100;

private:
	InterfaceType& Interface;

private:
	const uint8_t DeviceId;
	uint8_t TestHeader = 123;
	uint8_t TestPayloadSize = 0;
	uint8_t TestPayload[MaxPayloadSize]{};
	uint8_t SentCount = 0;

public:
	TestSenderTask(TS::Scheduler& scheduler, InterfaceType& interface, const uint8_t deviceId)
		: TS::Task(SendIntervalMillis, TASK_FOREVER, &scheduler, false)
		, Interface(interface)
		, DeviceId(deviceId)
	{
	}

	bool Failed = false;
	bool Complete = false;

	bool Callback() final
	{
		PrintName();
		Serial.print(F("Sending header "));
		Serial.print(TestHeader);
		Serial.print(F(" with payload length "));
		Serial.println(TestPayloadSize);
		if (!Interface.SendMessage(TestHeader, TestPayload, TestPayloadSize))
		{
			Failed = true;
			PrintName();
			Serial.println(F("FAIL: SendMessage rejected the frame"));
			disable();
			return true;
		}

		SentCount++;
		if (SentCount == MaxPayloadSize + 1)
		{
			Complete = true;
			disable();
			return true;
		}

		TestHeader++;
		TestPayloadSize++;

		for (uint8_t i = 0; i < MaxPayloadSize; i++)
		{
			TestPayload[i] += (i + 1);
		}

		if (TestPayloadSize > MaxPayloadSize)
		{
			TestPayloadSize = 0;
		}

		return true;
	}

private:
	void PrintName()
	{
		Serial.print(millis());
		Serial.print(':');
		Serial.print(F("Sender "));
		Serial.print(DeviceId);
		Serial.print(F(": "));
	}
};
#endif