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
#include "pw_status/status.h"
#include "pw_stream/stream.h"

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

  UartDmaStreamMcuxpresso(const Config& config) : config_(config) {}

  pw::Status Init(uint32_t srcclk);

 private:
  StatusWithSize DoRead(ByteSpan) override;
  Status DoWrite(ConstByteSpan) override;

  usart_dma_handle_t uart_dma_handle_;  // USART DMA Handle

  Config const config_;  // USART DMA configuration
};

}  // namespace pw::stream
