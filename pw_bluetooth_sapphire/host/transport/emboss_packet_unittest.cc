// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "emboss_packet.h"

#include <gtest/gtest.h>

#include "emboss_control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h"

namespace bt::hci {
namespace {

TEST(StaticPacketTest, StaticPacketBasic) {
  StaticPacket<pw::bluetooth::emboss::TestCommandPacketWriter> packet;
  packet.view().header().opcode().BackingStorage().WriteUInt(1234);
  packet.view().header().parameter_total_size().Write(1);
  packet.view().payload().Write(13);

  EXPECT_EQ(packet.data(), BufferView({0xD2, 0x04, 0x01, 0x0D}));

  packet.SetToZeros();
  EXPECT_EQ(packet.data(), BufferView({0, 0, 0, 0}));
}

TEST(EmbossCommandPacketTest, EmbossCommandPacketBasic) {
  auto packet = EmbossCommandPacket::New<pw::bluetooth::emboss::TestCommandPacketWriter>(1234);
  packet.view_t().payload().Write(13);

  EXPECT_EQ(packet.size(), 4u);
  EXPECT_EQ(packet.data(), BufferView({0xD2, 0x04, 0x01, 0x0D}));
  EXPECT_EQ(packet.mutable_data(), packet.data());
  EXPECT_EQ(packet.opcode(), 1234);
  EXPECT_EQ(packet.ocf(), 1234 & 0x3FF);
  EXPECT_EQ(packet.ogf(), 1234 >> 10);
  EXPECT_EQ(packet.view<pw::bluetooth::emboss::TestCommandPacketView>().payload().Read(), 13);
}

TEST(EmbossCommandPacketTest, EmbossCommandPacketDeathTest) {
  EmbossCommandPacket packet =
      EmbossCommandPacket::New<pw::bluetooth::emboss::TestCommandPacketView>(1234);

  // Try and fail to request view for struct larger than TestCommandPacket.
  EXPECT_DEATH_IF_SUPPORTED(packet.view<pw::bluetooth::emboss::InquiryCommandView>(),
                            "emboss packet buffer not large enough");
  // Try and fail to allocate 0 length packet (needs at least 3 bytes for the header).
  EXPECT_DEATH_IF_SUPPORTED(
      EmbossCommandPacket::New<pw::bluetooth::emboss::CommandHeaderView>(1234, 0),
      "command packet size must be at least 3 bytes");
}

}  // namespace
}  // namespace bt::hci
