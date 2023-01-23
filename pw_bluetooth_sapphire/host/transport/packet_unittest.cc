// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <endian.h>
#include <zircon/compiler.h>

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"

using bt::ContainersEqual;
using bt::StaticByteBuffer;

namespace bt::hci::test {
namespace {

constexpr hci_spec::OpCode kTestOpCode = 0x07FF;
constexpr hci_spec::EventCode kTestEventCode = 0xFF;

struct TestPayload {
  uint8_t foo;
} __PACKED;

TEST(PacketTest, CommandPacket) {
  constexpr size_t kPayloadSize = sizeof(TestPayload);
  auto packet = CommandPacket::New(kTestOpCode, kPayloadSize);

  EXPECT_EQ(kTestOpCode, packet->opcode());
  EXPECT_EQ(kPayloadSize, packet->view().payload_size());

  uint8_t foo = 0x7F;
  packet->mutable_payload<TestPayload>()->foo = foo;

  // clang-format off

  StaticByteBuffer kExpected(
      0xFF, 0x07,  // opcode
      0x01,        // parameter_total_size
      foo
  );

  // clang-format on

  EXPECT_TRUE(ContainersEqual(kExpected, packet->view().data()));
}

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

TEST(PacketTest, EventPacketReturnParams) {
  uint8_t return_parameter = 0x7F;

  // clang-format off
 StaticByteBuffer correct_size_bad_event_code(
      // Event header
      0xFF, 0x04,  // (event_code is not CommandComplete)

      // hci_spec::CommandCompleteEventParams
      0x01, 0xFF, 0x07,

      return_parameter);
  auto cmd_complete_small_payload = StaticByteBuffer(
      // Event header
      0x0E, 0x03,

      // hci_spec::CommandCompleteEventParams
      0x01, 0xFF, 0x07);
  auto valid = StaticByteBuffer(
      // Event header
      0x0E, 0x04,

      // hci_spec::CommandCompleteEventParams
      0x01, 0xFF, 0x07,

      return_parameter);
  // clang-format on

  // Allocate a large enough packet which we'll reuse for the 3 payloads.
  auto packet = EventPacket::New(valid.size());

  // If the event code or the payload size don't match, then return_params()
  // should return nullptr.
  packet->mutable_view()->mutable_data().Write(correct_size_bad_event_code);
  packet->InitializeFromBuffer();
  EXPECT_EQ(nullptr, packet->return_params<TestPayload>());

  packet->mutable_view()->mutable_data().Write(cmd_complete_small_payload);
  packet->InitializeFromBuffer();
  EXPECT_EQ(nullptr, packet->return_params<TestPayload>());

  // Reset packet size to the original so that |valid| can fit.
  packet->mutable_view()->Resize(valid.size());

  // Valid case
  packet->mutable_view()->mutable_data().Write(valid);
  packet->InitializeFromBuffer();
  ASSERT_NE(nullptr, packet->return_params<TestPayload>());
  EXPECT_EQ(return_parameter, packet->return_params<TestPayload>()->foo);
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
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE), status);
}

TEST(PacketTest, CommandCompleteEventStatus) {
  // clang-format off
  auto evt = StaticByteBuffer(
      // Event header
      0x0E, 0x04,  // (event code is CommandComplete)

      // hci_spec::CommandCompleteEventParams
      0x01, 0xFF, 0x07,

      // Return parameters (status: hardware failure)
      0x03);
  // clang-format on

  auto packet = EventPacket::New(evt.size());
  packet->mutable_view()->mutable_data().Write(evt);
  packet->InitializeFromBuffer();

  Result<> status = packet->ToResult();
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::HARDWARE_FAILURE), status);
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

