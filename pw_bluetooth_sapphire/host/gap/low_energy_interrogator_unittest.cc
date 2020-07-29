// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/status.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::gap {

constexpr hci::ConnectionHandle kConnectionHandle = 0x0BAA;
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kLERandom, {1});

using bt::testing::CommandTransaction;

const auto kReadRemoteVersionInfoRsp =
    testing::CommandStatusPacket(hci::kReadRemoteVersionInfo, hci::StatusCode::kSuccess);
const auto kLEReadRemoteFeaturesRsp =
    testing::CommandStatusPacket(hci::kLEReadRemoteFeatures, hci::StatusCode::kSuccess);

using TestingBase = bt::testing::FakeControllerTest<bt::testing::TestController>;

class LowEnergyInterrogatorTest : public TestingBase {
 public:
  LowEnergyInterrogatorTest() = default;
  ~LowEnergyInterrogatorTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    peer_cache_ =
        std::make_unique<PeerCache>(inspector_.GetRoot().CreateChild(PeerCache::kInspectNodeName));
    interrogator_ = std::make_unique<LowEnergyInterrogator>(
        peer_cache_.get(), transport()->WeakPtr(), async_get_default_dispatcher());

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
  void QueueSuccessfulInterrogation(hci::ConnectionHandle conn,
                                    hci::LESupportedFeatures features = {0}) const {
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
  inspect::Inspector inspector_;
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<LowEnergyInterrogator> interrogator_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogatorTest);
};

using GAP_LowEnergyInterrogatorTest = LowEnergyInterrogatorTest;

TEST_F(GAP_LowEnergyInterrogatorTest, SuccessfulInterrogation) {
  const hci::LESupportedFeatures kFeatures{0x0123456789abcdef};
  QueueSuccessfulInterrogation(kConnectionHandle, kFeatures);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->version());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success());

  EXPECT_TRUE(peer->version());
  ASSERT_TRUE(peer->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer->le()->features()->le_features);
}

TEST_F(GAP_LowEnergyInterrogatorTest, SuccessfulInterrogationPeerAlreadyHasLEFeatures) {
  const hci::LESupportedFeatures kFeatures{0x0123456789abcdef};

  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());
  peer->MutLe().SetFeatures(kFeatures);
  EXPECT_TRUE(peer->le()->features());

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success());
  ASSERT_TRUE(peer->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer->le()->features()->le_features);
}

TEST_F(GAP_LowEnergyInterrogatorTest, SuccessfulReinterrogation) {
  QueueSuccessfulInterrogation(kConnectionHandle);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success());
  status = std::nullopt;

  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });

  // No commands should be sent on reinterrogation
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success());
}

TEST_F(GAP_LowEnergyInterrogatorTest, LEReadRemoteFeaturesErrorStatus) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_read_remote_features_error_status_packet =
      testing::CommandStatusPacket(hci::kLEReadRemoteFeatures, hci::StatusCode::kUnknownCommand);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &le_read_remote_features_error_status_packet);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_success());
  EXPECT_FALSE(peer->le()->features().has_value());
}

TEST_F(GAP_LowEnergyInterrogatorTest, PeerRemovedBeforeLEReadRemoteFeaturesComplete) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_remote_features_complete_packet =
      testing::LEReadRemoteFeaturesCompletePacket(kConnectionHandle, hci::LESupportedFeatures{0});

  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  EXPECT_FALSE(status.has_value());

  ASSERT_TRUE(peer_cache()->RemoveDisconnectedPeer(peer->identifier()));

  test_device()->SendCommandChannelPacket(le_remote_features_complete_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_success());
}

TEST_F(GAP_LowEnergyInterrogatorTest, ReadLERemoteFeaturesCallbackHandlesCanceledInterrogation) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_remote_features_complete_packet =
      testing::LEReadRemoteFeaturesCompletePacket(kConnectionHandle, hci::LESupportedFeatures{0});

  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp);

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  ASSERT_TRUE(peer->le());
  EXPECT_FALSE(peer->version());
  EXPECT_FALSE(peer->le()->features());

  std::optional<hci::Status> status;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&status](hci::Status cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  EXPECT_FALSE(status.has_value());

  interrogator()->Cancel(peer->identifier());
  RunLoopUntilIdle();
  EXPECT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_success());

  test_device()->SendCommandChannelPacket(le_remote_features_complete_packet);
  RunLoopUntilIdle();
  EXPECT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_success());
  // The read remote features handler should not update the features of a canceled interrogation.
  EXPECT_FALSE(peer->le()->features().has_value());
}

}  // namespace bt::gap
