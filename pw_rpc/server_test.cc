// Copyright 2020 The Pigweed Authors
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

#include "pw_rpc/server.h"

#include "gtest/gtest.h"

namespace pw::rpc {
namespace {

using std::byte;

template <size_t buffer_size>
class TestOutput : public ChannelOutput {
 public:
  constexpr TestOutput(uint32_t id) : ChannelOutput(id), sent_packet_({}) {}

  span<byte> AcquireBuffer() override { return buffer_; }

  void SendAndReleaseBuffer(size_t size) override {
    sent_packet_ = {buffer_, size};
  }

  span<const byte> sent_packet() const { return sent_packet_; }

 private:
  byte buffer_[buffer_size];
  span<const byte> sent_packet_;
};

TestOutput<512> output(1);

// clang-format off
constexpr uint8_t encoded_packet[] = {
  // type = PacketType::kRpc
  0x08, 0x00,
  // channel_id = 1
  0x10, 0x01,
  // service_id = 42
  0x18, 0x2a,
  // method_id = 27
  0x20, 0x1b,
  // payload
  0x82, 0x02, 0xff, 0xff,
};
// clang-format on

TEST(Server, DoesStuff) {
  Channel channels[] = {
      Channel(1, &output),
      Channel(2, &output),
  };
  Server server(channels);
  internal::Service service(42, {});
  server.RegisterService(service);

  server.ProcessPacket(as_bytes(span(encoded_packet)), output);
  auto packet = output.sent_packet();
  EXPECT_GT(packet.size(), 0u);
}

}  // namespace
}  // namespace pw::rpc
