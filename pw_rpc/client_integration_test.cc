// Copyright 2021 The Pigweed Authors
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

#include <cstring>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_rpc/benchmark.raw_rpc.pb.h"
#include "pw_rpc/integration_testing.h"
#include "pw_sync/binary_semaphore.h"

namespace rpc_test {
namespace {

constexpr int kIterations = 3;

using namespace std::chrono_literals;
using pw::ByteSpan;
using pw::ConstByteSpan;
using pw::Function;
using pw::OkStatus;
using pw::Status;

using pw::rpc::pw_rpc::raw::Benchmark;

Benchmark::Client kServiceClient(pw::rpc::integration_test::client(),
                                 pw::rpc::integration_test::kChannelId);

class StringReceiver {
 public:
  const char* Wait() {
    PW_CHECK(sem_.try_acquire_for(1500ms));
    return buffer_;
  }

  Function<void(ConstByteSpan, Status)> UnaryOnCompleted() {
    return [this](ConstByteSpan data, Status) { CopyPayload(data); };
  }

  Function<void(ConstByteSpan)> OnNext() {
    return [this](ConstByteSpan data) { CopyPayload(data); };
  }

 private:
  void CopyPayload(ConstByteSpan data) {
    std::memset(buffer_, 0, sizeof(buffer_));
    PW_CHECK_UINT_LE(data.size(), sizeof(buffer_));
    std::memcpy(buffer_, data.data(), data.size());
    sem_.release();
  }

  pw::sync::BinarySemaphore sem_;
  char buffer_[64];
};

TEST(RawRpcIntegrationTest, Unary) {
  for (int i = 0; i < kIterations; ++i) {
    StringReceiver receiver;
    pw::rpc::RawUnaryReceiver call = kServiceClient.UnaryEcho(
        std::as_bytes(std::span("hello")), receiver.UnaryOnCompleted());
    EXPECT_STREQ(receiver.Wait(), "hello");
  }
}

TEST(RawRpcIntegrationTest, BidirectionalStreaming) {
  for (int i = 0; i < kIterations; ++i) {
    StringReceiver receiver;
    pw::rpc::RawClientReaderWriter call =
        kServiceClient.BidirectionalEcho(receiver.OnNext());

    ASSERT_EQ(OkStatus(), call.Write(std::as_bytes(std::span("Yello"))));
    EXPECT_STREQ(receiver.Wait(), "Yello");

    ASSERT_EQ(OkStatus(), call.Write(std::as_bytes(std::span("Dello"))));
    EXPECT_STREQ(receiver.Wait(), "Dello");

    ASSERT_EQ(OkStatus(), call.Cancel());
  }
}

}  // namespace
}  // namespace rpc_test

int main(int argc, char* argv[]) {
  if (!pw::rpc::integration_test::InitializeClient(argc, argv).ok()) {
    return 1;
  }
  return RUN_ALL_TESTS();
}
