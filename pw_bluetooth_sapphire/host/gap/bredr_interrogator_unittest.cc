// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt::gap {

constexpr hci_spec::ConnectionHandle kConnectionHandle = 0x0BAA;
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kBREDR, {1});

const auto kRemoteNameRequestRsp =
    testing::CommandStatusPacket(hci_spec::kRemoteNameRequest, hci_spec::StatusCode::kSuccess);

const auto kReadRemoteVersionInfoRsp =
    testing::CommandStatusPacket(hci_spec::kReadRemoteVersionInfo, hci_spec::StatusCode::kSuccess);

const auto kReadRemoteSupportedFeaturesRsp = testing::CommandStatusPacket(
    hci_spec::kReadRemoteSupportedFeatures, hci_spec::StatusCode::kSuccess);

const auto kReadRemoteExtendedFeaturesRsp = testing::CommandStatusPacket(
    hci_spec::kReadRemoteExtendedFeatures, hci_spec::StatusCode::kSuccess);

using bt::testing::CommandTransaction;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class BrEdrInterrogatorTest : public TestingBase {
 public:
  BrEdrInterrogatorTest() = default;
  ~BrEdrInterrogatorTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    peer_cache_ = std::make_unique<PeerCache>();
    peer_ = peer_cache()->NewPeer(kTestDevAddr, /*connectable=*/true);
    EXPECT_FALSE(peer_->name());
    EXPECT_FALSE(peer_->version());
    EXPECT_FALSE(peer_->features().HasPage(0));
    EXPECT_FALSE(peer_->features().HasBit(0, hci_spec::LMPFeature::kExtendedFeatures));
    EXPECT_EQ(0u, peer_->features().last_page_number());

    interrogator_ = std::make_unique<BrEdrInterrogator>(peer_->GetWeakPtr(), kConnectionHandle,
                                                        transport()->WeakPtr());

    StartTestDevice();
  }

  void TearDown() override {
    RunLoopUntilIdle();
    test_device()->Stop();
    interrogator_ = nullptr;
    peer_cache_ = nullptr;
    TestingBase::TearDown();
  }

 protected:
  void QueueSuccessfulInterrogation(DeviceAddress addr, hci_spec::ConnectionHandle conn,
                                    bool extended_features = true) const {
    const DynamicByteBuffer remote_name_request_complete_packet =
        testing::RemoteNameRequestCompletePacket(addr);
    const DynamicByteBuffer remote_version_complete_packet =
        testing::ReadRemoteVersionInfoCompletePacket(conn);
    const DynamicByteBuffer remote_supported_complete_packet =
        testing::ReadRemoteSupportedFeaturesCompletePacket(conn, extended_features);

    EXPECT_CMD_PACKET_OUT(test_device(), testing::RemoteNameRequestPacket(addr),
                          &kRemoteNameRequestRsp, &remote_name_request_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(conn),
                          &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteSupportedFeaturesPacket(conn),
                          &kReadRemoteSupportedFeaturesRsp, &remote_supported_complete_packet);
    if (extended_features) {
      QueueSuccessfulReadRemoteExtendedFeatures(conn);
    }
  }

  void QueueSuccessfulReadRemoteExtendedFeatures(hci_spec::ConnectionHandle conn) const {
    const DynamicByteBuffer remote_extended1_complete_packet =
        testing::ReadRemoteExtended1CompletePacket(conn);
    const DynamicByteBuffer remote_extended2_complete_packet =
        testing::ReadRemoteExtended2CompletePacket(conn);

    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteExtended1Packet(conn),
                          &kReadRemoteExtendedFeaturesRsp, &remote_extended1_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteExtended2Packet(conn),
                          &kReadRemoteExtendedFeaturesRsp, &remote_extended2_complete_packet);
  }

  void DestroyInterrogator() { interrogator_.reset(); }

  Peer* peer() const { return peer_; }

  PeerCache* peer_cache() const { return peer_cache_.get(); }

  BrEdrInterrogator* interrogator() const { return interrogator_.get(); }

 private:
  Peer* peer_ = nullptr;
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<BrEdrInterrogator> interrogator_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrInterrogatorTest);
};

