// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_dynamic_channel.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_signaling_server.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::testing {
namespace {

hci_spec::ConnectionHandle kConnectionHandle = 0x01;
l2cap::CommandId kCommandId = 0x02;
l2cap::PSM kPsm = l2cap::kSDP;

TEST(FakeDynamicChannelTest, ConnectOpenDisconnectChannel) {
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto kConnectionHandle, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);
  auto channel_cb = [](auto fake_dynamic_channel) {};
  fake_l2cap.RegisterService(kPsm, channel_cb);
  l2cap::ChannelId src_id = l2cap::kFirstDynamicChannelId;
  l2cap::ChannelParameters params;

  // Assemble and send the ConnectionRequest to connect, but not open, the channel.
  auto connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& connection_header = connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto connection_header_len = sizeof(connection_header);
  auto connection_payload_len = le16toh(connection_header.data_total_length);
  auto connection_packet = DynamicByteBuffer(connection_payload_len);
  connection_acl_packet.Copy(&connection_packet, connection_header_len, connection_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, connection_packet);

  // Anticipate that we then receive a ConfigurationRequest. HandlePdu will
  // first send a ConnectionResponse, but the most recent packet should be a
  // ConfigurationRequest. The channel should also be connected, but not open,
  // at this time.
  // Manually create the expected ConfigurationRequest with no payload.
  StaticByteBuffer expected_request(
      // Configuration request command code, CommandId associated with the test
      l2cap::kConfigurationRequest, kCommandId,
      // Payload length (4 total bytes)
      0x04, 0x00,
      // Source ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id),
      // No continuation flags (2 bytes)
      0x00, 0x00);
  EXPECT_TRUE(ContainersEqual(expected_request, *received_packet));
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_request_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_response_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByRemoteId(kConnectionHandle, src_id)->opened());

  // Send a ConfigurationResponse to the received ConfigurationRequest.
  auto configuration_response_acl_packet =
      l2cap::testing::AclConfigRsp(kCommandId, kConnectionHandle, src_id, params);
  const auto& configuration_response_header =
      configuration_response_acl_packet.To<hci_spec::ACLDataHeader>();
  auto configuration_response_header_len = sizeof(configuration_response_header);
  auto configuration_response_payload_len =
      le16toh(configuration_response_header.data_total_length);
  auto configuration_response_packet = DynamicByteBuffer(configuration_response_payload_len);
  configuration_response_acl_packet.Copy(&configuration_response_packet,
                                         configuration_response_header_len,
                                         configuration_response_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, configuration_response_packet);
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_request_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_response_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByRemoteId(kConnectionHandle, src_id)->opened());

  // Assemble and send the ConfigurationRequest to open up the channel.
  // In this isolated test, we can assume that the src_id and dest_id are identical.
  auto configuration_request_acl_packet =
      l2cap::testing::AclConfigReq(kCommandId, kConnectionHandle, src_id, params);
  const auto& configuration_request_header =
      configuration_request_acl_packet.To<hci_spec::ACLDataHeader>();
  auto configuration_request_header_len = sizeof(configuration_request_header);
  auto configuration_request_payload_len = le16toh(configuration_request_header.data_total_length);
  auto configuration_request_packet = DynamicByteBuffer(configuration_request_payload_len);
  configuration_request_acl_packet.Copy(&configuration_request_packet,
                                        configuration_request_header_len,
                                        configuration_request_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, configuration_request_packet);

  // Anticipate that we then receive a ConfigurationResponse after we send a
  // Manually create the expected ConfigurationRequest with no payload.
  StaticByteBuffer expected_response(
      // Configuration request command code, CommandId associated with the test
      l2cap::kConfigurationResponse, kCommandId,
      // Payload length (6 total bytes)
      0x06, 0x00,
      // Source ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id),
      // No continuation flags (2 bytes)
      0x00, 0x00,
      // Result (Success)
      LowerBits(0x0000), UpperBits(0x0000));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_request_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_response_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByRemoteId(kConnectionHandle, src_id)->opened());

  // Assemble and send the DisconnectionRequest to open up the channel.
  // In this isolated test, we can assume that the src_id and dest_id are identical.
  auto disconnection_acl_packet =
      l2cap::testing::AclDisconnectionReq(kCommandId, kConnectionHandle, src_id, src_id);
  const auto& disconnection_header = disconnection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto disconnection_header_len = sizeof(disconnection_header);
  auto disconnection_payload_len = le16toh(disconnection_header.data_total_length);
  auto disconnection_packet = DynamicByteBuffer(disconnection_payload_len);
  disconnection_acl_packet.Copy(&disconnection_packet, disconnection_header_len,
                                disconnection_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, disconnection_packet);

  // Anticipate that we receive a DisconnectionResponse after we send the
  // request and that the channel has been deleted.
  StaticByteBuffer disconnection_response(
      // Configuration request command code, CommandId associated with the test
      l2cap::kDisconnectionResponse, kCommandId,
      // Payload length (4 total bytes)
      0x04, 0x00,
      // Source ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id),
      // Dest ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id));
  EXPECT_TRUE(ContainersEqual(disconnection_response, *received_packet));
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id));
}

