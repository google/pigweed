// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "sco_connection_manager.h"

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::sco {
namespace {

using OpenConnectionResult = ScoConnectionManager::OpenConnectionResult;
using AcceptConnectionResult = ScoConnectionManager::AcceptConnectionResult;

constexpr hci::ConnectionHandle kAclConnectionHandle = 0x40;
constexpr hci::ConnectionHandle kScoConnectionHandle = 0x41;
const DeviceAddress kLocalAddress(DeviceAddress::Type::kBREDR,
                                  {0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
const DeviceAddress kPeerAddress(DeviceAddress::Type::kBREDR, {0x00, 0x00, 0x00, 0x00, 0x00, 0x02});
constexpr hci::SynchronousConnectionParameters kConnectionParams{
    .transmit_bandwidth = 1,
    .receive_bandwidth = 2,
    .transmit_coding_format =
        hci::VendorCodingFormat{
            .coding_format = hci::CodingFormat::kMSbc,
            .company_id = 3,
            .vendor_codec_id = 4,
        },
    .receive_coding_format =
        hci::VendorCodingFormat{
            .coding_format = hci::CodingFormat::kCvsd,
            .company_id = 5,
            .vendor_codec_id = 6,
        },
    .transmit_codec_frame_size_bytes = 7,
    .receive_codec_frame_size_bytes = 8,
    .input_bandwidth = 9,
    .output_bandwidth = 10,
    .input_coding_format =
        hci::VendorCodingFormat{
            .coding_format = hci::CodingFormat::kALaw,
            .company_id = 11,
            .vendor_codec_id = 12,
        },
    .output_coding_format =
        hci::VendorCodingFormat{
            .coding_format = hci::CodingFormat::kLinearPcm,
            .company_id = 13,
            .vendor_codec_id = 14,
        },
    .input_coded_data_size_bits = 15,
    .output_coded_data_size_bits = 16,
    .input_pcm_data_format = hci::PcmDataFormat::k1sComplement,
    .output_pcm_data_format = hci::PcmDataFormat::k2sComplement,
    .input_pcm_sample_payload_msb_position = 17,
    .output_pcm_sample_payload_msb_position = 18,
    .input_data_path = hci::ScoDataPath::kAudioTestMode,
    .output_data_path = hci::ScoDataPath::kHci,
    .input_transport_unit_size_bits = 19,
    .output_transport_unit_size_bits = 20,
    .max_latency_ms = 21,
    .packet_types = 0x003F,  // All packet types
    .retransmission_effort = hci::ScoRetransmissionEffort::kQualityOptimized,
};

constexpr hci::SynchronousConnectionParameters ScoConnectionParams() {
  auto params = kConnectionParams;
  params.packet_types = params.packet_types = static_cast<uint16_t>(hci::ScoPacketTypeBits::kHv3);
  return params;
}

constexpr hci::SynchronousConnectionParameters EscoConnectionParams() {
  auto params = kConnectionParams;
  params.packet_types = params.packet_types = static_cast<uint16_t>(hci::ScoPacketTypeBits::kEv3);
  return params;
}

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class ScoConnectionManagerTest : public TestingBase {
 public:
  ScoConnectionManagerTest() = default;
  ~ScoConnectionManagerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();
    StartTestDevice();

    manager_ = std::make_unique<ScoConnectionManager>(PeerId(1), kAclConnectionHandle, kPeerAddress,
                                                      kLocalAddress, transport()->WeakPtr());
  }

  void TearDown() override {
    manager_.reset();
    RunLoopUntilIdle();
    TestingBase::TearDown();
  }

  void DestroyManager() { manager_.reset(); }

  ScoConnectionManager* manager() const { return manager_.get(); }

 private:
  std::unique_ptr<ScoConnectionManager> manager_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ScoConnectionManagerTest);
};

TEST_F(ScoConnectionManagerTest, OpenConnectionSuccess) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto result) { conn_result = std::move(result); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  ASSERT_TRUE(conn_result.value());
  EXPECT_EQ(conn_result.value()->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  conn_result.value()->Close();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, OpenConnectionAndReceiveFailureStatusEvent) {
  auto setup_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedSetupSynchronousConnection, hci::StatusCode::kConnectionLimitExceeded);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn;
  auto conn_cb = [&conn](auto cb_result) { conn = std::move(cb_result); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn.is_error());
  EXPECT_EQ(conn.error(), HostError::kFailed);
}

TEST_F(ScoConnectionManagerTest, OpenConnectionAndReceiveFailureCompleteEvent) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kConnectionFailedToBeEstablished);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn;
  auto conn_cb = [&conn](auto cb_result) { conn = std::move(cb_result); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn.is_error());
  EXPECT_EQ(conn.error(), HostError::kFailed);
}

