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
#include "pw_stream_uart_mcuxpresso/stream.h"
#include "pw_unit_test/framework.h"

namespace examples {

pw::Status UartStreamExample() {
  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartStreamExample]
  constexpr uint32_t kFlexcomm = 0;
  constexpr uint32_t kBaudRate = 115200;
  std::array<std::byte, 20> kBuffer = {};

  auto stream = pw::stream::UartStreamMcuxpresso{
      USART0, kBaudRate, kUSART_ParityDisabled, kUSART_OneStopBit, kBuffer};

  PW_TRY(stream.Init(CLOCK_GetFlexcommClkFreq(kFlexcomm)));

  std::array<std::byte, 10> to_write = {};
  PW_TRY(stream.Write(to_write));

  // DOCSTAG: [pw_stream_uart_mcuxpresso-UartStreamExample]

  // Do something else

  return pw::OkStatus();
}

TEST(Stream, Example) {
  pw::Status status = UartStreamExample();
  EXPECT_EQ(status.code(), PW_STATUS_OK);
}
}  // namespace examples
