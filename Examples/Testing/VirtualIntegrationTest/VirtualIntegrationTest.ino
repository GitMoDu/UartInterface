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
static constexpr uint32_t SplitPacketDelayMillis = 10;

enum class TestPhaseEnum : uint8_t
{
	PayloadSequence,
	WaitingForDisconnect,
	WaitingForReconnect,
	WaitingForRxError,
	SettlingReceiver,
	SendingSplitRemainder,
	WaitingForRecoveryRx,
	WaitingForTxError,
	SettlingTransmitter,
	Finished
};

enum class FaultCaseEnum : uint8_t
{
	TooShort,
	MalformedCobs,
	BadCrc,
	TruncatedFrame,
	SplitFrame,
	TxStartTimeout,
	TxDataTimeout,
	TxEndTimeout,
	Complete
};

uint32_t TestStartMillis = 0;
uint32_t PhaseStartMillis = 0;
bool TestFinished = false;
TestPhaseEnum TestPhase = TestPhaseEnum::PayloadSequence;
FaultCaseEnum CurrentFaultCase = FaultCaseEnum::TooShort;
FaultCaseEnum NextFaultCase = FaultCaseEnum::TooShort;
uint8_t ProbeFrameSize = 0;
uint8_t ProbeSplitOffset = 0;

void StopSenders();
void PrintCounts();
void ReportFailure(const __FlashStringHelper* reason);
void BeginFaultCase(const FaultCaseEnum faultCase);
void BeginRxErrorTest(const UartInterface::RxErrorEnum expectedError,
	const uint8_t* frame, const uint8_t frameSize, const FaultCaseEnum nextFaultCase);
void BeginTxTimeoutTest(const UartInterface::TxErrorEnum expectedError,
	const FaultCaseEnum nextFaultCase);
uint8_t BuildProbeFrame(const bool corrupt);
bool AllTestsPassed();


// UART definitions for the test.
namespace Definitions
{
	static constexpr uint8_t Key[]{ 1, 2, 3, 4, 5, 6, 7, 8 };
	static constexpr uint8_t KeySize = sizeof(Key);

	using UartDefinitions = UartInterface::TemplateUartDefinitions<>;

	using VirtualUartType = VirtualUart::UartSerial<>;
	using UartInterfaceTaskType = UartInterface::UartInterfaceTask<VirtualUartType, UartDefinitions>;
}

static constexpr uint8_t ProbeHeader = 0xD2;
static constexpr uint8_t ProbePayloadSize = 2;
static constexpr uint8_t ProbePayload[]{ 0xA5, 0x5A };
static constexpr uint8_t TimeoutTestHeader = 0xB7;
static constexpr uint8_t MalformedShortFrame[]{ 0, 0x01, 0 };
static constexpr uint8_t MalformedCobsFrame[]{ 0, 0xFF, 0x11, 0x22, 0x33, 0 };
static constexpr uint8_t TruncatedFrame[]{ 0, 0x03, 0x11, 0x22 };
static constexpr uint32_t ReceiveSettleMillis = Definitions::UartDefinitions::ReadTimeoutMillis + 10;
static constexpr uint32_t FaultPhaseTimeoutMillis = 2000;

