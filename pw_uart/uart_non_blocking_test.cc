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

#include "pw_uart/uart_non_blocking.h"

#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_unit_test/framework.h"

namespace pw::uart {
namespace {

class UartNonBlockingStub : public UartNonBlocking {
 public:
  UartNonBlockingStub() {}
  ~UartNonBlockingStub() override = default;

 private:
  Status DoEnable(bool) override { return OkStatus(); }
  Status DoSetBaudRate(uint32_t) override { return OkStatus(); }
  Status DoRead(ByteSpan,
                size_t,
                Function<void(Status, ConstByteSpan buffer)>&&) override {
    return OkStatus();
  }
  bool DoCancelRead() override { return true; }
  Status DoWrite(ConstByteSpan,
                 Function<void(StatusWithSize status)>&&) override {
    return OkStatus();
  }
  bool DoCancelWrite() override { return true; }
  size_t DoConservativeReadAvailable() override { return 0; }
  Status DoClearPendingReceiveBytes() override { return OkStatus(); }
};

class UartNonBlockingTest : public ::testing::Test {
 public:
  UartNonBlockingTest() : stub_() {}

 private:
  UartNonBlockingStub stub_;
};

TEST_F(UartNonBlockingTest, CompilationSucceeds) { EXPECT_TRUE(true); }

}  // namespace
}  // namespace pw::uart
