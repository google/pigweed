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
#include "pw_bluetooth_sapphire/internal/host/transport/emboss_packet.h"

#include <pw_bluetooth/hci_android.emb.h>
#include <pw_bluetooth/hci_commands.emb.h>
#include <pw_bluetooth/hci_test.emb.h>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"
#include "pw_bluetooth_sapphire/internal/host/transport/emboss_control_packets.h"
#include "pw_unit_test/framework.h"

using bt::ContainersEqual;

namespace bt::hci {
namespace {

namespace android_hci = hci_spec::vendor::android;
namespace android_emb = pw::bluetooth::vendor::android_hci;

TEST(StaticPacketTest, StaticPacketBasic) {
  StaticPacket<pw::bluetooth::emboss::TestCommandPacketWriter> packet;
  packet.view().header().opcode_bits().BackingStorage().WriteUInt(1234);
  packet.view().header().parameter_total_size().Write(1);
  packet.view().payload().Write(13);

  EXPECT_EQ(packet.data(), BufferView({0xD2, 0x04, 0x01, 0x0D}));

  packet.SetToZeros();
  EXPECT_EQ(packet.data(), BufferView({0, 0, 0, 0}));
}

TEST(EmbossCommandPacketTest, EmbossCommandPacketBasic) {
  auto packet =
      EmbossCommandPacket::New<pw::bluetooth::emboss::TestCommandPacketWriter>(
          1234);
  packet.view_t().payload().Write(13);

  EXPECT_EQ(packet.size(), 4u);
  EXPECT_EQ(packet.data(), BufferView({0xD2, 0x04, 0x01, 0x0D}));
  EXPECT_EQ(packet.mutable_data(), packet.data());
  EXPECT_EQ(packet.opcode(), 1234);
  EXPECT_EQ(packet.ocf(), 1234 & 0x3FF);
  EXPECT_EQ(packet.ogf(), 1234 >> 10);
  EXPECT_EQ(packet.view_t().payload().Read(), 13);
}

TEST(EmbossCommandPacketTest, EmbossCommandPacketDeathTest) {
  EmbossCommandPacket packet =
      EmbossCommandPacket::New<pw::bluetooth::emboss::TestCommandPacketView>(
          1234);

  // Try and fail to request view for struct larger than TestCommandPacket.
  EXPECT_DEATH_IF_SUPPORTED(
      packet.view<pw::bluetooth::emboss::InquiryCommandView>(), "IsComplete");
  // Try and fail to allocate 0 length packet (needs at least 3 bytes for the
  // header).
  EXPECT_DEATH_IF_SUPPORTED(
      EmbossCommandPacket::New<pw::bluetooth::emboss::CommandHeaderView>(1234,
                                                                         0),
      "command packet size must be at least 3 bytes");
}

TEST(EmbossEventPacketTest, EmbossEventPacketBasic) {
  auto packet =
      EmbossEventPacket::New<pw::bluetooth::emboss::TestEventPacketWriter>(123);
  packet.view_t().payload().Write(13);

  EXPECT_EQ(packet.size(), 3u);
  EXPECT_EQ(packet.data(), BufferView({0x7B, 0x01, 0x0D}));
  EXPECT_EQ(packet.mutable_data(), packet.data());
  EXPECT_EQ(packet.event_code(), 123);
  EXPECT_EQ(packet.view_t().payload().Read(), 13);
}

TEST(EmbossEventPacketTest, EmbossEventPacketDeathTest) {
  EmbossEventPacket packet =
      EmbossEventPacket::New<pw::bluetooth::emboss::TestEventPacketView>(123);

  // Try and fail to allocate 0 length packet (needs at least 2 bytes for the
  // header).
  EXPECT_DEATH_IF_SUPPORTED(EmbossEventPacket::New(0),
                            "event packet size must be at least 2 bytes");
}

TEST(EmbossEventPacketTest, StatusCode) {
  // Confirm status can be read from vendor subevent.
  auto packet =
      EmbossEventPacket::New<android_emb::LEMultiAdvtStateChangeSubeventWriter>(
          hci_spec::kVendorDebugEventCode);
  auto view = packet.view_t();
  view.status().Write(hci_spec::StatusCode::OPERATION_CANCELLED_BY_HOST);
  view.vendor_event().subevent_code().Write(
      android_hci::kLEMultiAdvtStateChangeSubeventCode);

  ASSERT_TRUE(packet.StatusCode().has_value());
  EXPECT_EQ(packet.StatusCode().value(),
            hci_spec::StatusCode::OPERATION_CANCELLED_BY_HOST);
  EXPECT_EQ(packet.ToResult(),
            ToResult(hci_spec::StatusCode::OPERATION_CANCELLED_BY_HOST));
}

TEST(EmbossPacketTest, ACLDataPacketFromFields) {
  constexpr size_t kLargeDataLength = 10;
  constexpr size_t kSmallDataLength = 1;

  auto packet =
      ACLDataPacket::New(0x007F,
                         hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                         hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
                         kSmallDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0x07F
  // Upper 4-bits: 0b0101
  EXPECT_TRUE(
      ContainersEqual(packet->view().data(),
                      std::array<uint8_t, 5>{{0x7F, 0x50, 0x01, 0x00, 0x00}}));

  packet =
      ACLDataPacket::New(0x0FFF,
                         hci_spec::ACLPacketBoundaryFlag::kCompletePDU,
                         hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
                         kSmallDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0111
  EXPECT_TRUE(
      ContainersEqual(packet->view().data(),
                      std::array<uint8_t, 5>{{0xFF, 0x7F, 0x01, 0x00, 0x00}}));

  packet =
      ACLDataPacket::New(0x0FFF,
                         hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint,
                         kLargeDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0000
  EXPECT_TRUE(ContainersEqual(packet->view().data(),
                              std::array<uint8_t, 14>{{0xFF,
                                                       0x0F,
                                                       0x0A,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00,
                                                       0x00}}));
}

TEST(EmbossPacketTest, ACLDataPacketFromBuffer) {
  constexpr size_t kLargeDataLength = 256;
  constexpr size_t kSmallDataLength = 1;

  // The same test cases as ACLDataPacketFromFields test above but in the
  // opposite direction.

  // First 12-bits: 0x07F
  // Upper 4-bits: 0b0101
  auto bytes = StaticByteBuffer(0x7F, 0x50, 0x01, 0x00, 0x00);
  auto packet = ACLDataPacket::New(kSmallDataLength);
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();

  EXPECT_EQ(0x007F, packet->connection_handle());
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
            packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
            packet->broadcast_flag());
  EXPECT_EQ(kSmallDataLength, packet->view().payload_size());

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0111
  bytes = StaticByteBuffer(0xFF, 0x7F, 0x01, 0x00, 0x00);
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();

  EXPECT_EQ(0x0FFF, packet->connection_handle());
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kCompletePDU,
            packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
            packet->broadcast_flag());
  EXPECT_EQ(kSmallDataLength, packet->view().payload_size());

  packet = ACLDataPacket::New(kLargeDataLength);
  packet->mutable_view()->mutable_data().Write(
      StaticByteBuffer(0xFF, 0x0F, 0x00, 0x01));
  packet->InitializeFromBuffer();

  EXPECT_EQ(0x0FFF, packet->connection_handle());
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
            packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kPointToPoint,
            packet->broadcast_flag());
  EXPECT_EQ(kLargeDataLength, packet->view().payload_size());
}

}  // namespace
}  // namespace bt::hci