using FaultCodecType = UartInterface::MessageCodec<ProbePayloadSize>;
FaultCodecType FaultCodec(Definitions::Key, Definitions::KeySize);
uint8_t ProbeFrame[FaultCodecType::BufferSize + 2]{};

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
		!Interface2.Setup() ||
		!FaultCodec.Setup())
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
		Serial.println(F("Lifecycle callbacks passed; starting fault-injection cases"));
		BeginFaultCase(FaultCaseEnum::TooShort);
	}
	else if (TestPhase == TestPhaseEnum::WaitingForReconnect
		&& millis() - PhaseStartMillis >= LifecyclePhaseTimeoutMillis)
	{
		ReportFailure(F("timeout waiting for reconnect callbacks"));
	}
	else if (TestPhase == TestPhaseEnum::WaitingForRxError)
	{
		if (Listener2.HasExpectedRxError())
		{
			Listener2.ClearExpectedRxError();
			TestPhase = TestPhaseEnum::SettlingReceiver;
			PhaseStartMillis = millis();
		}
		else if (millis() - PhaseStartMillis >= FaultPhaseTimeoutMillis)
		{
			ReportFailure(F("timeout waiting for expected RX error"));
		}
	}
	else if (TestPhase == TestPhaseEnum::SettlingReceiver
		&& millis() - PhaseStartMillis >= ReceiveSettleMillis)
	{
		BeginFaultCase(NextFaultCase);
	}
	else if (TestPhase == TestPhaseEnum::SendingSplitRemainder
		&& millis() - PhaseStartMillis >= SplitPacketDelayMillis)
	{
		Uart2.ReceiveBytes(&ProbeFrame[ProbeSplitOffset], ProbeFrameSize - ProbeSplitOffset);
		TestPhase = TestPhaseEnum::WaitingForRecoveryRx;
		PhaseStartMillis = millis();
	}
	else if (TestPhase == TestPhaseEnum::WaitingForRecoveryRx)
	{
		if (Listener2.HasAdditionalRx())
		{
			BeginFaultCase(FaultCaseEnum::TxStartTimeout);
		}
		else if (millis() - PhaseStartMillis >= FaultPhaseTimeoutMillis)
		{
			ReportFailure(F("split-frame recovery packet was not received"));
		}
	}
	else if (TestPhase == TestPhaseEnum::WaitingForTxError)
	{
		const bool txErrorReceived = Listener1.HasExpectedTxError();
		const bool rxErrorReceived = CurrentFaultCase != FaultCaseEnum::TxEndTimeout
			|| Listener2.HasExpectedRxError();
		if (txErrorReceived && rxErrorReceived)
		{
			Listener1.ClearExpectedTxError();
			if (CurrentFaultCase == FaultCaseEnum::TxEndTimeout)
				Listener2.ClearExpectedRxError();
			Uart1.BlockWriteAvailability = false;
			TestPhase = TestPhaseEnum::SettlingTransmitter;
			PhaseStartMillis = millis();
		}
		else if (millis() - PhaseStartMillis >= FaultPhaseTimeoutMillis)
		{
			ReportFailure(F("timeout waiting for expected TX/RX timeout callbacks"));
		}
	}
	else if (TestPhase == TestPhaseEnum::SettlingTransmitter
		&& millis() - PhaseStartMillis >= ReceiveSettleMillis)
	{
		BeginFaultCase(NextFaultCase);
	}
}

