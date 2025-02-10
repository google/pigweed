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

#include "pw_uart/stream.h"

#include "pw_unit_test/framework.h"

namespace pw::uart {
namespace {

class UartStub : public Uart {
 private:
  Status DoEnable(bool) override { return OkStatus(); }
  StatusWithSize DoTryReadFor(
      ByteSpan, size_t, std::optional<chrono::SystemClock::duration>) override {
    return StatusWithSize(0);
  }
  StatusWithSize DoTryWriteFor(
      ConstByteSpan, std::optional<chrono::SystemClock::duration>) override {
    return StatusWithSize(0);
  }

  Status DoSetBaudRate(uint32_t) override { return OkStatus(); }
  Status DoSetFlowControl(bool) override { return OkStatus(); }
  size_t DoConservativeReadAvailable() override { return 0; }
  Status DoFlushOutput() override { return OkStatus(); }
  Status DoClearPendingReceiveBytes() override { return OkStatus(); }
};

class UartStreamTest : public ::testing::Test {
 public:
  UartStreamTest() : stream_(uart_) {}

 private:
  UartStub uart_;
  UartStream stream_;
};

TEST_F(UartStreamTest, CompilationSucceeds) { EXPECT_TRUE(true); }

}  // namespace
}  // namespace pw::uart
