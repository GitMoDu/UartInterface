/*
	Library Integration Testing Project with Virtual Uart cross-wire loopback.
	UartInterfaceTask depends on TaskScheduler (https://github.com/arkhipenko/TaskScheduler).
	VirtualUart depends on CircularBuffer (https://github.com/rlogiacco/CircularBuffer).
*/

#define SERIAL_BAUD_RATE 115200

#define _TASK_OO_CALLBACKS
#include <TScheduler.hpp>

#include <UartInterface.h>
#include <UartInterfaceTask.h>

#include "VirtualUart.h"
#include "TestListener.h"
#include "TestSenderTask.h"

static constexpr uint32_t TestTimeoutMillis = 60000;
static constexpr uint32_t LifecyclePhaseTimeoutMillis = 5000;
static constexpr uint32_t SenderStaggerMillis = 50;

enum class TestPhaseEnum : uint8_t
{
	PayloadSequence,
	WaitingForDisconnect,
	WaitingForReconnect,
	Finished
};

uint32_t TestStartMillis = 0;
uint32_t PhaseStartMillis = 0;
bool TestFinished = false;
TestPhaseEnum TestPhase = TestPhaseEnum::PayloadSequence;

void StopSenders();
void PrintCounts();
void ReportFailure(const __FlashStringHelper* reason);


// UART definitions for the test.
namespace Definitions
{
	static constexpr uint8_t Key[]{ 1, 2, 3, 4, 5, 6, 7, 8 };
	static constexpr uint8_t KeySize = sizeof(Key);

	using UartDefinitions = UartInterface::TemplateUartDefinitions<>;

	using VirtualUartType = VirtualUart::UartSerial<>;
	using UartInterfaceTaskType = UartInterface::UartInterfaceTask<VirtualUartType, UartDefinitions>;
}

// Process scheduler.
TS::Scheduler SchedulerBase{};

// Test receiver tasks for both endpoints.
TestListener Listener1(1);
TestListener Listener2(2);

// Endpoints 1 and 2 instances.
Definitions::VirtualUartType Uart1(1);
Definitions::VirtualUartType Uart2(2);
Definitions::UartInterfaceTaskType Interface1(SchedulerBase, Uart1, &Listener1, Definitions::Key, Definitions::KeySize);
Definitions::UartInterfaceTaskType Interface2(SchedulerBase, Uart2, &Listener2, Definitions::Key, Definitions::KeySize);

// Test sender tasks for both endpoints.
using TestSenderTaskType = TestSenderTask<Definitions::UartInterfaceTaskType>;
TestSenderTaskType Sender1(SchedulerBase, Interface1, 1);
TestSenderTaskType Sender2(SchedulerBase, Interface2, 2);

void setup()
{
	Serial.begin(SERIAL_BAUD_RATE);
	while (!Serial)
		;

	Serial.println();
	Serial.println(F("Uart Interface Integration Test Start"));
	Serial.println();

	// Setup interfaces.
	if (!Interface1.Setup() ||
		!Interface2.Setup())
	{
		Serial.println(F("FAIL: Setup failed"));
		while (true)
			;;
	}

	// Setup virtual UART cross-wire connections.
	Uart1.Receiver = &Uart2;
	Uart2.Receiver = &Uart1;

	// Start both endpoints.
	Interface1.Start();
	Interface2.Start();
	TestStartMillis = millis();
	PhaseStartMillis = TestStartMillis;

	// Start sending test messages from both ends.
	Sender1.enableDelayed(TestSenderTaskType::SendIntervalMillis);
	Sender2.enableDelayed(TestSenderTaskType::SendIntervalMillis + SenderStaggerMillis);
}

void loop()
{
	SchedulerBase.execute();

	if (TestFinished)
		return;

	if (Sender1.Failed || Sender2.Failed)
	{
		StopSenders();
		ReportFailure(F("sender rejected a frame"));
	}
	else if (Listener1.Failed || Listener2.Failed)
	{
		StopSenders();
		ReportFailure(F("listener assertion or UART error"));
	}
	else if (TestPhase == TestPhaseEnum::PayloadSequence
		&& Sender1.Complete && Sender2.Complete && Listener1.IsComplete() && Listener2.IsComplete())
	{
		StopSenders();
		Listener1.ExpectDisconnectReconnect();
		Listener2.ExpectDisconnectReconnect();
		Serial.println(F("Payload sequence passed; forcing disconnect"));
		Uart1.Receiver = nullptr;
		Uart2.Receiver = nullptr;
		TestPhase = TestPhaseEnum::WaitingForDisconnect;
		PhaseStartMillis = millis();
	}
	else if (TestPhase == TestPhaseEnum::PayloadSequence
		&& millis() - TestStartMillis >= TestTimeoutMillis)
	{
		StopSenders();
		ReportFailure(F("timeout waiting for both endpoints to complete"));
	}
	else if (TestPhase == TestPhaseEnum::WaitingForDisconnect
		&& Listener1.IsDisconnectedForLifecycle() && Listener2.IsDisconnectedForLifecycle()
		&& !Interface1.IsSerialConnected() && !Interface2.IsSerialConnected())
	{
		Serial.println(F("Both endpoints reported disconnect; reconnecting"));
		Uart1.Receiver = &Uart2;
		Uart2.Receiver = &Uart1;
		TestPhase = TestPhaseEnum::WaitingForReconnect;
		PhaseStartMillis = millis();
	}
	else if (TestPhase == TestPhaseEnum::WaitingForDisconnect
		&& millis() - PhaseStartMillis >= LifecyclePhaseTimeoutMillis)
	{
		ReportFailure(F("timeout waiting for disconnect callbacks"));
	}
	else if (TestPhase == TestPhaseEnum::WaitingForReconnect
		&& Listener1.IsLifecycleComplete() && Listener2.IsLifecycleComplete()
		&& Interface1.IsSerialConnected() && Interface2.IsSerialConnected())
	{
		PrintCounts();
		Serial.println(F("All tests passed"));
		TestFinished = true;
		TestPhase = TestPhaseEnum::Finished;
	}
	else if (TestPhase == TestPhaseEnum::WaitingForReconnect
		&& millis() - PhaseStartMillis >= LifecyclePhaseTimeoutMillis)
	{
		ReportFailure(F("timeout waiting for reconnect callbacks"));
	}
}

void StopSenders()
{
	Sender1.disable();
	Sender2.disable();
}

void PrintCounts()
{
	Serial.print(F("Endpoint 1: TX="));
	Serial.print(Listener1.TxCount);
	Serial.print(F(" RX="));
	Serial.print(Listener1.RxCount);
	Serial.print(F(" connections="));
	Serial.print(Listener1.ConnectedEvents);
	Serial.print(F(" disconnections="));
	Serial.println(Listener1.DisconnectedEvents);
	Serial.print(F("Endpoint 2: TX="));
	Serial.print(Listener2.TxCount);
	Serial.print(F(" RX="));
	Serial.print(Listener2.RxCount);
	Serial.print(F(" connections="));
	Serial.print(Listener2.ConnectedEvents);
	Serial.print(F(" disconnections="));
	Serial.println(Listener2.DisconnectedEvents);
}

void ReportFailure(const __FlashStringHelper* reason)
{
	PrintCounts();
	Serial.print(F("FAIL: "));
	Serial.println(reason);
	TestFinished = true;
	TestPhase = TestPhaseEnum::Finished;
}
