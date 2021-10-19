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
#include <thread>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_hdlc/rpc_channel.h"
#include "pw_hdlc/rpc_packets.h"
#include "pw_log/log.h"
#include "pw_rpc/benchmark.raw_rpc.pb.h"
#include "pw_stream/socket_stream.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/logging_event_handler.h"

namespace rpc_test {
namespace {

constexpr size_t kMaxTransmissionUnit = 512;
constexpr uint32_t kChannelId = 1;
constexpr int kIterations = 3;

pw::stream::SocketStream stream;
pw::hdlc::RpcChannelOutputBuffer<kMaxTransmissionUnit> channel_output(
    stream, pw::hdlc::kDefaultRpcAddress, "socket");
pw::rpc::Channel channel =
    pw::rpc::Channel::Create<kChannelId>(&channel_output);

}  // namespace

pw::rpc::Client client(std::span(&channel, 1));

namespace {

using namespace std::chrono_literals;
using pw::ByteSpan;
using pw::ConstByteSpan;
using pw::Function;
using pw::OkStatus;
using pw::Status;

using pw::rpc::pw_rpc::raw::Benchmark;

void ProcessClientPackets() {
  std::byte decode_buffer[kMaxTransmissionUnit];
  pw::hdlc::Decoder decoder(decode_buffer);

  while (true) {
    std::byte byte[1];
    pw::Result<ByteSpan> read = stream.Read(byte);

    if (!read.ok() || read->size() == 0u) {
      continue;
    }

    if (auto result = decoder.Process(*byte); result.ok()) {
      pw::hdlc::Frame& frame = result.value();
      if (frame.address() == pw::hdlc::kDefaultRpcAddress) {
        PW_CHECK_OK(client.ProcessPacket(frame.data()),
                    "Processing a packet failed");
      }
    }
  }
}

constexpr Benchmark::Client kServiceClient(client, kChannelId);

class StringReceiver {
 public:
  const char* Wait() {
    PW_CHECK(sem_.try_acquire_for(500ms));
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

    call.Cancel();
  }
}

}  // namespace
}  // namespace rpc_test

int main(int argc, char* argv[]) {
  if (argc != 2) {
    PW_LOG_ERROR("Usage: %s PORT", argv[0]);
    return 1;
  }

  const int port = std::atoi(argv[1]);

  PW_LOG_INFO("Connecting to pw_rpc client at localhost:%d", port);
  PW_CHECK_OK(rpc_test::stream.Connect("localhost", port));

  PW_LOG_INFO("Starting pw_rpc client");
  std::thread{rpc_test::ProcessClientPackets}.detach();

  pw::unit_test::LoggingEventHandler log_test_events;
  pw::unit_test::RegisterEventHandler(&log_test_events);
  return RUN_ALL_TESTS();
}
