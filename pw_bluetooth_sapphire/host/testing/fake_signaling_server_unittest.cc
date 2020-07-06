// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_signaling_server.h"

#include <lib/gtest/test_loop_fixture.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_l2cap.h"

namespace bt {
namespace testing {

class TESTING_FakeSignalingServerTest : public gtest::TestLoopFixture {
 public:
  TESTING_FakeSignalingServerTest() = default;
  ~TESTING_FakeSignalingServerTest() override = default;

  // Instantiate the FakeL2Cap instance that can be reused throughout these
  // tests. Tests should create their own FakeSignalingServer instances and
  // register them with FakeL2Cap so that they can access locally stored
  // received packets.
  void SetUp() override {
    TestLoopFixture::SetUp();
    fake_l2cap_ = std::make_unique<FakeL2cap>();
  }

  FakeL2cap& fake_l2cap() { return *fake_l2cap_; }

 private:
  std::unique_ptr<FakeL2cap> fake_l2cap_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TESTING_FakeSignalingServerTest);
};

TEST_F(TESTING_FakeSignalingServerTest, ExtendedFeaturesInformationRequest) {
  // Copy the received packet to a local variable.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };

  // Configure the FakeSignalingServer with the associated callback.
  FakeSignalingServer server(send_cb);
  server.RegisterWithL2cap(&fake_l2cap());
  l2cap::CommandId id = 0x02;
  hci::ConnectionHandle conn = 0x01;

  // Assemble and send the information request.
  auto sent_acl_packet = l2cap::testing::AclExtFeaturesInfoReq(id, conn);
  const auto& send_header = sent_acl_packet.As<hci::ACLDataHeader>();
  auto send_header_len = sizeof(send_header);
  auto send_payload_len = le16toh(send_header.data_total_length);
  auto sent_packet = DynamicByteBuffer(send_payload_len);
  sent_acl_packet.Copy(&sent_packet, send_header_len, send_payload_len);
  fake_l2cap().HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::ExtendedFeatures extended_features =
      l2cap::kExtendedFeaturesBitFixedChannels | l2cap::kExtendedFeaturesBitEnhancedRetransmission;
  auto expected_acl_response = l2cap::testing::AclExtFeaturesInfoRsp(id, conn, extended_features);
  auto expected_response =
      expected_acl_response.view(sizeof(hci::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
};

TEST_F(TESTING_FakeSignalingServerTest, FixedChannelInformationRequest) {
  // Copy the received packet to a local variable.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };

  // Configure the FakeSignalingServer with the associated callback.
  FakeSignalingServer server(send_cb);
  server.RegisterWithL2cap(&fake_l2cap());
  l2cap::CommandId id = 0x02;
  hci::ConnectionHandle conn = 0x01;

  // Assemble and send the information request.
  auto sent_acl_packet = l2cap::testing::AclFixedChannelsSupportedInfoReq(id, conn);
  const auto& send_header = sent_acl_packet.As<hci::ACLDataHeader>();
  auto send_header_len = sizeof(send_header);
  auto send_payload_len = le16toh(send_header.data_total_length);
  auto sent_packet = DynamicByteBuffer(send_payload_len);
  sent_acl_packet.Copy(&sent_packet, send_header_len, send_payload_len);
  fake_l2cap().HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::FixedChannelsSupported fixed_channels = l2cap::kFixedChannelsSupportedBitSignaling;
  auto expected_acl_response =
      l2cap::testing::AclFixedChannelsSupportedInfoRsp(id, conn, fixed_channels);
  auto expected_response =
      expected_acl_response.view(sizeof(hci::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
};

TEST_F(TESTING_FakeSignalingServerTest, RejectInvalidInformationRequest) {
  // Copy the received packet to a local variable.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };

  // Configure the FakeSignalingServer with the associated callback.
  FakeSignalingServer server(send_cb);
  server.RegisterWithL2cap(&fake_l2cap());
  l2cap::CommandId id = 0x02;
  hci::ConnectionHandle conn = 0x01;

  // Construct and send a custom invalid packet here.
  StaticByteBuffer sent_packet(
      // Length = 0x06 (4 byte header + 2 byte information type)
      0x06, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Command code for information request = 0x0A, CommandID = 0x02
      l2cap::kInformationRequest, id,
      // Payload length = 0x02
      0x02, 0x00,
      // Information type = 0x0004
      LowerBits(0x0004), UpperBits(0x0004));
  fake_l2cap().HandlePdu(0x001, sent_packet);

  // Assemble the expected packet and confirm that it matches the received packet.
  l2cap::ChannelId cid = l2cap::kSignalingChannelId;
  auto expected_acl_response = l2cap::testing::AclCommandRejectNotUnderstoodRsp(id, conn, cid);
  auto expected_response =
      expected_acl_response.view(sizeof(hci::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
};

}  // namespace testing
}  // namespace bt
