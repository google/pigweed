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

#include "pw_stream_uart_linux/stream.h"

#include "gtest/gtest.h"
#include "public/pw_stream_uart_linux/stream.h"

namespace pw::stream {
namespace {

// Path to a non-UART device.
constexpr auto kPathNonUart = "/dev/null";

TEST(UartStreamLinuxTest, TestOpenNonUartDevice) {
  UartStreamLinux uart;
  Status status = uart.Open(kPathNonUart, 115200);
  EXPECT_EQ(status, Status::Unknown());
}

TEST(UartStreamLinuxTest, TestOpenInvalidBaudRate) {
  UartStreamLinux uart;
  Status status = uart.Open(kPathNonUart, 123456);
  EXPECT_EQ(status, Status::InvalidArgument());
}

}  // namespace
}  // namespace pw::stream