void BeginFaultCase(const FaultCaseEnum faultCase)
{
	CurrentFaultCase = faultCase;
	switch (faultCase)
	{
	case FaultCaseEnum::TooShort:
		Serial.println(F("RX fault case: too-short frame"));
		NextFaultCase = FaultCaseEnum::MalformedCobs;
		BeginRxErrorTest(UartInterface::RxErrorEnum::TooShort, MalformedShortFrame,
			sizeof(MalformedShortFrame), NextFaultCase);
		break;
	case FaultCaseEnum::MalformedCobs:
		Serial.println(F("RX fault case: malformed COBS"));
		NextFaultCase = FaultCaseEnum::BadCrc;
		BeginRxErrorTest(UartInterface::RxErrorEnum::Crc, MalformedCobsFrame,
			sizeof(MalformedCobsFrame), NextFaultCase);
		break;
	case FaultCaseEnum::BadCrc:
		Serial.println(F("RX fault case: corrupted CRC"));
		NextFaultCase = FaultCaseEnum::TruncatedFrame;
		ProbeFrameSize = BuildProbeFrame(true);
		if (ProbeFrameSize == 0)
		{
			ReportFailure(F("could not construct CRC fault frame"));
			break;
		}
		BeginRxErrorTest(UartInterface::RxErrorEnum::Crc, ProbeFrame, ProbeFrameSize, NextFaultCase);
		break;
	case FaultCaseEnum::TruncatedFrame:
		Serial.println(F("RX fault case: truncated frame timeout"));
		NextFaultCase = FaultCaseEnum::SplitFrame;
		BeginRxErrorTest(UartInterface::RxErrorEnum::EndTimeout, TruncatedFrame,
			sizeof(TruncatedFrame), NextFaultCase);
		break;
	case FaultCaseEnum::SplitFrame:
		ProbeFrameSize = BuildProbeFrame(false);
		if (ProbeFrameSize == 0)
		{
			ReportFailure(F("could not construct split-frame probe"));
			break;
		}
		Listener2.ExpectAdditionalRx(ProbeHeader, ProbePayload, ProbePayloadSize);
		ProbeSplitOffset = ProbeFrameSize / 2;
		Uart2.ReceiveBytes(ProbeFrame, ProbeSplitOffset);
		TestPhase = TestPhaseEnum::SendingSplitRemainder;
		PhaseStartMillis = millis();
		Serial.println(F("Injected first half of valid frame"));
		break;
	case FaultCaseEnum::TxStartTimeout:
		Serial.println(F("TX fault case: start timeout"));
		NextFaultCase = FaultCaseEnum::TxDataTimeout;
		BeginTxTimeoutTest(UartInterface::TxErrorEnum::StartTimeout, NextFaultCase);
		break;
	case FaultCaseEnum::TxDataTimeout:
		Serial.println(F("TX fault case: data timeout"));
		NextFaultCase = FaultCaseEnum::TxEndTimeout;
		BeginTxTimeoutTest(UartInterface::TxErrorEnum::DataTimeout, NextFaultCase);
		break;
	case FaultCaseEnum::TxEndTimeout:
		Serial.println(F("TX fault case: end timeout"));
		NextFaultCase = FaultCaseEnum::Complete;
		BeginTxTimeoutTest(UartInterface::TxErrorEnum::EndTimeout, NextFaultCase);
		break;
	case FaultCaseEnum::Complete:
		if (AllTestsPassed())
		{
			PrintCounts();
			Serial.println(F("All tests passed"));
			TestFinished = true;
			TestPhase = TestPhaseEnum::Finished;
		}
		else
		{
			ReportFailure(F("final fault-test counters did not match expectations"));
		}
		break;
	}
}

void BeginRxErrorTest(const UartInterface::RxErrorEnum expectedError,
	const uint8_t* frame, const uint8_t frameSize, const FaultCaseEnum nextFaultCase)
{
	NextFaultCase = nextFaultCase;
	Listener2.ExpectRxError(expectedError);
	Uart2.ReceiveBytes(frame, frameSize);
	TestPhase = TestPhaseEnum::WaitingForRxError;
	PhaseStartMillis = millis();
}

void BeginTxTimeoutTest(const UartInterface::TxErrorEnum expectedError,
	const FaultCaseEnum nextFaultCase)
{
	NextFaultCase = nextFaultCase;
	Listener1.ExpectTxError(expectedError);
	Uart1.BlockWriteAvailability = false;
	Uart1.BlockAfterStartDelimiter = false;
	Uart1.BlockAfterData = false;

	if (expectedError == UartInterface::TxErrorEnum::EndTimeout)
		Listener2.ExpectRxError(UartInterface::RxErrorEnum::EndTimeout);
	else if (expectedError == UartInterface::TxErrorEnum::DataTimeout)
		Uart1.BlockAfterStartDelimiter = true;

	if (!Interface1.SendMessage(TimeoutTestHeader))
	{
		Uart1.BlockAfterStartDelimiter = false;
		Uart1.BlockAfterData = false;
		ReportFailure(F("could not queue synthetic TX-timeout frame"));
		return;
	}

	if (expectedError == UartInterface::TxErrorEnum::StartTimeout)
		Uart1.BlockWriteAvailability = true;
	else if (expectedError == UartInterface::TxErrorEnum::EndTimeout)
		Uart1.BlockAfterData = true;

	TestPhase = TestPhaseEnum::WaitingForTxError;
	PhaseStartMillis = millis();
	Serial.println(F("Started synthetic TX-timeout case"));
}

