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

#include "pw_dma_mcuxpresso/dma.h"
#include "pw_status/try.h"
#include "pw_uart/blocking_adapter.h"
#include "pw_uart_mcuxpresso/dma_uart_nonblocking.h"
#include "pw_unit_test/framework.h"

namespace examples {

pw::Status DmaUartNonBlockingBasicExample() {
  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartNonBlockingBasicExample]
  const auto kUartBase = USART0;
  constexpr uint32_t kBaudRate = 115200;
  constexpr bool kFlowControl = true;
  std::array<std::byte, 65536> ring_buffer = {};
  constexpr uint32_t kUartRxDmaCh = 0;
  constexpr uint32_t kUartTxDmaCh = 1;
  static pw::dma::McuxpressoDmaController dma(DMA0_BASE);
  static pw::dma::McuxpressoDmaChannel rx_dma_ch = dma.GetChannel(kUartRxDmaCh);
  static pw::dma::McuxpressoDmaChannel tx_dma_ch = dma.GetChannel(kUartTxDmaCh);

  PW_TRY(dma.Init());
  rx_dma_ch.Init();
  tx_dma_ch.Init();

  const pw::uart::DmaUartMcuxpressoNonBlocking::Config kConfig = {
      .usart_base = kUartBase,
      .baud_rate = kBaudRate,
      .flow_control = kFlowControl,
      .parity = kUSART_ParityDisabled,
      .stop_bits = kUSART_OneStopBit,
      .rx_dma_ch = rx_dma_ch,
      .tx_dma_ch = tx_dma_ch,
      .rx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0RxToDmac0Ch0RequestEna,
      .tx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0TxToDmac0Ch1RequestEna,
      .buffer = pw::ByteSpan(ring_buffer),
  };

  auto uart = pw::uart::DmaUartMcuxpressoNonBlocking{kConfig};

  PW_TRY(uart.Enable());

  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartNonBlockingBasicExample]

  // Do something else

  return pw::OkStatus();
}

pw::Status DmaUartNonBlockingAdapterExample() {
  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartNonBlockingAdapterExample]
  const auto kUartBase = USART0;
  constexpr uint32_t kBaudRate = 115200;
  constexpr bool kFlowControl = true;
  std::array<std::byte, 65536> ring_buffer = {};
  constexpr uint32_t kUartRxDmaCh = 0;
  constexpr uint32_t kUartTxDmaCh = 1;
  static pw::dma::McuxpressoDmaController dma(DMA0_BASE);
  static pw::dma::McuxpressoDmaChannel rx_dma_ch = dma.GetChannel(kUartRxDmaCh);
  static pw::dma::McuxpressoDmaChannel tx_dma_ch = dma.GetChannel(kUartTxDmaCh);

  PW_TRY(dma.Init());
  rx_dma_ch.Init();
  tx_dma_ch.Init();

  const pw::uart::DmaUartMcuxpressoNonBlocking::Config kConfig = {
      .usart_base = kUartBase,
      .baud_rate = kBaudRate,
      .flow_control = kFlowControl,
      .parity = kUSART_ParityDisabled,
      .stop_bits = kUSART_OneStopBit,
      .rx_dma_ch = rx_dma_ch,
      .tx_dma_ch = tx_dma_ch,
      .rx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0RxToDmac0Ch0RequestEna,
      .tx_input_mux_dmac_ch_request_en =
          kINPUTMUX_Flexcomm0TxToDmac0Ch1RequestEna,
      .buffer = pw::ByteSpan(ring_buffer),
  };

  auto uart = pw::uart::DmaUartMcuxpressoNonBlocking{kConfig};
  auto adapted_uart = pw::uart::UartBlockingAdapter(uart);

  PW_TRY(adapted_uart.Enable());

  // DOCSTAG: [pw_uart_mcuxpresso-DmaUartNonBlockingAdapterExample]

  // Do something else

  return pw::OkStatus();
}

TEST(DmaUartNonBlocking, BasicExample) {
  pw::Status status = DmaUartNonBlockingBasicExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}

TEST(DmaUartNonBlocking, AdapterExample) {
  pw::Status status = DmaUartNonBlockingAdapterExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
