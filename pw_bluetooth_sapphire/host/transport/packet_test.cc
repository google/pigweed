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

#include <array>
#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"
#include "pw_unit_test/framework.h"

using bt::ContainersEqual;
using bt::StaticByteBuffer;

namespace bt::hci::test {
namespace {

constexpr hci_spec::EventCode kTestEventCode = 0xFF;

struct TestPayload {
  uint8_t foo;
} __attribute__((packed));

TEST(PacketTest, EventPacket) {
  constexpr size_t kPayloadSize = sizeof(TestPayload);
  auto packet = EventPacket::New(kPayloadSize);
  uint8_t foo = 0x7F;

  // clang-format off
 StaticByteBuffer bytes(
      0xFF,  // event code
      0x01,  // parameter_total_size
      foo   // foo
  );
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();
  // clang-format on

  EXPECT_EQ(kTestEventCode, packet->event_code());
  EXPECT_EQ(kPayloadSize, packet->view().payload_size());
  EXPECT_EQ(foo, packet->params<TestPayload>().foo);
}

TEST(PacketTest, EventPacketStatus) {
  // clang-format off
  auto evt = StaticByteBuffer(
      // Event header
      0x05, 0x04,  // (event_code is DisconnectionComplete)

      // Disconnection Complete event parameters
      0x03,        // status: hardware failure
      0x01, 0x00,  // handle: 0x0001
      0x16         // reason: terminated by local host
  );
  // clang-format on

  auto packet = EventPacket::New(evt.size());
  packet->mutable_view()->mutable_data().Write(evt);
  packet->InitializeFromBuffer();

  Result<> status = packet->ToResult();
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE),
            status);
}

TEST(PacketTest, CommandCompleteEventStatus) {
  // clang-format off
  auto evt = StaticByteBuffer(
      // Event header
      0x0E, 0x04,  // (event code is CommandComplete)

      // pw::bluetooth::emboss::CommandCompleteEvent
      0x01, 0xFF, 0x07,

      // Return parameters (status: hardware failure)
      0x03);
  // clang-format on

  auto packet = EventPacket::New(evt.size());
  packet->mutable_view()->mutable_data().Write(evt);
  packet->InitializeFromBuffer();

  Result<> status = packet->ToResult();
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE),
            status);
}

TEST(PacketTest, EventPacketMalformed) {
  // clang-format off
  auto evt = StaticByteBuffer(
      // Event header
      0x05, 0x03,  // (event_code is DisconnectionComplete)

      // Disconnection Complete event parameters
      0x03,        // status: hardware failure
      0x01, 0x00   // handle: 0x0001
      // event is one byte too short
  );
  // clang-format on

  auto packet = EventPacket::New(evt.size());
  packet->mutable_view()->mutable_data().Write(evt);
  packet->InitializeFromBuffer();

  Result<> status = packet->ToResult();
  EXPECT_EQ(ToResult(HostError::kPacketMalformed), status);
}

TEST(PacketTest, ACLDataPacketFromFields) {
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

TEST(PacketTest, ACLDataPacketFromBuffer) {
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
}  // namespace bt::hci::test
