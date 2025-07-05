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
TestSenderTask<Definitions::UartInterfaceTaskType> Sender1(SchedulerBase, Interface1, 1);
TestSenderTask<Definitions::UartInterfaceTaskType> Sender2(SchedulerBase, Interface2, 2);

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
		Serial.println(F("Setup Failed!"));
		while (true)
			;;
	}

	// Setup virtual UART cross-wire connections.
	Uart1.Receiver = &Uart2;
	Uart2.Receiver = &Uart1;

	// Start both endpoints.
	Interface1.Start();
	Interface2.Start();

	// Start sending test messages from both ends.
	Sender1.enableDelayed(1000);
	Sender2.enableDelayed(1500);
}

void loop()
{
	SchedulerBase.execute();
}