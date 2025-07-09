// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include "fsl_usart.h"
#include "pw_bytes/span.h"
#include "pw_clock_tree/clock_tree.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_uart/uart_non_blocking.h"

namespace pw::uart {

// A non-dma implementation of non blocking uart interface for USART peripheral.
class UartMcuxpressoNonBlocking final : public UartNonBlocking {
 public:
  // Configuration structure
  struct Config {
    USART_Type* usart_base;     // Base of USART control struct
    uint32_t baud_rate;         // Desired communication speed
    bool flow_control = false;  // Hardware flow control setting
    usart_parity_mode_t parity = kUSART_ParityDisabled;  // Parity setting
    usart_stop_bit_count_t stop_bits =
        kUSART_OneStopBit;                    // Number of stop bits to use
    ByteSpan buffer;                          // Receive ring buffer
    pw::clock_tree::ClockTree* clock_tree{};  // Optional clock Tree
    pw::clock_tree::Element*
        clock_tree_element{};  // Optional clock tree element
  };

  UartMcuxpressoNonBlocking(const Config& config)
      : config_(config),
        clock_tree_element_controller_(config.clock_tree,
                                       config.clock_tree_element) {}

  ~UartMcuxpressoNonBlocking();
  UartMcuxpressoNonBlocking(const UartMcuxpressoNonBlocking& other) = delete;
  UartMcuxpressoNonBlocking& operator=(const UartMcuxpressoNonBlocking& other) =
      delete;

 private:
  static size_t constexpr kUsartMaxTransferCount = 512;

  // Usart TX data structure
  struct UsartTxData {
    usart_transfer_t transfer{};  // USART TX transfer structure
    struct {
      bool valid{};          // A request is in-flight
      ConstByteSpan buffer;  // TX transaction buffer
      Function<void(StatusWithSize status)> callback;
    } request{};

    struct {
      bool valid{};
      Function<void(Status status)> callback;
    } flush_request{};
  };

  // Usart RX data structure
  struct UsartRxData {
    usart_transfer_t transfer{};  // USART RX transfer structure

    // User read request data
    struct {
      bool valid{};     // A request is in-flight
      ByteSpan buffer;  // User destination buffer
      Function<void(Status status, ConstByteSpan buffer)> callback;
      size_t size{};  // Target total number of read bytes
    } request{};
  };

  Status DoEnable(bool enable) override PW_LOCKS_EXCLUDED(interrupt_lock_);
  Status DoSetBaudRate(uint32_t baud_rate) override;
  Status DoSetFlowControl(bool enable) override;

  Status DoRead(ByteSpan rx_buffer,
                size_t min_bytes,
                Function<void(Status status, ConstByteSpan buffer)>&& callback)
      override PW_LOCKS_EXCLUDED(interrupt_lock_);
  bool DoCancelRead() override PW_LOCKS_EXCLUDED(interrupt_lock_);

  Status DoWrite(ConstByteSpan tx_buffer,
                 Function<void(StatusWithSize status)>&& callback) override
      PW_LOCKS_EXCLUDED(interrupt_lock_);
  bool DoCancelWrite() override PW_LOCKS_EXCLUDED(interrupt_lock_);
  Status DoFlushOutput(Function<void(Status status)>&& callback) override;
  bool DoCancelFlushOutput() override PW_LOCKS_EXCLUDED(interrupt_lock_);
  size_t DoConservativeReadAvailable() override
      PW_LOCKS_EXCLUDED(interrupt_lock_);
  Status DoClearPendingReceiveBytes() override
      PW_LOCKS_EXCLUDED(interrupt_lock_);

  // Helper functions

  void Deinit() PW_LOCKS_EXCLUDED(interrupt_lock_);
  Status Init() PW_LOCKS_EXCLUDED(interrupt_lock_);

  bool DoCancelReadLockHeld() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  bool DoCancelWriteLockHeld() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  bool DoCancelFlushOutputLockHeld()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  Status DoClearPendingReceiveBytesLockHeld()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  void HandleCompletedRx() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  bool CompleteFlushRequest(Status status)
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  void TriggerRead() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  void TriggerWrite() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  void TxRxCompletionCallback(status_t status);

  static void UsartCallback(USART_Type* base,
                            usart_handle_t* state,
                            status_t status,
                            void* param);

  pw::sync::InterruptSpinLock
      interrupt_lock_;  // Lock to synchronize with interrupt handler and to
                        // guarantee exclusive access to control registers
  usart_handle_t uart_handle_;  // USART Handle
  struct UsartTxData tx_data_ PW_GUARDED_BY(interrupt_lock_);
  struct UsartRxData rx_data_ PW_GUARDED_BY(interrupt_lock_);

  Config config_;  // USART configuration
  pw::clock_tree::ElementController
      clock_tree_element_controller_;  // Element controller encapsulating
                                       // optional clock tree information
  bool initialized_;                   // Whether the USART has been initialized
  uint32_t flexcomm_clock_freq_{};
};

}  // namespace pw::uart