TEST(FakeDynamicChannelTest, FailToRegisterChannelWithoutRegisteredService) {
  // Create a custom FakeL2cap with no registered services.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto kConnectionHandle, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap_without_service = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap_without_service);
  l2cap::ChannelId src_id = l2cap::kFirstDynamicChannelId;

  // Assemble and send the ConnectionRequest to connect, but not open, the channel.
  auto connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& connection_header = connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto connection_header_len = sizeof(connection_header);
  auto connection_payload_len = le16toh(connection_header.data_total_length);
  auto connection_packet = DynamicByteBuffer(connection_payload_len);
  connection_acl_packet.Copy(&connection_packet, connection_header_len, connection_payload_len);
  fake_l2cap_without_service.HandlePdu(kConnectionHandle, connection_packet);

  // Anticipate that we will receive a rejection as the packet is not supported.
  // As this is an isolated test case, assume that src_id and dst_id are the same.
  auto expected_acl_response = l2cap::testing::AclConnectionRsp(
      kCommandId, kConnectionHandle, src_id, l2cap::kInvalidChannelId,
      l2cap::ConnectionResult::kPSMNotSupported);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
  EXPECT_FALSE(fake_l2cap_without_service.FindDynamicChannelByLocalId(kConnectionHandle, src_id));
}

TEST(FakeDynamicChannelTest, FailToRegisterChannelWithInvalidCid) {
  // Configure FakeSignalingServer to copy any received signaling packets.
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto kConnectionHandle, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);
  auto channel_cb = [](auto fake_dynamic_channel) {};
  fake_l2cap.RegisterService(kPsm, channel_cb);
  l2cap::ChannelId src_id = l2cap::kInvalidChannelId;

  // Assemble and send the ConnectionRequest to connect, but not open, the channel.
  auto connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& connection_header = connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto connection_header_len = sizeof(connection_header);
  auto connection_payload_len = le16toh(connection_header.data_total_length);
  auto connection_packet = DynamicByteBuffer(connection_payload_len);
  connection_acl_packet.Copy(&connection_packet, connection_header_len, connection_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, connection_packet);

  // Anticipate that we will receive a rejection as the ID is not supported.
  auto expected_acl_response = l2cap::testing::AclConnectionRsp(
      kCommandId, kConnectionHandle, src_id, l2cap::kInvalidChannelId,
      l2cap::ConnectionResult::kInvalidSourceCID);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id));
}

TEST(FakeDynamicChannelTest, FailToRegisterDuplicateRemoteId) {
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto kConnectionHandle, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fake_l2cap);
  auto channel_cb = [](auto fake_dynamic_channel) {};
  fake_l2cap.RegisterService(kPsm, channel_cb);
  l2cap::ChannelId src_id = l2cap::kFirstDynamicChannelId;
  l2cap::ChannelParameters params;

  // Assemble and send the ConnectionRequest to connect, but not open, the channel.
  auto connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& connection_header = connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto connection_header_len = sizeof(connection_header);
  auto connection_payload_len = le16toh(connection_header.data_total_length);
  auto connection_packet = DynamicByteBuffer(connection_payload_len);
  connection_acl_packet.Copy(&connection_packet, connection_header_len, connection_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, connection_packet);

  // Anticipate that we then receive a ConfigurationRequest. HandlePdu will
  // first send a ConnectionResponse, but the most recent packet should be a
  // ConfigurationRequest. The channel should also be connected, but not open,
  // at this time.
  // Manually create the expected ConfigurationRequest with no payload.
  StaticByteBuffer expected_request(
      // Configuration request command code, CommandId associated with the test
      l2cap::kConfigurationRequest, kCommandId,
      // Payload length (4 total bytes)
      0x04, 0x00,
      // Source ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id),
      // No continuation flags (2 bytes)
      0x00, 0x00);
  EXPECT_TRUE(ContainersEqual(expected_request, *received_packet));
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_request_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_response_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)->opened());

  // Send a ConfigurationResponse to the received ConfigurationRequest.
  auto configuration_response_acl_packet =
      l2cap::testing::AclConfigRsp(kCommandId, kConnectionHandle, src_id, params);
  const auto& configuration_response_header =
      configuration_response_acl_packet.To<hci_spec::ACLDataHeader>();
  auto configuration_response_header_len = sizeof(configuration_response_header);
  auto configuration_response_payload_len =
      le16toh(configuration_response_header.data_total_length);
  auto configuration_response_packet = DynamicByteBuffer(configuration_response_payload_len);
  configuration_response_acl_packet.Copy(&configuration_response_packet,
                                         configuration_response_header_len,
                                         configuration_response_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, configuration_response_packet);
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                   ->configuration_request_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_response_received());
  EXPECT_FALSE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)->opened());

  // Assemble and send the ConfigurationRequest to open up the channel.
  // In this isolated test, we can assume that the src_id and dest_id are identical.
  auto configuration_request_acl_packet =
      l2cap::testing::AclConfigReq(kCommandId, kConnectionHandle, src_id, params);
  const auto& configuration_request_header =
      configuration_request_acl_packet.To<hci_spec::ACLDataHeader>();
  auto configuration_request_header_len = sizeof(configuration_request_header);
  auto configuration_request_payload_len = le16toh(configuration_request_header.data_total_length);
  auto configuration_request_packet = DynamicByteBuffer(configuration_request_payload_len);
  configuration_request_acl_packet.Copy(&configuration_request_packet,
                                        configuration_request_header_len,
                                        configuration_request_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, configuration_request_packet);

  // Anticipate that we then receive a ConfigurationResponse after we send a
  // Manually create the expected ConfigurationRequest with no payload.
  StaticByteBuffer expected_response(
      // Configuration request command code, CommandId associated with the test
      l2cap::kConfigurationResponse, kCommandId,
      // Payload length (6 total bytes)
      0x06, 0x00,
      // Source ID (2 bytes)
      LowerBits(src_id), UpperBits(src_id),
      // No continuation flags (2 bytes)
      0x00, 0x00,
      // Result (Success)
      LowerBits(0x0000), UpperBits(0x0000));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_request_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)
                  ->configuration_response_received());
  EXPECT_TRUE(fake_l2cap.FindDynamicChannelByLocalId(kConnectionHandle, src_id)->opened());

  // Try to open up the same channel again.
  auto second_connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& second_connection_header = second_connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto second_connection_header_len = sizeof(second_connection_header);
  auto second_connection_payload_len = le16toh(second_connection_header.data_total_length);
  auto second_connection_packet = DynamicByteBuffer(second_connection_payload_len);
  second_connection_acl_packet.Copy(&second_connection_packet, second_connection_header_len,
                                    second_connection_payload_len);
  fake_l2cap.HandlePdu(kConnectionHandle, second_connection_packet);

  // Anticipate that we will receive a rejection as the remote ID has already been registered.
  auto second_expected_acl_response = l2cap::testing::AclConnectionRsp(
      kCommandId, kConnectionHandle, src_id, l2cap::kInvalidChannelId,
      l2cap::ConnectionResult::kSourceCIDAlreadyAllocated);
  auto second_expected_response = second_expected_acl_response.view(
      sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(second_expected_response, *received_packet));
}