using GAP_BrEdrInterrogatorTest = BrEdrInterrogatorTest;

TEST_F(BrEdrInterrogatorTest, InterrogationFailsWithMalformedRemoteNameRequestComplete) {
  // Remote Name Request Complete event with insufficient length.
  const auto addr = kTestDevAddr.value().bytes();
  StaticByteBuffer remote_name_request_complete_packet(
      hci_spec::kRemoteNameRequestCompleteEventCode,
      0x08,                            // parameter_total_size (8)
      hci_spec::StatusCode::kSuccess,  // status
      addr[0], addr[1], addr[2], addr[3], addr[4],
      addr[5],  // peer address
      'F'       // remote name
  );
  EXPECT_CMD_PACKET_OUT(test_device(), testing::RemoteNameRequestPacket(kTestDevAddr),
                        &kRemoteNameRequestRsp, &remote_name_request_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle));
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::ReadRemoteSupportedFeaturesPacket(kConnectionHandle));

  hci::Result<> status = fitx::ok();
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  EXPECT_TRUE(status.is_error());
}

TEST_F(BrEdrInterrogatorTest, SuccessfulInterrogation) {
  QueueSuccessfulInterrogation(kTestDevAddr, kConnectionHandle);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);

  EXPECT_TRUE(peer()->name());
  EXPECT_TRUE(peer()->version());
  EXPECT_TRUE(peer()->features().HasPage(0));
  EXPECT_TRUE(peer()->features().HasBit(0, hci_spec::LMPFeature::kExtendedFeatures));
  EXPECT_EQ(2u, peer()->features().last_page_number());
}

TEST_F(BrEdrInterrogatorTest, SuccessfulReinterrogation) {
  QueueSuccessfulInterrogation(kTestDevAddr, kConnectionHandle);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);
  status = std::nullopt;

  QueueSuccessfulReadRemoteExtendedFeatures(kConnectionHandle);
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);
}

TEST_F(BrEdrInterrogatorTest, InterrogationFailedToGetName) {
  const DynamicByteBuffer remote_name_request_failure_rsp = testing::CommandStatusPacket(
      hci_spec::kRemoteNameRequest, hci_spec::StatusCode::kUnspecifiedError);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::RemoteNameRequestPacket(kTestDevAddr),
                        &remote_name_request_failure_rsp);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle));
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::ReadRemoteSupportedFeaturesPacket(kConnectionHandle));

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
}

TEST_F(BrEdrInterrogatorTest, Cancel) {
  QueueSuccessfulInterrogation(kTestDevAddr, kConnectionHandle, /*extended_features=*/false);

  std::optional<hci::Result<>> result;
  interrogator()->Start([&](hci::Result<> status) { result = status; });
  EXPECT_FALSE(result.has_value());

  interrogator()->Cancel();

  // The result callback should be called synchronously.
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), ToResult(HostError::kCanceled));
  result.reset();

  // Events should be ignored.
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());
}

TEST_F(BrEdrInterrogatorTest, InterrogatorDestroyedInCompleteCallback) {
  QueueSuccessfulInterrogation(kTestDevAddr, kConnectionHandle);

  std::optional<hci::Result<>> status;
  interrogator()->Start([this, &status](hci::Result<> cb_status) {
    status = cb_status;
    DestroyInterrogator();
  });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_ok());

  EXPECT_TRUE(peer()->name());
  EXPECT_TRUE(peer()->version());
  EXPECT_TRUE(peer()->features().HasPage(0));
  EXPECT_TRUE(peer()->features().HasBit(0, hci_spec::LMPFeature::kExtendedFeatures));
  EXPECT_EQ(2u, peer()->features().last_page_number());
}
}  // namespace bt::gap
