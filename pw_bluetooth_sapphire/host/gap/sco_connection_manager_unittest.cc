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

  manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

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

  manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

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

  manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

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

  manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

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

  manager()->OpenConnection(kConnectionParams, std::move(conn_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(conn);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
  DestroyManager();
  RunLoopUntilIdle();
  // Ref should still be valid.
  EXPECT_TRUE(conn);
}

TEST_F(GAP_ScoConnectionManagerTest, QueueTwoRequests) {
  const hci::ConnectionHandle handle_0 = kScoConnectionHandle;
  const hci::ConnectionHandle handle_1 = handle_0 + 1;

  auto setup_status_packet = testing::CommandStatusPacket(hci::kEnhancedSetupSynchronousConnection,
                                                          hci::StatusCode::kSuccess);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  fbl::RefPtr<ScoConnection> conn_0;
  auto conn_cb_0 = [&conn_0](fbl::RefPtr<ScoConnection> cb_conn) { conn_0 = std::move(cb_conn); };
  manager()->OpenConnection(kConnectionParams, std::move(conn_cb_0));

  fbl::RefPtr<ScoConnection> conn_1;
  auto conn_cb_1 = [&conn_1](fbl::RefPtr<ScoConnection> cb_conn) { conn_1 = std::move(cb_conn); };
  manager()->OpenConnection(kConnectionParams, std::move(conn_cb_1));

  RunLoopUntilIdle();
  EXPECT_FALSE(conn_0);
  EXPECT_FALSE(conn_1);

  EXPECT_CMD_PACKET_OUT(
      test_device(),
      testing::EnhancedSetupSynchronousConnectionPacket(kAclConnectionHandle, kConnectionParams),
      &setup_status_packet);

  auto conn_complete_packet_0 = testing::SynchronousConnectionCompletePacket(
      handle_0, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_0);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_0);
  EXPECT_FALSE(conn_1);

  auto conn_complete_packet_1 = testing::SynchronousConnectionCompletePacket(
      handle_1, kPeerAddress, hci::LinkType::kExtendedSCO, hci::StatusCode::kSuccess);
  test_device()->SendCommandChannelPacket(conn_complete_packet_1);
  RunLoopUntilIdle();
  EXPECT_TRUE(conn_1);

  // Send status and complete events so second disconnect command isn't queued in CommandChannel.
  auto disconn_status_packet_0 =
      testing::CommandStatusPacket(hci::kDisconnect, hci::StatusCode::kSuccess);
  auto disconn_complete_0 = testing::DisconnectionCompletePacket(handle_0);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_0),
                        &disconn_status_packet_0, &disconn_complete_0);
  conn_0->Deactivate();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(handle_1));
  conn_1->Deactivate();
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

  manager()->OpenConnection(kConnectionParams, conn_cb);

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

  manager()->OpenConnection(kConnectionParams, conn_cb);

  RunLoopUntilIdle();
  ASSERT_TRUE(conn);
  EXPECT_EQ(conn->handle(), kScoConnectionHandle);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kScoConnectionHandle));
}

}  // namespace
}  // namespace bt::gap