TEST_F(ScoConnectionManagerTest,
       AcceptConnectionCompleteEventErrorAndResultCallbackDestroysRequestHandle) {
  AcceptConnectionResult conn;
  std::optional<ScoConnectionManager::RequestHandle> req_handle;
  auto conn_cb = [&conn, &req_handle](auto cb_result) {
    req_handle.reset();
    conn = std::move(cb_result);
  };

  req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kUnspecifiedError);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kConnectionAcceptTimeoutExceeded);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet, &conn_complete_packet);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn.is_error());
  EXPECT_EQ(conn.error(), HostError::kParametersRejected);
}

TEST_F(ScoConnectionManagerTest, IgnoreWrongAddressInConnectionComplete) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  const DeviceAddress kWrongPeerAddress(DeviceAddress::Type::kBREDR,
                                        {0x00, 0x00, 0x00, 0x00, 0x00, 0x05});
  auto conn_complete_packet_wrong = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kWrongPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet_wrong);

  OpenConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto result) { conn_result = std::move(result); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  // Ensure subsequent correct complete packet completes request.
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, UnexpectedConnectionCompleteDisconnectsConnection) {
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  test_device()->SendCommandChannelPacket(conn_complete_packet);
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, DestroyingManagerClosesConnections) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn_result;
  auto conn_cb = [&conn_result](OpenConnectionResult result) { conn_result = std::move(result); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_ok());

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  DestroyManager();
  RunLoopUntilIdle();
  // Ref should still be valid.
  EXPECT_TRUE(conn_result.value());
}

TEST_F(ScoConnectionManagerTest, QueueThreeRequestsCancelsSecond) {
  const hci::ConnectionHandle handle_0 = kScoConnectionHandle;
  const hci::ConnectionHandle handle_1 = handle_0 + 1;
  const hci::ConnectionHandle handle_2 = handle_1 + 1;

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1](auto cb_result) { conn_result_1 = std::move(cb_result); };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  OpenConnectionResult conn_result_2;
  auto conn_cb_2 = [&conn_result_2](auto cb_conn) { conn_result_2 = std::move(cb_conn); };
  auto req_handle_2 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_2));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_0.is_pending());
  EXPECT_TRUE(conn_result_2.is_pending());
  ASSERT_TRUE(conn_result_1.is_error());
  EXPECT_EQ(conn_result_1.error(), HostError::kCanceled);

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  auto conn_complete_packet_0 = testing::SynchronousConnectionCompletePacket(
      handle_0, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_0);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_0.is_ok());
  EXPECT_TRUE(conn_result_2.is_pending());

  auto conn_complete_packet_2 = testing::SynchronousConnectionCompletePacket(
      handle_2, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_2);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_2.is_ok());

  // Send status and complete events so second disconnect command isn't queued in CommandChannel.
  auto disconn_status_packet_0 =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete_0 = testing::DisconnectionCompletePacket(handle_0);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_0),
                        &disconn_status_packet_0, &disconn_complete_0);
  conn_result_0.value()->Deactivate();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_2));
  conn_result_2.value()->Deactivate();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, HandleReuse) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };

  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, conn_cb);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  auto conn = conn_result.take_value();
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  auto disconn_status_packet =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete = testing::DisconnectionCompletePacket(kScoConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle),
                        &disconn_status_packet, &disconn_complete);
  conn->Deactivate();
  conn_result = fpromise::pending();

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, conn_cb);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value()->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionSuccess) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) {
    EXPECT_TRUE(cb_conn.is_ok());
    conn_result = std::move(cb_conn);
  };
  auto req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value().first->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionAndReceiveStatusAndCompleteEventWithErrors) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };

  auto req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet =
      testing::CommandStatusPacket(hci::kEnhancedAcceptSynchronousConnectionRequest,
                                   hci::StatusCode::kInvalidHCICommandParameters);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO,
      hci::StatusCode::kConnectionAcceptTimeoutExceeded));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kParametersRejected);
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionAndReceiveCompleteEventWithFailureStatus) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };

  auto req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO,
      hci::StatusCode::kConnectionFailedToBeEstablished));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kParametersRejected);
}

