# UartInterface

Framed UART messaging for Arduino-style platforms using COBS framing and a keyed Fletcher16 CRC. Provides non-blocking TX/RX tasks, backpressure handling, and compile-time configuration.

## Key features
- Robust framed messaging over a byte stream (e.g., HardwareSerial)
- COBS framing with 0x00 as frame delimiter (payloads may contain 0x00)
- Keyed Fletcher16 CRC for message integrity
- Event-driven, non-blocking TX/RX tasks (TS::Task-based)
- Fixed-size internal buffers (no dynamic allocation)
- Compile-time configuration via templates

### Framing and integrity
- COBS encode/decode to allow 0x00 as a reliable frame delimiter
- Message layout (pre-COBS): CRC (2 bytes, little-endian) | Header (1 byte) | Payload (N bytes)
- Keyed CRC: Fletcher16 seeded with a user-provided key (KeyedCrc)
- Validity checks performed in MessageCodec before delivery

### Transport & timing behavior
- TS::Task-driven state machines for RX and TX
- TX implements start/data/end delimiters, write timeouts, and chunked writes to avoid blocking
- RX implements accumulation with read timeouts, too-short/too-long detection, CRC error reporting, and delivery callbacks

### Backpressure and throughput
- Configurable max serial write/read step sizes to limit per-cycle writes/reads
- Non-blocking SendMessage that uses an async UartOutTask to push bytes to the serial interface

### Memory & safety
- Fixed-size buffers sized at compile-time (via MessageDefinition and template parameters)
- No heap allocations — suitable for constrained MCU environments

### Configuration and extensibility
- TemplateUartDefinitions allows compile-time configuration: baud rate, max payload, step sizes, timeouts, poll period
- MessageCodec templated by payload/buffer sizes for flexibility
- UartListener interface exposes connection, RX/TX notifications, and errors for application logic

## Dependencies
- Arduino core (Serial-like API, millis)
- Fletcher16 (Rob Tillaart) — <Fletcher16.h>  
  https://github.com/RobTillaart/Fletcher
- TaskScheduler (arkhipenko) — <TSchedulerDeclarations.hpp>  
  https://github.com/arkhipenko/TaskScheduler

## Advanced configuration

You can customize baud rate, max payload, step sizes, and timeouts via `TemplateUartDefinitions` and pass it as the second template parameter to `UartInterfaceTask`.

```cpp
// baudrate, maxPayloadSize, maxSerialStepOut, maxSerialStepIn, writeTimeoutMs, readTimeoutMs, pollPeriodMs
using MyDefs = UartInterface::TemplateUartDefinitions<230400, 64, 32, 32, 50, 50, 1>;

UartInterface::UartInterfaceTask<HardwareSerial, MyDefs> uiTask(
  scheduler, Serial1, &listener, KEY, sizeof(KEY)
);
```

## Protocol details

![Message layout and on-wire framing](https://github.com/GitMoDu/UartInterface/blob/master/Media/message_layout.svg)

- On-wire framing:
  - Each message is sent as: [0x00] + COBS-encoded message bytes + [0x00]
  - `MessageDefinition::Delimiter` is 0x00
- Message layout before COBS encoding (indexes within the raw message buffer):
  - Byte 0: CRC LSB (`MessageDefinition::FieldIndexEnum::Crc0`)
  - Byte 1: CRC MSB (`MessageDefinition::FieldIndexEnum::Crc1`) — little-endian storage
  - Byte 2: Header (`MessageDefinition::FieldIndexEnum::Header`)
  - Byte 3..N: Payload (`MessageDefinition::FieldIndexEnum::Payload`)
- CRC:
  - Fletcher16 over [Header + Payload], seeded with the user-provided key (KeyedCrc)
- Sizes:
  - Minimum message size is 3 bytes (CRC[2] + Header[1])
  - Absolute maximum payload is `MessageDefinition::PayloadSizeMax`
  - The effective payload limit at runtime is `UartDefinitions::MaxPayloadSize`
  - Encoded buffer sizes follow `UartCobsCodec::GetBufferSize(...)`

## Public API (overview)

Headers:
- `<UartInterface.h>`: Codec and model types
  - `UartInterface::MessageDefinition`
  - `UartInterface::TemplateUartDefinitions<>`
  - `UartInterface::TxErrorEnum`, `UartInterface::RxErrorEnum`
  - `UartInterface::UartListener` (pure virtual)
- `<UartInterfaceTask.h>`: Tasks and high-level interface
  - `UartInterface::UartInterfaceTask<SerialType, UartDefinitions>`
    - `bool Setup()`
    - `void Start()`
    - `void Stop()`
    - `bool CanSendMessage() const`
    - `bool SendMessage(uint8_t header)`
    - `bool SendMessage(uint8_t header, const uint8_t* payload, uint8_t payloadSize)`
    - `bool IsSerialConnected()`
    - `void OnSerialEvent()` (optional acceleration hook)
- Codec (available if you need lower-level access):
  - `UartInterface::MessageCodec<PayloadSizeMax>`
    - `bool Setup()`
    - `uint16_t EncodeMessageAndCrcInPlace(uint8_t* message, uint16_t messageSize)`
    - `bool DecodeMessageInPlaceIfValid(uint8_t* buffer, uint16_t bufferSize)`
    - `bool MessageValid(uint8_t* message, uint16_t messageSize)`

## Error handling

- RX errors (`UartInterface::RxErrorEnum`): `StartTimeout`, `Crc`, `TooShort`, `TooLong`, `EndTimeout`
- TX errors (`UartInterface::TxErrorEnum`): `StartTimeout`, `DataTimeout`, `EndTimeout`

These are reported through `UartListener` callbacks.
