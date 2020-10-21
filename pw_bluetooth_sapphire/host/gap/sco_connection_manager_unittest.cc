// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "sco_connection_manager.h"

#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::gap {
namespace {

const PeerId kPeerId(1u);
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
    .packet_types = 22,
    .retransmission_effort = hci::ScoRetransmissionEffort::kQualityOptimized,
};

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class ScoConnectionManagerTest : public TestingBase {
 public:
  ScoConnectionManagerTest() = default;
  ~ScoConnectionManagerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();
    StartTestDevice();

    manager_ = std::make_unique<ScoConnectionManager>(kPeerId, kAclConnectionHandle, kPeerAddress,
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

using GAP_ScoConnectionManagerTest = ScoConnectionManagerTest;

TEST_F(GAP_ScoConnectionManagerTest, OpenConnectionSuccess) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) { conn = std::move(cb_conn); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  conn->Close();
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, OpenConnectionAndReceiveFailureStatusEvent) {
  auto setup_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedSetupSynchronousConnection, hci::StatusCode::kConnectionLimitExceeded);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  size_t conn_cb_count = 0u;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    conn_cb_count++;
    EXPECT_FALSE(cb_conn);
  };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, OpenConnectionAndReceiveFailureCompleteEvent) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO,
      hci::StatusCode::kConnectionFailedToBeEstablished);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  size_t conn_cb_count = 0u;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    conn_cb_count++;
    EXPECT_FALSE(cb_conn);
  };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, IgnoreWrongAddressInConnectionComplete) {
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

  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) { conn = std::move(cb_conn); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_FALSE(conn);

  // Ensure subsequent correct complete packet completes request.
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(GAP_ScoConnectionManagerTest, UnexpectedConnectionCompleteDisconnectsConnection) {
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  test_device()->SendCommandChannelPacket(conn_complete_packet);
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, DestroyingManagerClosesConnections) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) { conn = std::move(cb_conn); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  DestroyManager();
  RunLoopUntilIdle();
  // Ref should still be valid.
  EXPECT_TRUE(conn);
}