TEST(FakeDynamicChannelTest, FailWhenOutOfIds) {
  auto unexpected_cb = [](auto handle, auto& pdu) {};
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto kConnectionHandle, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fewer_ids_fake_l2cap_ = FakeL2cap(send_cb, unexpected_cb, l2cap::kFirstDynamicChannelId);
  auto server = std::make_unique<FakeSignalingServer>();
  server->RegisterWithL2cap(&fewer_ids_fake_l2cap_);
  auto channel_cb = [](auto fake_dynamic_channel) {};
  fewer_ids_fake_l2cap_.RegisterService(kPsm, channel_cb);
  l2cap::ChannelId src_id = l2cap::kFirstDynamicChannelId;

  // Assemble and send the ConnectionRequest to connect, but not open, the channel.
  auto connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, src_id, kPsm);
  const auto& connection_header = connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto connection_header_len = sizeof(connection_header);
  auto connection_payload_len = le16toh(connection_header.data_total_length);
  auto connection_packet = DynamicByteBuffer(connection_payload_len);
  connection_acl_packet.Copy(&connection_packet, connection_header_len, connection_payload_len);
  fewer_ids_fake_l2cap_.HandlePdu(kConnectionHandle, connection_packet);
  EXPECT_FALSE(
      fewer_ids_fake_l2cap_.FindDynamicChannelByLocalId(kConnectionHandle, src_id)->opened());

  // The FakeL2cap instance should now be out of ChannelIds to assign.
  l2cap::ChannelId second_src_id = l2cap::kFirstDynamicChannelId + 1;
  auto second_connection_acl_packet =
      l2cap::testing::AclConnectionReq(kCommandId, kConnectionHandle, second_src_id, kPsm);
  const auto& second_connection_header = second_connection_acl_packet.To<hci_spec::ACLDataHeader>();
  auto second_connection_header_len = sizeof(second_connection_header);
  auto second_connection_payload_len = le16toh(second_connection_header.data_total_length);
  auto second_connection_packet = DynamicByteBuffer(second_connection_payload_len);
  second_connection_acl_packet.Copy(&second_connection_packet, second_connection_header_len,
                                    second_connection_payload_len);
  fewer_ids_fake_l2cap_.HandlePdu(kConnectionHandle, second_connection_packet);

  // Anticipate that we will receive a rejection as there are no Ids left.
  auto expected_acl_response = l2cap::testing::AclConnectionRsp(
      kCommandId, kConnectionHandle, second_src_id, l2cap::kInvalidChannelId,
      l2cap::ConnectionResult::kNoResources);
  auto expected_response =
      expected_acl_response.view(sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::CommandHeader));
  EXPECT_TRUE(ContainersEqual(expected_response, *received_packet));
  EXPECT_FALSE(fewer_ids_fake_l2cap_.FindDynamicChannelByLocalId(kConnectionHandle, second_src_id));
}

}  // namespace
}  // namespace bt::testing
