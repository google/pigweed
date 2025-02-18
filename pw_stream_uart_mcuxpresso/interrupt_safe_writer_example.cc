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
#include "pw_stream_uart_mcuxpresso/interrupt_safe_writer.h"
#include "pw_unit_test/framework.h"

namespace examples {

pw::Status UartInterruptSafeWriterExample() {
  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartInterruptSafeWriterExample]
  constexpr uint32_t kBaudRate = 115200;
  static pw::stream::InterruptSafeUartWriterMcuxpresso crash_safe_uart(
      USART0_BASE, kCLOCK_Flexcomm0Clk, kBaudRate);

  PW_TRY(crash_safe_uart.Enable());

  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartInterruptSafeWriterExample]

  // Do something else

  return pw::OkStatus();
}

TEST(InterruptSafeWriter, Example) {
  pw::Status status = UartInterruptSafeWriterExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