TEST_F(GAP_ScoConnectionManagerTest, QueueThreeRequestsCancelsSecond) {
  const hci::ConnectionHandle handle_0 = kScoConnectionHandle;
  const hci::ConnectionHandle handle_1 = handle_0 + 1;
  const hci::ConnectionHandle handle_2 = handle_1 + 1;

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  fbl::RefPtr<ScoConnection> conn_0;
  auto conn_cb_0 = [&conn_0](fbl::RefPtr<ScoConnection> cb_conn) { conn_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  size_t cb_count_1 = 0;
  auto conn_cb_1 = [&cb_count_1](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    cb_count_1++;
  };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  fbl::RefPtr<ScoConnection> conn_2;
  auto conn_cb_2 = [&conn_2](fbl::RefPtr<ScoConnection> cb_conn) { conn_2 = std::move(cb_conn); };
  auto req_handle_2 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_2));

  RunLoopUntilIdle();
  EXPECT_FALSE(conn_0);
  EXPECT_EQ(cb_count_1, 1u);
  EXPECT_FALSE(conn_2);

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  auto conn_complete_packet_0 = testing::SynchronousConnectionCompletePacket(
      handle_0, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_0);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_0);
  EXPECT_FALSE(conn_2);

  auto conn_complete_packet_2 = testing::SynchronousConnectionCompletePacket(
      handle_2, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_2);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_2);

  // Send status and complete events so second disconnect command isn't queued in CommandChannel.
  auto disconn_status_packet_0 =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete_0 = testing::DisconnectionCompletePacket(handle_0);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_0),
                        &disconn_status_packet_0, &disconn_complete_0);
  conn_0->Deactivate();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_2));
  conn_2->Deactivate();
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, HandleReuse) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) { conn = std::move(cb_conn); };

  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, conn_cb);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  auto disconn_status_packet =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete = testing::DisconnectionCompletePacket(kScoConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle),
                        &disconn_status_packet, &disconn_complete);
  conn->Deactivate();
  conn.reset();

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet, &conn_complete_packet);

  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, conn_cb);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(GAP_ScoConnectionManagerTest, AcceptConnectionSuccess) {
  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_TRUE(cb_conn);
    conn = std::move(cb_conn);
  };
  auto req_handle = manager()->AcceptConnection(kConnectionParams, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_FALSE(conn);

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(GAP_ScoConnectionManagerTest, AcceptConnectionStatusFailure) {
  size_t conn_cb_count = 0;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_cb_count++;
  };
  auto req_handle = manager()->AcceptConnection(kConnectionParams, std::move(conn_cb));
  EXPECT_EQ(conn_cb_count, 0u);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kConnectionLimitExceeded);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);

  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, AcceptConnectionAndReceiveCompleteEventWithFailureStatus) {
  size_t conn_cb_count = 0;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_cb_count++;
  };
  auto req_handle = manager()->AcceptConnection(kConnectionParams, std::move(conn_cb));

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 0u);

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO,
      hci::StatusCode::kConnectionFailedToBeEstablished));

  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, RejectInboundRequestWhileInitiatorRequestPending) {
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

TEST_F(GAP_ScoConnectionManagerTest, RejectInboundRequestWhenNoRequestsPending) {
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

TEST_F(GAP_ScoConnectionManagerTest, IgnoreInboundAclRequest) {
  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kACL);
  test_device()->SendCommandChannelPacket(conn_req_packet);
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, IgnoreInboundRequestWrongPeerAddress) {
  const DeviceAddress address(DeviceAddress::Type::kBREDR, {0x00, 0x00, 0x00, 0x00, 0x00, 0x05});
  auto conn_req_packet = testing::ConnectionRequestPacket(address, hci::LinkType::kACL);
  test_device()->SendCommandChannelPacket(conn_req_packet);
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, QueueTwoAcceptConnectionRequestsCancelsFirstRequest) {
  size_t conn_cb_count = 0;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_cb_count++;
  };
  auto req_handle_0 = manager()->AcceptConnection(kConnectionParams, conn_cb);

  auto second_conn_params = kConnectionParams;
  second_conn_params.transmit_bandwidth = 99;

  fbl::RefPtr<ScoConnection> conn;
  auto req_handle_1 = manager()->AcceptConnection(
      second_conn_params, [&conn](auto cb_conn) { conn = std::move(cb_conn); });

  EXPECT_EQ(conn_cb_count, 1u);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, second_conn_params),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_FALSE(conn);

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(GAP_ScoConnectionManagerTest, QueueSecondAcceptRequestAfterFirstRequestReceivesEvent) {
  size_t conn_cb_count = 0;
  auto conn_cb = [&conn_cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_cb_count++;
  };
  auto req_handle_0 = manager()->AcceptConnection(kConnectionParams, conn_cb);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::EnhancedAcceptSynchronousConnectionRequestPacket(
                                           kPeerAddress, kConnectionParams));
  RunLoopUntilIdle();

  fbl::RefPtr<ScoConnection> conn;
  auto req_handle_1 = manager()->AcceptConnection(
      kConnectionParams, [&conn](auto cb_conn) { conn = std::move(cb_conn); });

  // First request should not be cancelled because a request event was received.
  EXPECT_EQ(conn_cb_count, 0u);

  // Send failure status to fail first request.
  test_device()->SendCommandChannelPacket(testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kCommandDisallowed));
  RunLoopUntilIdle();
  EXPECT_EQ(conn_cb_count, 1u);

  // Second request should now be in progress.
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto accept_status_packet = testing::CommandStatusPacket(
      hci::kEnhancedAcceptSynchronousConnectionRequest, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedAcceptSynchronousConnectionRequestPacket(kPeerAddress, kConnectionParams),
      &accept_status_packet);
  RunLoopUntilIdle();
  EXPECT_FALSE(conn);

  test_device()->SendCommandChannelPacket(testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kSCO, hci::StatusCode::kSuccess));

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

