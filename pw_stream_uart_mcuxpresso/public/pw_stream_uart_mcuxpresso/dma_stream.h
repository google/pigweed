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

#include <atomic>

#include "fsl_dma.h"
#include "fsl_inputmux.h"
#include "fsl_usart_dma.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/thread_notification.h"

namespace pw::stream {

class UartDmaStreamMcuxpresso final : public NonSeekableReaderWriter {
 public:
  // Configuration structure
  struct Config {
    USART_Type* usart_base;            // Base of USART control struct
    uint32_t baud_rate;                // Desired communication speed
    usart_parity_mode_t parity;        // Parity setting
    usart_stop_bit_count_t stop_bits;  // Number of stop bits to use
    DMA_Type* dma_base;                // Base of DMA control struct
    uint32_t rx_dma_ch;                // Receive DMA channel
    uint32_t tx_dma_ch;                // Transmit DMA channel
    inputmux_signal_t rx_input_mux_dmac_ch_request_en;  // Rx input mux signal
    inputmux_signal_t tx_input_mux_dmac_ch_request_en;  // Tx input mux signal
    ByteSpan buffer;                                    // Receive ring buffer
  };

  UartDmaStreamMcuxpresso(const Config& config)
      : config_(config), rx_data_{.ring_buffer = config.buffer} {}

  ~UartDmaStreamMcuxpresso();

  UartDmaStreamMcuxpresso(const UartDmaStreamMcuxpresso& other) = delete;

  UartDmaStreamMcuxpresso& operator=(const UartDmaStreamMcuxpresso& other) =
      delete;

  pw::Status Init(uint32_t srcclk);

 private:
  // Usart DMA TX data structure
  struct UsartDmaTxData {
    ConstByteSpan buffer;       // TX transaction buffer
    size_t tx_idx;              // Position within TX transaction
    dma_handle_t dma_handle;    // DMA handle
    usart_transfer_t transfer;  // USART TX transfer structure
    std::atomic_uint8_t busy;   // Flag to prevent concurrent access to TX queue
    pw::sync::ThreadNotification notification;  // TX completion notification
  };

  // Usart DMA RX data structure
  struct UsartDmaRxData {
    ByteSpan ring_buffer;          // Receive ring buffer
    size_t ring_buffer_read_idx;   // ring buffer reader index
    size_t ring_buffer_write_idx;  // ring buffer writer index
    size_t
        data_received;  // data received and acknowledged by completion callback
    size_t data_copied;  // data copied out to receiver
    // completion callback will be executed when completion size decreases to 0
    // bytes
    size_t completion_size;
    usart_transfer_t transfer;  // USART RX transfer structure
    dma_handle_t dma_handle;    // DMA handle
    std::atomic_uint8_t
        busy;  // Flag to prevent concurrent access to RX ring buffer
    pw::sync::ThreadNotification notification;  // RX completion notification
  };

  StatusWithSize DoRead(ByteSpan) override;
  Status DoWrite(ConstByteSpan) override;

  // Helper functions
  static IRQn_Type GetInterrupt(const DMA_Type* base);
  void Deinit();
  static void TxRxCompletionCallback(USART_Type* base,
                                     usart_dma_handle_t* state,
                                     status_t status,
                                     void* param);

  pw::sync::InterruptSpinLock
      interrupt_lock_;  // Lock to synchronize with interrupt handler and to
                        // guarantee exclusive access to DMA control registers
  usart_dma_handle_t uart_dma_handle_;  // USART DMA Handle
  struct UsartDmaTxData tx_data_;       // TX data
  struct UsartDmaRxData rx_data_;       // RX data

  Config const config_;  // USART DMA configuration
  bool
      initialized_;  // Whether the USART and DMA channels have been initialized
};

}  // namespace pw::stream
