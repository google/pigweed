// Copyright 2023 The Pigweed Authors
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

#include "pw_stream_uart_mcuxpresso/stream.h"

#include "pw_unit_test/framework.h"

namespace pw::stream {
namespace {

constexpr uint32_t kFlexcomm = 0;
constexpr uint32_t kBaudRate = 115200;
std::array<std::byte, 20> kBuffer = {};

TEST(UartStreamMcuxpresso, InitOk) {
  auto stream = UartStreamMcuxpresso{
      USART0, kBaudRate, kUSART_ParityDisabled, kUSART_OneStopBit, kBuffer};
  EXPECT_EQ(stream.Init(CLOCK_GetFlexcommClkFreq(kFlexcomm)), OkStatus());
}

}  // namespace
}  // namespace pw::stream