TEST_F(ScoConnectionManagerTest, RejectInboundRequestWhileInitiatorRequestPending) {
  size_t conn_cb_count = 0;
  auto conn_cb = [&conn_cb_count](auto cb_conn) { conn_cb_count++; };

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 0u);
  // Destroy manager so that callback gets called before callback reference is invalid.
  DestroyManager();
  EXPECT_EQ(conn_cb_count, 1u);
}

TEST_F(ScoConnectionManagerTest, RejectInboundRequestWhenNoRequestsPending) {
  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, IgnoreInboundAclRequest) {
  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kACL);
  test_device()->SendCommandChannelPacket(conn_req_packet);
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, IgnoreInboundRequestWrongPeerAddress) {
  const DeviceAddress address(DeviceAddress::Type::kBREDR, {0x00, 0x00, 0x00, 0x00, 0x00, 0x05});
  auto conn_req_packet = testing::ConnectionRequestPacket(address, hci::LinkType::kACL);
  test_device()->SendCommandChannelPacket(conn_req_packet);
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, QueueTwoAcceptConnectionRequestsCancelsFirstRequest) {
  AcceptConnectionResult conn_result_0;
  auto conn_cb = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->AcceptConnection({kConnectionParams}, conn_cb);

  auto second_conn_params = kConnectionParams;
  second_conn_params.transmit_bandwidth = 99;

  AcceptConnectionResult conn_result_1;
  auto req_handle_1 = manager()->AcceptConnection(
      {second_conn_params}, [&conn_result_1](auto cb_conn) { conn_result_1 = std::move(cb_conn); });

  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kCanceled);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, second_conn_params),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_1.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_1.is_ok());
  EXPECT_EQ(conn_result_1.value().first->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(
    ScoConnectionManagerTest,
    QueueTwoAcceptConnectionRequestsCancelsFirstRequestAndFirstRequestCallbackDestroysRequestHandle) {
  std::optional<ScoConnectionManager::RequestHandle> req_handle_0;
  AcceptConnectionResult conn_result_0;
  auto conn_cb = [&conn_result_0, &req_handle_0](auto cb_conn) {
    conn_result_0 = std::move(cb_conn);
    req_handle_0.reset();
  };
  req_handle_0 = manager()->AcceptConnection({kConnectionParams}, conn_cb);

  auto second_conn_params = kConnectionParams;
  second_conn_params.transmit_bandwidth = 99;

  AcceptConnectionResult conn_result_1;
  auto req_handle_1 = manager()->AcceptConnection(
      {second_conn_params}, [&conn_result_1](auto cb_conn) { conn_result_1 = std::move(cb_conn); });

  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kCanceled);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, second_conn_params),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_1.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_1.is_ok());
  EXPECT_EQ(conn_result_1.value().first->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, QueueSecondAcceptRequestAfterFirstRequestReceivesEvent) {
  AcceptConnectionResult conn_result_0;
  auto req_handle_0 = manager()->AcceptConnection(
      {kConnectionParams}, [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); });

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::EnhancedAcceptSynchronousConnectionRequestPacket(
                                           kPeerAddress, kConnectionParams));
  RunLoopUntilIdle();

  AcceptConnectionResult conn_result_1;
  auto req_handle_1 = manager()->AcceptConnection(
      {kConnectionParams}, [&conn_result_1](auto cb_conn) { conn_result_1 = std::move(cb_conn); });

  // First request should not be cancelled because a request event was received.
  EXPECT_TRUE(conn_result_0.is_pending());

  // Send failure events to fail first request.
  test_device()->SendCommandChannelPacket(testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kCommandDisallowed));
  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO,
      hci::StatusCode::kConnectionAcceptTimeoutExceeded));
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kParametersRejected);

  // Second request should now be in progress.
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_1.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_1.is_ok());
  EXPECT_EQ(conn_result_1.value().first->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, RequestsCancelledOnManagerDestruction) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1](auto cb_conn) { conn_result_1 = std::move(cb_conn); };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();

  DestroyManager();
  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kCanceled);
  ASSERT_TRUE(conn_result_1.is_error());
  EXPECT_EQ(conn_result_1.error(), HostError::kCanceled);
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionExplicitlyCancelledByClient) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));

  req_handle.Cancel();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kCanceled);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kCanceled);
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionCancelledByClientDestroyingHandle) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };

  // req_handle destroyed at end of scope
  { auto req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb)); }
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kCanceled);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kCanceled);
}

