#ifndef _TEST_SENDER_TASK_h
#define _TEST_SENDER_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

template<typename InterfaceType, uint8_t MaxPayloadSize = 16>
class TestSenderTask : public TS::Task
{
private:
	InterfaceType& Interface;

private:
	const uint8_t DeviceId;
	uint8_t TestHeader = 123;
	uint8_t TestPayloadSize = 0;
	uint8_t TestPayload[MaxPayloadSize]{};

public:
	TestSenderTask(TS::Scheduler& scheduler, InterfaceType& interface, const uint8_t deviceId)
		: TS::Task(1000, TASK_FOREVER, &scheduler, false)
		, Interface(interface)
		, DeviceId(deviceId)
	{
	}

	bool Callback() final
	{
		Interface.SendMessage(TestHeader, TestPayload, TestPayloadSize);

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