TEST_F(GAP_ScoConnectionManagerTest, RequestsCancelledOnManagerDestruction) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  size_t conn_0_cb_count = 0;
  auto conn_cb_0 = [&conn_0_cb_count](auto cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_0_cb_count++;
  };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  size_t conn_1_cb_count = 0;
  auto conn_cb_1 = [&conn_1_cb_count](auto cb_conn) {
    EXPECT_FALSE(cb_conn);
    conn_1_cb_count++;
  };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();

  DestroyManager();
  EXPECT_EQ(conn_0_cb_count, 1u);
  EXPECT_EQ(conn_1_cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, AcceptConnectionExplicitlyCancelledByClient) {
  size_t cb_count = 0;
  auto conn_cb = [&cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    cb_count++;
  };
  auto req_handle = manager()->AcceptConnection(kConnectionParams, std::move(conn_cb));

  req_handle.Cancel();
  EXPECT_EQ(cb_count, 1u);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, AcceptConnectionCancelledByClientDestroyingHandle) {
  size_t cb_count = 0;
  auto conn_cb = [&cb_count](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    cb_count++;
  };

  // req_handle destroyed at end of scope
  { auto req_handle = manager()->AcceptConnection(kConnectionParams, std::move(conn_cb)); }
  EXPECT_EQ(cb_count, 1u);

  auto conn_req_packet = testing::ConnectionRequestPacket(kPeerAddress, hci::LinkType::kSCO);
  test_device()->SendCommandChannelPacket(conn_req_packet);

  auto reject_status_packet = testing::CommandStatusPacket(hci::kRejectSynchronousConnectionRequest,
                                                           hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::RejectSynchronousConnectionRequest(
                            kPeerAddress, hci::StatusCode::kConnectionRejectedBadBdAddr),
                        &reject_status_packet);
  RunLoopUntilIdle();
  EXPECT_EQ(cb_count, 1u);
}

TEST_F(GAP_ScoConnectionManagerTest, OpenConnectionCantBeCancelledOnceInProgress) {
  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  auto conn_complete_packet = testing::SynchronousConnectionCompletePacket(
      kScoConnectionHandle, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  fbl::RefPtr<ScoConnection> conn;
  auto conn_cb = [&conn](fbl::RefPtr<ScoConnection> cb_conn) { conn = std::move(cb_conn); };

  auto req_handle = manager()->OpenConnection(kConnectionParams, std::move(conn_cb));
  req_handle.Cancel();

  RunLoopUntilIdle();
  EXPECT_FALSE(conn);
  test_device()->SendCommandChannelPacket(conn_complete_packet);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  conn->Close();
  RunLoopUntilIdle();
}

TEST_F(GAP_ScoConnectionManagerTest, QueueTwoRequestsAndCancelSecond) {
  const hci::ConnectionHandle handle_0 = kScoConnectionHandle;

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  fbl::RefPtr<ScoConnection> conn_0;
  auto conn_cb_0 = [&conn_0](fbl::RefPtr<ScoConnection> cb_conn) { conn_0 = std::move(cb_conn); };
  auto req_handle_0 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  size_t cb_count_1 = 0;
  auto conn_cb_1 = [&cb_count_1](fbl::RefPtr<ScoConnection> cb_conn) {
    EXPECT_FALSE(cb_conn);
    cb_count_1++;
  };
  auto req_handle_1 = manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();
  EXPECT_FALSE(conn_0);

  req_handle_1.Cancel();
  EXPECT_EQ(cb_count_1, 1u);

  auto conn_complete_packet_0 = testing::SynchronousConnectionCompletePacket(
      handle_0, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_0);
  RunLoopUntilIdle();
  ASSERT_TRUE(conn_0);
  EXPECT_EQ(cb_count_1, 1u);

  auto disconn_status_packet_0 =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete_0 = testing::DisconnectionCompletePacket(handle_0);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_0),
                        &disconn_status_packet_0, &disconn_complete_0);
  conn_0->Deactivate();
  RunLoopUntilIdle();
}

}  // namespace
}  // namespace bt::gap