TEST(PacketTest, LEEventParams) {
  uint8_t subevent_payload = 0x7F;

  // clang-format off
  auto correct_size_bad_event_code = StaticByteBuffer(
      // Event header
      0xFE, 0x02,  // (event_code is not hci_spec::kLEMetaEventCode)

      // Subevent code
      0xFF,

      subevent_payload);
  auto payload_too_small = StaticByteBuffer(
      0x3E, 0x01,

      // Subevent code
      0xFF);
  auto valid = StaticByteBuffer(
      // Event header
      0x3E, 0x02,

      // Subevent code
      0xFF,

      subevent_payload);
  // clang-format on

  auto packet = EventPacket::New(valid.size());

  // If the event code or the payload size don't match, then return_params()
  // should return nullptr.
  packet->mutable_view()->mutable_data().Write(correct_size_bad_event_code);
  packet->InitializeFromBuffer();
  EXPECT_EQ(nullptr, packet->subevent_params<TestPayload>());

  packet->mutable_view()->mutable_data().Write(payload_too_small);
  packet->InitializeFromBuffer();
  EXPECT_EQ(nullptr, packet->subevent_params<TestPayload>());

  // Valid case
  packet->mutable_view()->Resize(valid.size());
  packet->mutable_view()->mutable_data().Write(valid);
  packet->InitializeFromBuffer();

  EXPECT_NE(nullptr, packet->subevent_params<TestPayload>());
  EXPECT_EQ(subevent_payload, packet->subevent_params<TestPayload>()->foo);
}

TEST(PacketTest, ACLDataPacketFromFields) {
  constexpr size_t kLargeDataLength = 10;
  constexpr size_t kSmallDataLength = 1;

  auto packet =
      ACLDataPacket::New(0x007F, hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                         hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast, kSmallDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0x07F
  // Upper 4-bits: 0b0101
  EXPECT_TRUE(ContainersEqual(packet->view().data(),
                              std::array<uint8_t, 5>{{0x7F, 0x50, 0x01, 0x00, 0x00}}));

  packet =
      ACLDataPacket::New(0x0FFF, hci_spec::ACLPacketBoundaryFlag::kCompletePDU,
                         hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast, kSmallDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0111
  EXPECT_TRUE(ContainersEqual(packet->view().data(),
                              std::array<uint8_t, 5>{{0xFF, 0x7F, 0x01, 0x00, 0x00}}));

  packet = ACLDataPacket::New(0x0FFF, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci_spec::ACLBroadcastFlag::kPointToPoint, kLargeDataLength);
  packet->mutable_view()->mutable_payload_data().Fill(0);

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0000
  EXPECT_TRUE(ContainersEqual(packet->view().data(),
                              std::array<uint8_t, 14>{{0xFF, 0x0F, 0x0A, 0x00, 0x00, 0x00, 0x00,
                                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}));
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
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kContinuingFragment, packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast, packet->broadcast_flag());
  EXPECT_EQ(kSmallDataLength, packet->view().payload_size());

  // First 12-bits: 0xFFF
  // Upper 4-bits: 0b0111
  bytes = StaticByteBuffer(0xFF, 0x7F, 0x01, 0x00, 0x00);
  packet->mutable_view()->mutable_data().Write(bytes);
  packet->InitializeFromBuffer();

  EXPECT_EQ(0x0FFF, packet->connection_handle());
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kCompletePDU, packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast, packet->broadcast_flag());
  EXPECT_EQ(kSmallDataLength, packet->view().payload_size());

  packet = ACLDataPacket::New(kLargeDataLength);
  packet->mutable_view()->mutable_data().Write(StaticByteBuffer(0xFF, 0x0F, 0x00, 0x01));
  packet->InitializeFromBuffer();

  EXPECT_EQ(0x0FFF, packet->connection_handle());
  EXPECT_EQ(hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable, packet->packet_boundary_flag());
  EXPECT_EQ(hci_spec::ACLBroadcastFlag::kPointToPoint, packet->broadcast_flag());
  EXPECT_EQ(kLargeDataLength, packet->view().payload_size());
}

}  // namespace
}  // namespace bt::hci::test
