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
#include "pw_protobuf/codegen.h"
#include "pw_protobuf/wire_format.h"

namespace pw::rpc::internal {
namespace {

using std::byte;

constexpr byte kPayload[] = {byte(0x82), byte(0x02), byte(0xff), byte(0xff)};

constexpr byte kEncoded[] = {
    // Payload
    byte{MakeKey(5, protobuf::WireType::kDelimited)},
    byte{0x04},
    byte{0x82},
    byte{0x02},
    byte{0xff},
    byte{0xff},

    // Packet type
    byte{MakeKey(1, protobuf::WireType::kVarint)},
    byte{0},  // RPC

    // Channel ID
    byte{MakeKey(2, protobuf::WireType::kVarint)},
    byte{1},

    // Service ID
    byte{MakeKey(3, protobuf::WireType::kVarint)},
    byte{42},

    // Method ID
    byte{MakeKey(4, protobuf::WireType::kVarint)},
    byte{100},

    // Status
    byte{MakeKey(6, protobuf::WireType::kVarint)},
    byte{0x00},
};

constexpr size_t kReservedSize = 2 /* type */ + 2 /* channel */ +
                                 2 /* service */ + 2 /* method */ +
                                 2 /* payload key */ + 2 /* status */;

TEST(Packet, Encode) {
  byte buffer[64];

  Packet packet(PacketType::RPC, 1, 42, 100, kPayload);

  auto sws = packet.Encode(buffer);
  ASSERT_EQ(sizeof(kEncoded), sws.size());
  EXPECT_EQ(std::memcmp(kEncoded, buffer, sizeof(kEncoded)), 0);
}

TEST(Packet, Decode) {
  Packet packet = Packet::FromBuffer(kEncoded);

  EXPECT_EQ(PacketType::RPC, packet.type());
  EXPECT_EQ(1u, packet.channel_id());
  EXPECT_EQ(42u, packet.service_id());
  EXPECT_EQ(100u, packet.method_id());
  ASSERT_EQ(sizeof(kPayload), packet.payload().size());
  EXPECT_EQ(0,
            std::memcmp(packet.payload().data(), kPayload, sizeof(kPayload)));
}

TEST(Packet, EncodeDecode) {
  constexpr byte payload[]{byte(0x00), byte(0x01), byte(0x02), byte(0x03)};

  Packet packet = Packet(PacketType::RPC);
  packet.set_channel_id(12);
  packet.set_service_id(0xdeadbeef);
  packet.set_method_id(0x03a82921);
  packet.set_payload(payload);
  packet.set_status(Status::UNAVAILABLE);

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
  EXPECT_EQ(decoded.status(), Status::UNAVAILABLE);
}

TEST(Packet, PayloadUsableSpace_EmptyBuffer) {
  Packet packet(PacketType::RPC, 1, 42, 100, kPayload);
  EXPECT_TRUE(packet.PayloadUsableSpace(span<byte>()).empty());
}

TEST(Packet, PayloadUsableSpace_TooSmall) {
  Packet packet(PacketType::RPC, 1, 42, 100, kPayload);

  byte buffer[10];
  EXPECT_TRUE(packet.PayloadUsableSpace(buffer).empty());
}

TEST(Packet, PayloadUsableSpace_ExactFit) {
  byte buffer[kReservedSize];
  const span payload =
      Packet(PacketType::RPC, 1, 42, 100).PayloadUsableSpace(buffer);

  EXPECT_EQ(payload.size(), sizeof(buffer) - kReservedSize);
  EXPECT_EQ(buffer + kReservedSize, payload.data());
}

TEST(Packet, PayloadUsableSpace_ExtraRoom) {
  byte buffer[kReservedSize * 3];
  const span payload =
      Packet(PacketType::RPC, 1, 42, 100).PayloadUsableSpace(buffer);

  EXPECT_EQ(payload.size(), sizeof(buffer) - kReservedSize);
  EXPECT_EQ(buffer + kReservedSize, payload.data());
}

TEST(Packet, PayloadUsableSpace_LargerVarints) {
  byte buffer[kReservedSize * 3];
  const span payload =
      Packet(PacketType::RPC, 17000, 200, 200).PayloadUsableSpace(buffer);

  constexpr size_t expected_size = kReservedSize + 2 + 1 + 1;

  EXPECT_EQ(payload.size(), sizeof(buffer) - expected_size);
  EXPECT_EQ(buffer + expected_size, payload.data());
}

}  // namespace
}  // namespace pw::rpc::internal