TEST_F(ScoConnectionManagerTest, OpenConnectionCantBeCancelledOnceInProgress) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));
  req_handle.Cancel();

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());
  test_device()->SendCommandChannelPacket(conn_complete_packet);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value()->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  conn_result.value()->Close();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest, QueueTwoRequestsAndCancelSecond) {
  const hci::ConnectionHandle handle_0 = kScoConnectionHandle;

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  size_t cb_count_1 = 0;
  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&cb_count_1, &conn_result_1](auto cb_conn) {
    cb_count_1++;
    conn_result_1 = std::move(cb_conn);
  };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_0.is_pending());

  req_handle_1.Cancel();
  EXPECT_EQ(cb_count_1, 1u);
  ASSERT_TRUE(conn_result_1.is_error());
  EXPECT_EQ(conn_result_1.error(), HostError::kCanceled);

  auto conn_complete_packet_0 = testing::SynchronousConnectionCompletePacket(
      handle_0, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_0);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_0.is_ok());
  EXPECT_EQ(cb_count_1, 1u);

  auto disconn_status_packet_0 =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete_0 = testing::DisconnectionCompletePacket(handle_0);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_0),
                        &disconn_status_packet_0, &disconn_complete_0);
  conn_result_0.value()->Deactivate();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest,
       QueueingThreeRequestsCancelsSecondAndRequestHandleDestroyedInResultCallback) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  std::optional<ScoConnectionManager::RequestHandle> req_handle_1;
  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1, &req_handle_1](auto cb_conn) {
    req_handle_1.reset();
    conn_result_1 = std::move(cb_conn);
  };
  req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  OpenConnectionResult conn_result_2;
  auto conn_cb_2 = [&conn_result_2](auto cb_conn) { conn_result_2 = std::move(cb_conn); };
  auto req_handle_2 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_2));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_0.is_pending());
  EXPECT_TRUE(conn_result_2.is_pending());
  ASSERT_TRUE(conn_result_1.is_error());
  EXPECT_EQ(conn_result_1.error(), HostError::kCanceled);

  DestroyManager();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest,
       OpenConnectionFollowedByPeerDisconnectAndSecondOpenConnectonWithHandleReuse) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto result) { conn_result_0 = std::move(result); };

  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_0.is_ok());
  ASSERT_TRUE(conn_result_0.value());
  EXPECT_EQ(conn_result_0.value()->handle(), kScoConnectionHandle);

  test_device()->SendCommandChannelPacket(testing::DisconnectionCompletePacket(
      kScoConnectionHandle, hci::StatusCode::kRemoteUserTerminatedConnection));
  RunLoopUntilIdle();

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1](auto result) { conn_result_1 = std::move(result); };

  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_1.is_ok());
  ASSERT_TRUE(conn_result_1.value());
  EXPECT_EQ(conn_result_1.value()->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  conn_result_1.value()->Close();
  RunLoopUntilIdle();
}

TEST_F(ScoConnectionManagerTest,
       DestroyManagerWhileResponderRequestInProgressAndDestroyRequestHandleInResultCallback) {
  AcceptConnectionResult conn;
  std::optional<ScoConnectionManager::RequestHandle> req_handle;
  auto conn_cb = [&conn, &req_handle](auto cb_result) {
    req_handle.reset();
    conn = std::move(cb_result);
  };

  req_handle = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb));
  RunLoopUntilIdle();

  DestroyManager();
  RunLoopUntilIdle();

  ASSERT_TRUE(conn.is_error());
  EXPECT_EQ(conn.error(), HostError::kCanceled);
}

TEST_F(ScoConnectionManagerTest,
       DestroyManagerWhileInitiatorRequestQueuedAndDestroyRequestHandleInResultCallback) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  OpenConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  std::optional<ScoConnectionManager::RequestHandle> req_handle_1;
  OpenConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1, &req_handle_1](auto cb_conn) {
    req_handle_1.reset();
    conn_result_1 = std::move(cb_conn);
  };
  req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_0.is_pending());

  DestroyManager();
  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kCanceled);
  ASSERT_TRUE(conn_result_1.is_error());
  EXPECT_EQ(conn_result_1.error(), HostError::kCanceled);
}

