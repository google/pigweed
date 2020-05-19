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

#include "pw_rpc/internal/packet.h"

#include "gtest/gtest.h"

namespace pw::rpc::internal {
namespace {

using std::byte;

TEST(Packet, EncodeDecode) {
  constexpr byte payload[]{byte(0x00), byte(0x01), byte(0x02), byte(0x03)};

  Packet packet = Packet::Empty();
  packet.set_type(PacketType::RPC);
  packet.set_channel_id(12);
  packet.set_service_id(0xdeadbeef);
  packet.set_method_id(0x03a82921);
  packet.set_payload(payload);

  byte buffer[128];
  StatusWithSize sws = packet.Encode(buffer);
  ASSERT_EQ(sws.status(), Status::OK);

  span<byte> packet_data(buffer, sws.size());
  Packet decoded = Packet::FromBuffer(packet_data);

  EXPECT_EQ(decoded.type(), packet.type());
  EXPECT_EQ(decoded.channel_id(), packet.channel_id());
  EXPECT_EQ(decoded.service_id(), packet.service_id());
  EXPECT_EQ(decoded.method_id(), packet.method_id());
  ASSERT_EQ(decoded.payload().size(), packet.payload().size());
  EXPECT_EQ(std::memcmp(decoded.payload().data(),
                        packet.payload().data(),
                        packet.payload().size()),
            0);
}

}  // namespace
}  // namespace pw::rpc::internal
