// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_signaling_server.h"

#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::testing {

class FakeSignalingServerTest : public gtest::TestLoopFixture {
 public:
  FakeSignalingServerTest() = default;
  ~FakeSignalingServerTest() override = default;

  // Each test sets up its own FakeL2cap and FakeSignalingServer, so only
  // instantiate constants here.
  void SetUp() override { TestLoopFixture::SetUp(); }
  hci_spec::ConnectionHandle kConnectionHandle = 0x01;
  l2cap::CommandId kCommandId = 0x02;

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeSignalingServerTest);
};

TEST_F(FakeSignalingServerTest, ExtendedFeaturesInformationRequest) {
  // Copy the received packet to a local variable.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);

  // Assemble and send the information request.
  auto sent_acl_packet = l2cap::testing::AclExtFeaturesInfoReq(kCommandId, kConnectionHandle);
  const auto& send_header = sent_acl_packet.To<hci_spec::ACLDataHeader>();
  auto send_header_len = sizeof(send_header);
  auto send_payload_len = le16toh(send_header.data_total_length);
  auto sent_packet = DynamicByteBuffer(send_payload_len);
  sent_acl_packet.Copy(&sent_packet, send_header_len, send_payload_len);
  fake_l2cap.HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::ExtendedFeatures extended_features =
      l2cap::kExtendedFeaturesBitFixedChannels | l2cap::kExtendedFeaturesBitEnhancedRetransmission;
  auto expected_acl_response =
      l2cap::testing::AclExtFeaturesInfoRsp(kCommandId, kConnectionHandle, extended_features);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
}

TEST_F(FakeSignalingServerTest, FixedChannelInformationRequest) {
  // Copy the received packet to a local variable.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);

  // Assemble and send the information request.
  auto sent_acl_packet =
      l2cap::testing::AclFixedChannelsSupportedInfoReq(kCommandId, kConnectionHandle);
  const auto& send_header = sent_acl_packet.To<hci_spec::ACLDataHeader>();
  auto send_header_len = sizeof(send_header);
  auto send_payload_len = le16toh(send_header.data_total_length);
  auto sent_packet = DynamicByteBuffer(send_payload_len);
  sent_acl_packet.Copy(&sent_packet, send_header_len, send_payload_len);
  fake_l2cap.HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::FixedChannelsSupported fixed_channels = l2cap::kFixedChannelsSupportedBitSignaling;
  auto expected_acl_response = l2cap::testing::AclFixedChannelsSupportedInfoRsp(
      kCommandId, kConnectionHandle, fixed_channels);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
}

TEST_F(FakeSignalingServerTest, RejectInvalidInformationRequest) {
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);

  // Construct and send a custom invalid packet here.
  StaticByteBuffer sent_packet(
      // Length = 0x06 (4 byte header + 2 byte information type)
      0x06, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Command code for information request = 0x0A, CommandID = 0x02
      l2cap::kInformationRequest, kCommandId,
      // Payload length = 0x02
      0x02, 0x00,
      // Information type = 0x0004
      LowerBits(0x0004), UpperBits(0x0004));
  fake_l2cap.HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::ChannelId cid = l2cap::kSignalingChannelId;
  auto expected_acl_response =
      l2cap::testing::AclCommandRejectNotUnderstoodRsp(kCommandId, kConnectionHandle, cid);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
}

}  // namespace bt::testing