TEST_F(ScoConnectionManagerTest, AcceptConnectionFirstParametersRejectedSecondParametersAccepted) {
  hci::SynchronousConnectionParameters esco_params_0 = kConnectionParams;
  esco_params_0.packet_types = static_cast<uint16_t>(hci::ScoPacketTypeBits::kEv3);
  hci::SynchronousConnectionParameters esco_params_1 = kConnectionParams;
  esco_params_1.packet_types = static_cast<uint16_t>(hci::ScoPacketTypeBits::kEv4);

  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection({esco_params_0, esco_params_1}, std::move(conn_cb));

  auto conn_req_packet_0 =
      testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kExtendedSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet_0);

  auto accept_status_packet_0 = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, esco_params_0),
      &accept_status_packet_0);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kUnsupportedFeatureOrParameter));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  auto conn_req_packet_1 =
      testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kExtendedSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet_1);

  auto accept_status_packet_1 = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, esco_params_1),
      &accept_status_packet_1);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value().first->handle(), kScoConnectionHandle);
  size_t result_parameter_index = conn_result.value().second;
  EXPECT_EQ(result_parameter_index, 1u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(
    ScoConnectionManagerTest,
    AcceptScoConnectionWithFirstParametersEscoPacketTypeAndSecondScoPacketTypeSkipsToSecondParameters) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection({EscoConnectionParams(), ScoConnectionParams()},
                                                std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::EnhancedAcceptSynchronousConnectionRequestPacket(
                            kPeerAddress, ScoConnectionParams()),
                        &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value().first->handle(), kScoConnectionHandle);
  size_t result_parameter_index = conn_result.value().second;
  EXPECT_EQ(result_parameter_index, 1u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(
    ScoConnectionManagerTest,
    AcceptEscoConnectionWithFirstParametersScoPacketTypeAndSecondEscoPacketTypeSkipsToSecondParameters) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection({ScoConnectionParams(), EscoConnectionParams()},
                                                std::move(conn_cb));

  auto conn_req_packet =
      testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kExtendedSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::EnhancedAcceptSynchronousConnectionRequestPacket(
                            kPeerAddress, EscoConnectionParams()),
                        &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_ok());
  EXPECT_EQ(conn_result.value().first->handle(), kScoConnectionHandle);
  size_t result_parameter_index = conn_result.value().second;
  EXPECT_EQ(result_parameter_index, 1u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(ScoConnectionManagerTest, AcceptScoConnectionWithEscoParametersFailsAndSendsRejectCommand) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection({EscoConnectionParams()}, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kUnacceptableConnectionParameters),
                        &reject_status_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kParametersRejected);
}

TEST_F(ScoConnectionManagerTest, AcceptScoConnectionWithEmptyParametersFails) {
  AcceptConnectionResult conn_result;
  auto conn_cb = [&conn_result](auto cb_conn) { conn_result = std::move(cb_conn); };
  auto req_handle = manager()->AcceptConnection(/*parameters=*/{}, std::move(conn_cb));
  ASSERT_TRUE(conn_result.is_error());
  EXPECT_EQ(conn_result.error(), HostError::kInvalidParameters);
}

TEST_F(
    ScoConnectionManagerTest,
    QueuedRequestAfterAcceptConnectionCommandCancelsNextAcceptConnectionParameterAttemptWhenThereAreMultipleParameters) {
  AcceptConnectionResult conn_result_0;
  auto conn_cb_0 = [&conn_result_0](auto cb_conn) { conn_result_0 = std::move(cb_conn); };

  // Queue an accept request with 2 parameters. The first parameters should fail and the second
  // should never be used due to the second request canceling the first request.
  auto req_handle_0 =
      manager()->AcceptConnection({kConnectionParams, kConnectionParams}, std::move(conn_cb_0));

  auto conn_req_packet_0 =
      testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kExtendedSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet_0);

  auto accept_status_packet_0 = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet_0);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_0.is_pending());

  // Second request should cancel first request when connection complete event is received.
  AcceptConnectionResult conn_result_1;
  auto conn_cb_1 = [&conn_result_1](auto cb_conn) { conn_result_1 = std::move(cb_conn); };
  auto req_handle_1 = manager()->AcceptConnection({kConnectionParams}, std::move(conn_cb_1));
  EXPECT_TRUE(conn_result_0.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kUnsupportedFeatureOrParameter));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_0.is_error());
  EXPECT_EQ(conn_result_0.error(), HostError::kCanceled);
  EXPECT_TRUE(conn_result_1.is_pending());

  // Complete the second accept request with an incoming connection.
  auto conn_req_packet_1 =
      testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kExtendedSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet_1);
  auto accept_status_packet_1 = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet_1);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_result_1.is_pending());

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn_result_1.is_ok());
  EXPECT_EQ(conn_result_1.value().first->handle(), kScoConnectionHandle);
  size_t result_parameter_index = conn_result_1.value().second;
  EXPECT_EQ(result_parameter_index, 0u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

}  // namespace
}  // namespace bt::sco