uint8_t BuildProbeFrame(const bool corrupt)
{
	uint8_t encoded[FaultCodecType::BufferSize]{};
	const uint8_t headerOffset = static_cast<uint8_t>(UartInterface::MessageDefinition::FieldIndexEnum::Header);
	const uint8_t payloadOffset = static_cast<uint8_t>(UartInterface::MessageDefinition::FieldIndexEnum::Payload);
	encoded[headerOffset] = ProbeHeader;
	for (uint8_t i = 0; i < ProbePayloadSize; i++)
		encoded[payloadOffset + i] = ProbePayload[i];

	const uint16_t encodedSize = FaultCodec.EncodeMessageAndCrcInPlace(encoded,
		UartInterface::MessageDefinition::GetMessageSize(ProbePayloadSize));
	if (encodedSize == 0 || encodedSize + 2 > sizeof(ProbeFrame))
		return 0;
	if (corrupt)
		encoded[encodedSize - 1] ^= 1;

	ProbeFrame[0] = UartInterface::MessageDefinition::Delimiter;
	for (uint16_t i = 0; i < encodedSize; i++)
		ProbeFrame[i + 1] = encoded[i];
	ProbeFrame[encodedSize + 1] = UartInterface::MessageDefinition::Delimiter;
	return static_cast<uint8_t>(encodedSize + 2);
}

bool AllTestsPassed()
{
	return !Listener1.Failed && !Listener2.Failed
		&& Sender1.Complete && Sender2.Complete
		&& Listener1.RxCount == TestListener::ExpectedMessageCount
		&& Listener2.RxCount == TestListener::ExpectedMessageCount
		&& Listener1.TxCount == TestListener::ExpectedMessageCount
		&& Listener2.TxCount == TestListener::ExpectedMessageCount
		&& Listener1.ConnectedEvents == 2 && Listener1.DisconnectedEvents == 1
		&& Listener2.ConnectedEvents == 2 && Listener2.DisconnectedEvents == 1
		&& Listener1.RxErrorCount == 0 && Listener2.RxErrorCount == 5
		&& Listener2.GetRxErrorCount(UartInterface::RxErrorEnum::TooShort) == 1
		&& Listener2.GetRxErrorCount(UartInterface::RxErrorEnum::Crc) == 2
		&& Listener2.GetRxErrorCount(UartInterface::RxErrorEnum::EndTimeout) == 2
		&& Listener1.TxErrorCount == 3 && Listener2.TxErrorCount == 0
		&& Listener1.GetTxErrorCount(UartInterface::TxErrorEnum::StartTimeout) == 1
		&& Listener1.GetTxErrorCount(UartInterface::TxErrorEnum::DataTimeout) == 1
		&& Listener1.GetTxErrorCount(UartInterface::TxErrorEnum::EndTimeout) == 1
		&& Listener2.HasAdditionalRx()
		&& Interface1.IsSerialConnected() && Interface2.IsSerialConnected()
		&& !Uart1.BlockWriteAvailability && !Uart1.BlockAfterStartDelimiter && !Uart1.BlockAfterData;
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
	Serial.print(Listener1.DisconnectedEvents);
	Serial.print(F(" RX errors="));
	Serial.print(Listener1.RxErrorCount);
	Serial.print(F(" TX errors="));
	Serial.println(Listener1.TxErrorCount);
	Serial.print(F("Endpoint 2: TX="));
	Serial.print(Listener2.TxCount);
	Serial.print(F(" RX="));
	Serial.print(Listener2.RxCount);
	Serial.print(F(" connections="));
	Serial.print(Listener2.ConnectedEvents);
	Serial.print(F(" disconnections="));
	Serial.print(Listener2.DisconnectedEvents);
	Serial.print(F(" RX errors="));
	Serial.print(Listener2.RxErrorCount);
	Serial.print(F(" TX errors="));
	Serial.println(Listener2.TxErrorCount);
}

void ReportFailure(const __FlashStringHelper* reason)
{
	PrintCounts();
	Serial.print(F("FAIL: "));
	Serial.println(reason);
	TestFinished = true;
	TestPhase = TestPhaseEnum::Finished;
}
