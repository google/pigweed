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

#include "pw_uart/uart.h"

#include "gtest/gtest.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::uart {
namespace {

class UartStub : public Uart {
 public:
  UartStub() {}
  ~UartStub() override = default;

 private:
  Status DoEnable(bool) override { return OkStatus(); }
  StatusWithSize DoTryReadFor(
      ByteSpan, std::optional<chrono::SystemClock::duration>) override {
    return StatusWithSize(0);
  }
  StatusWithSize DoTryWriteFor(
      ConstByteSpan, std::optional<chrono::SystemClock::duration>) override {
    return StatusWithSize(0);
  }

  Status DoSetBaudRate(uint32_t) override { return OkStatus(); }

  size_t DoConservativeReadAvailable() override { return 0; }
  Status DoFlushOutput() override { return OkStatus(); }
  Status DoClearPendingReceiveBytes() override { return OkStatus(); }
};

class UartTest : public ::testing::Test {
 public:
  UartTest() : stub_() {}

 private:
  UartStub stub_;
};

TEST_F(UartTest, CompilationSucceeds) { EXPECT_TRUE(true); }

}  // namespace
}  // namespace pw::uart