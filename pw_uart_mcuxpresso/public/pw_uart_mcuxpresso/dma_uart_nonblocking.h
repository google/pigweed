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

#include "fsl_dma.h"
#include "fsl_inputmux.h"
#include "fsl_usart_dma.h"
#include "pw_bytes/span.h"
#include "pw_clock_tree/clock_tree.h"
#include "pw_dma_mcuxpresso/dma.h"
#include "pw_status/status.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_uart/uart_non_blocking.h"

namespace pw::uart {

class DmaUartMcuxpressoNonBlocking final : public UartNonBlocking {
 public:
  // Configuration structure
  struct Config {
    USART_Type* usart_base;     // Base of USART control struct
    uint32_t baud_rate;         // Desired communication speed
    bool flow_control = false;  // Hardware flow control setting
    usart_parity_mode_t parity = kUSART_ParityDisabled;  // Parity setting
    usart_stop_bit_count_t stop_bits =
        kUSART_OneStopBit;                 // Number of stop bits to use
    dma::McuxpressoDmaChannel& rx_dma_ch;  // Receive DMA channel
    dma::McuxpressoDmaChannel& tx_dma_ch;  // Transmit DMA channel
    inputmux_signal_t rx_input_mux_dmac_ch_request_en;  // Rx input mux signal
    inputmux_signal_t tx_input_mux_dmac_ch_request_en;  // Tx input mux signal
    ByteSpan buffer;                                    // Receive ring buffer
    pw::clock_tree::ClockTree* clock_tree{};            // Optional clock Tree
    pw::clock_tree::Element*
        clock_tree_element{};  // Optional clock tree element
  };

  DmaUartMcuxpressoNonBlocking(const Config& config)
      : rx_data_{.ring_buffer = config.buffer},
        config_(config),
        clock_tree_element_controller_(config.clock_tree,
                                       config.clock_tree_element) {}

  ~DmaUartMcuxpressoNonBlocking();

  DmaUartMcuxpressoNonBlocking(const DmaUartMcuxpressoNonBlocking& other) =
      delete;

  DmaUartMcuxpressoNonBlocking& operator=(
      const DmaUartMcuxpressoNonBlocking& other) = delete;

 private:
  // Usart DMA TX data structure
  struct UsartDmaTxData {
    ConstByteSpan buffer;       // TX transaction buffer
    size_t tx_idx;              // Position within TX transaction
    usart_transfer_t transfer;  // USART TX transfer structure
    struct {
      bool valid{};  // A request is in-flight
      Function<void(StatusWithSize status)> callback;
    } request{};
    struct {
      bool valid{};
      Function<void(Status status)> callback;
    } flush_request{};
  };

  enum class DmaRxTarget {
    kRingBuffer = 0,
    kUserBuffer = 1,
  };

  // Usart DMA RX data structure
  struct UsartDmaRxData {
    ByteSpan ring_buffer;  // Receive ring buffer
    DmaRxTarget target{};
    size_t ring_buffer_read_idx{};   // Ring buffer reader index
    size_t ring_buffer_write_idx{};  // Ring buffer writer index
    size_t data_received{};       // Increments when data enters the ring buffer
    size_t data_copied{};         // Increments when data exits the ring buffer
    bool data_loss{};             // Set when the ring buffer overflows
    usart_transfer_t transfer{};  // USART RX transfer structure

    // User read request data
    struct {
      bool valid{};     // A request is in-flight
      ByteSpan buffer;  // User destination buffer
      Function<void(Status status, ConstByteSpan buffer)> callback;
      size_t write_idx{};        // Writer index into the user buffer
      size_t bytes_requested{};  // Target total number of read bytes
      size_t bytes_remaining{};  // Number of bytes needed
    } request{};
  };

  // Since we are calling USART_TransferGetReceiveCountDMA we may only
  // transfer DMA_MAX_TRANSFER_COUNT - 1 bytes per DMA transfer.
  static constexpr size_t kUsartDmaMaxTransferCount =
      DMA_MAX_TRANSFER_COUNT - 1;

  Status DoEnable(bool enable) override;
  Status DoSetBaudRate(uint32_t baud_rate) override;
  Status DoSetFlowControl(bool enable) override;

  Status DoRead(
      ByteSpan rx_buffer,
      size_t min_bytes,
      Function<void(Status status, ConstByteSpan buffer)>&& callback) override;
  bool DoCancelRead() override;

  Status DoWrite(ConstByteSpan tx_buffer,
                 Function<void(StatusWithSize status)>&& callback) override;
  bool DoCancelWrite() override;
  Status DoFlushOutput(Function<void(Status status)>&& callback) override;
  bool DoCancelFlushOutput() override;
  size_t DoConservativeReadAvailable() override;
  Status DoClearPendingReceiveBytes() override;

  // Helper functions

  void Deinit();
  Status Init();

  void HandleCompletedRxIntoRingBuffer()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  void HandleCompletedRxIntoUserBuffer()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  bool ClearRxDmaInterrupt() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  bool CompleteFlushRequest(Status status)
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  void TriggerReadDmaIntoRingBuffer()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  void TriggerReadDmaIntoUserBuffer()
      PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);
  void TriggerWriteDma() PW_EXCLUSIVE_LOCKS_REQUIRED(interrupt_lock_);

  void TxRxCompletionCallback(status_t status);

  static void DmaCallback(USART_Type* base,
                          usart_dma_handle_t* handle,
                          status_t dma_status,
                          void* userdata);

  pw::sync::InterruptSpinLock
      interrupt_lock_;  // Lock to synchronize with interrupt handler and to
                        // guarantee exclusive access to DMA control registers
  usart_dma_handle_t uart_dma_handle_;  // USART DMA Handle
  struct UsartDmaTxData tx_data_ PW_GUARDED_BY(interrupt_lock_);
  struct UsartDmaRxData rx_data_ PW_GUARDED_BY(interrupt_lock_);

  Config config_;  // USART DMA configuration
  pw::clock_tree::ElementController
      clock_tree_element_controller_;  // Element controller encapsulating
                                       // optional clock tree information
  bool
      initialized_;  // Whether the USART and DMA channels have been initialized
  uint32_t flexcomm_clock_freq_{};
};

}  // namespace pw::uart
