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
#include "pw_uart_mcuxpresso/dma_uart.h"
#include "pw_unit_test/framework.h"

namespace examples {

pw::Status DmaUartExample() {
  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartExample]
  constexpr uint32_t kFlexcomm = 0;
  const auto kUartBase = USART0;
  constexpr uint32_t kBaudRate = 115200;
  constexpr bool kFlowControl = true;
  constexpr uint32_t kUartInstance = 5;
  std::array<std::byte, 65536> ring_buffer = {};
  constexpr uint32_t kUartRxDmaCh = 0;
  constexpr uint32_t kUartTxDmaCh = 1;

  uint32_t flexcomm_clock_freq = CLOCK_GetFlexcommClkFreq(kUartInstance);

  const pw::uart::DmaUartMcuxpresso::Config kConfig = {
      .usart_base = kUartBase,
      .baud_rate = kBaudRate,
      .flow_control = kFlowControl,
      .parity = kUSART_ParityDisabled,
      .stop_bits = kUSART_OneStopBit,
      .dma_base = DMA0,
      .rx_dma_ch = kUartRxDmaCh,
      .tx_dma_ch = kUartTxDmaCh,
      .rx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0RxToDmac0Ch0RequestEna,
      .tx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0TxToDmac0Ch1RequestEna,
      .buffer = pw::ByteSpan(ring_buffer),
      .srcclk = flexcomm_clock_freq,
  };

  auto uart = pw::uart::DmaUartMcuxpresso{kConfig};

  PW_TRY(uart.Enable());

  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartExample]

  // Do something else

  return pw::OkStatus();
}

TEST(DmaUart, Example) {
  pw::Status status = DmaUartExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
