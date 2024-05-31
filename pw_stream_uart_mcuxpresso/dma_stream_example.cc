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

#include "pw_status/try.h"
#include "pw_stream_uart_mcuxpresso/dma_stream.h"
#include "pw_unit_test/framework.h"

namespace examples {

pw::Status UartDmaStreamExample() {
  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartDmaStreamExample]
  constexpr uint32_t kFlexcomm = 0;
  const auto kUartBase = USART0;
  constexpr uint32_t kBaudRate = 115200;
  std::array<std::byte, 65536> ring_buffer = {};
  constexpr uint32_t kUartRxDmaCh = 0;
  constexpr uint32_t kUartTxDmaCh = 1;

  const pw::stream::UartDmaStreamMcuxpresso::Config kConfig = {
      .usart_base = kUartBase,
      .baud_rate = kBaudRate,
      .parity = kUSART_ParityDisabled,
      .stop_bits = kUSART_OneStopBit,
      .dma_base = DMA0,
      .rx_dma_ch = kUartRxDmaCh,
      .tx_dma_ch = kUartTxDmaCh,
      .rx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0RxToDmac0Ch0RequestEna,
      .tx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0TxToDmac0Ch1RequestEna,
      .buffer = pw::ByteSpan(ring_buffer)};

  auto stream = pw::stream::UartDmaStreamMcuxpresso{kConfig};

  PW_TRY(stream.Init(CLOCK_GetFlexcommClkFreq(kFlexcomm)));

  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartDmaStreamExample]

  // Do something else

  return pw::OkStatus();
}

TEST(DmaStream, Example) {
  pw::Status status = UartDmaStreamExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
