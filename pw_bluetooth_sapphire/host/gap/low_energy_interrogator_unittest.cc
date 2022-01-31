// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt::gap {

constexpr hci_spec::ConnectionHandle kConnectionHandle = 0x0BAA;
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kLERandom, {1});

using bt::testing::CommandTransaction;

const auto kReadRemoteVersionInfoRsp =
    testing::CommandStatusPacket(hci_spec::kReadRemoteVersionInfo, hci_spec::StatusCode::kSuccess);
const auto kLEReadRemoteFeaturesRsp =
    testing::CommandStatusPacket(hci_spec::kLEReadRemoteFeatures, hci_spec::StatusCode::kSuccess);

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class LowEnergyInterrogatorTest : public TestingBase {
 public:
  LowEnergyInterrogatorTest() = default;
  ~LowEnergyInterrogatorTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    peer_cache_ = std::make_unique<PeerCache>();
    interrogator_ =
        std::make_unique<LowEnergyInterrogator>(peer_cache_.get(), transport()->WeakPtr());

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
  void QueueSuccessfulInterrogation(hci_spec::ConnectionHandle conn,
                                    hci_spec::LESupportedFeatures features = {0}) const {
    const auto remote_version_complete_packet = testing::ReadRemoteVersionInfoCompletePacket(conn);
    const auto le_remote_features_complete_packet =
        testing::LEReadRemoteFeaturesCompletePacket(conn, features);

    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(conn),
                          &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(conn),
                          &kLEReadRemoteFeaturesRsp, &le_remote_features_complete_packet);
  }

  PeerCache* peer_cache() const { return peer_cache_.get(); }

  LowEnergyInterrogator* interrogator() const { return interrogator_.get(); }

 private:
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<LowEnergyInterrogator> interrogator_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogatorTest);
};

using GAP_LowEnergyInterrogatorTest = LowEnergyInterrogatorTest;

TEST_F(LowEnergyInterrogatorTest, SuccessfulInterrogation) {
  const hci_spec::LESupportedFeatures kFeatures{0x0123456789abcdef};
  QueueSuccessfulInterrogation(kConnectionHandle, kFeatures);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->version());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);

  EXPECT_TRUE(peer->version());
  ASSERT_TRUE(peer->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer->le()->features()->le_features);
}

TEST_F(LowEnergyInterrogatorTest, SuccessfulInterrogationPeerAlreadyHasLEFeatures) {
  const hci_spec::LESupportedFeatures kFeatures{0x0123456789abcdef};

  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());
  peer->MutLe().SetFeatures(kFeatures);
  EXPECT_TRUE(peer->le()->features());

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);
  ASSERT_TRUE(peer->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer->le()->features()->le_features);
}

TEST_F(LowEnergyInterrogatorTest, SuccessfulReinterrogation) {
  QueueSuccessfulInterrogation(kConnectionHandle);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);
  status = std::nullopt;

  // Remote version should always be read, even if already known.
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);

  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fitx::ok(), *status);
}

TEST_F(LowEnergyInterrogatorTest, LEReadRemoteFeaturesErrorStatus) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_read_remote_features_error_status_packet = testing::CommandStatusPacket(
      hci_spec::kLEReadRemoteFeatures, hci_spec::StatusCode::kUnknownCommand);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &le_read_remote_features_error_status_packet);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
  EXPECT_FALSE(peer->le()->features().has_value());
}

TEST_F(LowEnergyInterrogatorTest, PeerRemovedBeforeLEReadRemoteFeaturesComplete) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_remote_features_complete_packet = testing::LEReadRemoteFeaturesCompletePacket(
      kConnectionHandle, hci_spec::LESupportedFeatures{0});

  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  EXPECT_FALSE(status.has_value());

  ASSERT_TRUE(peer_cache()->RemoveDisconnectedPeer(peer->identifier()));

  test_device()->SendCommandChannelPacket(le_remote_features_complete_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
}

TEST_F(LowEnergyInterrogatorTest, ReadLERemoteFeaturesCallbackHandlesCanceledInterrogation) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_remote_features_complete_packet = testing::LEReadRemoteFeaturesCompletePacket(
      kConnectionHandle, hci_spec::LESupportedFeatures{0});

  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->version());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Result<>> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  EXPECT_FALSE(status.has_value());

  interrogator()->Cancel(peer->identifier());
  RunLoopUntilIdle();
  EXPECT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());

  test_device()->SendCommandChannelPacket(le_remote_features_complete_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
  // The read remote features handler should not update the features of a canceled interrogation.
  EXPECT_FALSE(peer->le()->features().has_value());
}

}  // namespace bt::gap
