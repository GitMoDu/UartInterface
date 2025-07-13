#ifndef _UART_OUT_TASK_h
#define _UART_OUT_TASK_h

#define _TASK_OO_CALLBACKS
#include <TSchedulerDeclarations.hpp>

#include <UartInterface.h>

namespace UartInterface
{
	namespace UartOut
	{
		/// <summary>
		/// Async stream writer from an external fixed buffer.
		/// Delimits each buffer transmission with the MessageDefinition delimiter.
		/// </summary>
		/// <typeparam name="SerialType"></typeparam>
		/// <typeparam name="MaxSerialStepOut"></typeparam>
		template<typename SerialType,
			uint8_t MaxSerialStepOut,
			uint32_t WriteTimeoutMillis>
		class UartOutTask : public TS::Task
		{
		private:
			enum class StateEnum : uint8_t
			{
				NotSending,
				SendingStartDelimiter,
				SendingData,
				SendingEndDelimiter
			};

			StateEnum SendState = StateEnum::NotSending;

		private:
			using MessageDefinition = UartInterface::MessageDefinition;

		private:
			SerialType& SerialInstance;
			uint8_t* OutBuffer;
			UartListener* Listener;

		private:
			uint32_t OutStart = 0;
			uint8_t OutSize = 0;
			uint8_t OutIndex = 0;

		public:
			UartOutTask(TS::Scheduler& scheduler, SerialType& serialInstance, uint8_t* outBuffer, UartListener* listener)
				: Task(TASK_IMMEDIATE, TASK_FOREVER, &scheduler, false)
				, SerialInstance(serialInstance)
				, OutBuffer(outBuffer)
				, Listener(listener)
			{
			}

			bool Setup() const
			{
				return OutBuffer != nullptr;
			}

			void Clear()
			{
				OutSize = 0;
				SendState = StateEnum::NotSending;
				OutIndex = 0;
				SerialInstance.clearWriteError();
			}

			bool Start()
			{
				if (OutBuffer != nullptr)
				{
					Clear();
					TS::Task::disable();

					return true;
				}
				else
				{
					return false;
				}
			}

			bool CanSend() const
			{
				return SendState == StateEnum::NotSending
#if defined(ARDUINO_ARCH_STM32F4)
					&& (USART_TX_BUF_SIZE - SerialInstance.pending()) >= MessageDefinition::MessageSizeMin
#else
					&& SerialInstance.availableForWrite() >= MessageDefinition::MessageSizeMin
#endif
					;
			}

			bool SendMessage(const uint8_t messageSize)
			{
				if (!CanSend()
					|| messageSize < MessageDefinition::MessageSizeMin)
				{
					return false;
				}

				OutSize = messageSize;
				OutIndex = 0;
				SendState = StateEnum::SendingStartDelimiter;

				OutStart = millis();

				TS::Task::enableDelayed(TASK_IMMEDIATE);

				return true;
			}

			bool Callback() final
			{
				if (!SerialInstance)
				{
					Clear();
					TS::Task::disable();

					return true;
				}

				const bool timedOut = (millis() - OutStart) >= WriteTimeoutMillis;

				switch (SendState)
				{
				case StateEnum::SendingStartDelimiter:
					if (!SerialInstance || timedOut)
					{
						SendState = StateEnum::NotSending;
						if (timedOut && Listener != nullptr)
						{
							Listener->OnUartTxError(UartInterface::TxErrorEnum::StartTimeout);
						}
					}
#if defined(ARDUINO_ARCH_STM32F4)
					else if ((USART_TX_BUF_SIZE - SerialInstance.pending()) > MessageDefinition::MessageSizeMin)
#else
					else if (SerialInstance.availableForWrite() > MessageDefinition::MessageSizeMin)
#endif
					{
						SerialInstance.write((uint8_t)(MessageDefinition::Delimiter));
						SendState = StateEnum::SendingData;
					}
					break;
				case StateEnum::SendingData:
					if (OutIndex < OutSize)
					{
						if (!SerialInstance || timedOut)
						{
							SendState = StateEnum::NotSending;
							if (timedOut && Listener != nullptr)
							{
								Listener->OnUartTxError(UartInterface::TxErrorEnum::DataTimeout);
							}
							break;
						}
						else
						{
							OutIndex += PushOut();
							if (OutIndex >= OutSize)
							{
								SendState = StateEnum::SendingEndDelimiter;
								break;
							}
						}
					}
					else
					{
						SendState = StateEnum::SendingEndDelimiter;
					}
					break;
				case StateEnum::SendingEndDelimiter:
					if (!SerialInstance || timedOut)
					{
						SendState = StateEnum::NotSending;
						if (timedOut && Listener != nullptr)
						{
							Listener->OnUartTxError(UartInterface::TxErrorEnum::EndTimeout);
						}
					}
					else if (SerialInstance
#if defined(ARDUINO_ARCH_STM32F4)
						&& (USART_TX_BUF_SIZE - SerialInstance.pending())
#else
						&& SerialInstance.availableForWrite()
#endif
						)
					{
						SerialInstance.write((uint8_t)(MessageDefinition::Delimiter));
						SendState = StateEnum::NotSending;
						if (Listener != nullptr)
						{
							Listener->OnUartTx();
						}
					}
					break;
				case StateEnum::NotSending:
				default:
					TS::Task::disable();
					break;
				}

				return true;
			}

		private:
			uint8_t PushOut()
			{
				uint8_t size = OutSize - OutIndex;

				if (size > MaxSerialStepOut)
				{
					size = MaxSerialStepOut;
				}

#if defined(ARDUINO_ARCH_STM32F4)
				const uint16_t available = USART_TX_BUF_SIZE - SerialInstance.pending();
#else
				const uint16_t available = SerialInstance.availableForWrite();
#endif
				if (size > available)
				{
					size = available;
				}

				SerialInstance.write(&OutBuffer[OutIndex], size);


				return size;
			}
		};
	}
}
#endif