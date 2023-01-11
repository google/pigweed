// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"

namespace bt::gap {

constexpr hci_spec::ConnectionHandle kConnectionHandle = 0x0BAA;
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kLERandom, {1});

const auto kReadRemoteVersionInfoRsp = testing::CommandStatusPacket(
    hci_spec::kReadRemoteVersionInfo, pw::bluetooth::emboss::StatusCode::SUCCESS);
const auto kLEReadRemoteFeaturesRsp = testing::CommandStatusPacket(
    hci_spec::kLEReadRemoteFeatures, pw::bluetooth::emboss::StatusCode::SUCCESS);

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class LowEnergyInterrogatorTest : public TestingBase {
 public:
  LowEnergyInterrogatorTest() = default;
  ~LowEnergyInterrogatorTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    peer_cache_ = std::make_unique<PeerCache>();

    peer_ = peer_cache()->NewPeer(kTestDevAddr, /*connectable=*/true);
    ASSERT_TRUE(peer_->le());
    EXPECT_FALSE(peer_->version());
    EXPECT_FALSE(peer_->le()->features());

    interrogator_ = std::make_unique<LowEnergyInterrogator>(peer_->GetWeakPtr(), kConnectionHandle,
                                                            cmd_channel()->AsWeakPtr());
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

  void DestroyInterrogator() { interrogator_.reset(); }

  Peer* peer() const { return peer_; }

  PeerCache* peer_cache() const { return peer_cache_.get(); }

  LowEnergyInterrogator* interrogator() const { return interrogator_.get(); }

 private:
  Peer* peer_ = nullptr;
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<LowEnergyInterrogator> interrogator_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogatorTest);
};

using GAP_LowEnergyInterrogatorTest = LowEnergyInterrogatorTest;

TEST_F(LowEnergyInterrogatorTest, SuccessfulInterrogation) {
  const hci_spec::LESupportedFeatures kFeatures{0x0123456789abcdef};
  QueueSuccessfulInterrogation(kConnectionHandle, kFeatures);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fit::ok(), *status);

  EXPECT_TRUE(peer()->version());
  ASSERT_TRUE(peer()->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer()->le()->features()->le_features);
}

TEST_F(LowEnergyInterrogatorTest, SuccessfulInterrogationPeerAlreadyHasLEFeatures) {
  const hci_spec::LESupportedFeatures kFeatures{0x0123456789abcdef};

  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);

  peer()->MutLe().SetFeatures(kFeatures);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fit::ok(), *status);
  ASSERT_TRUE(peer()->le()->features());
  EXPECT_EQ(kFeatures.le_features, peer()->le()->features()->le_features);
}

TEST_F(LowEnergyInterrogatorTest, SuccessfulReinterrogation) {
  QueueSuccessfulInterrogation(kConnectionHandle);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();

  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fit::ok(), *status);
  status = std::nullopt;

  // Remote version should always be read, even if already known.
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);

  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(fit::ok(), *status);
}

TEST_F(LowEnergyInterrogatorTest, LEReadRemoteFeaturesErrorStatus) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_read_remote_features_error_status_packet = testing::CommandStatusPacket(
      hci_spec::kLEReadRemoteFeatures, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &le_read_remote_features_error_status_packet);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
  EXPECT_FALSE(peer()->le()->features().has_value());
}

TEST_F(LowEnergyInterrogatorTest, ReadRemoteVersionErrorStatus) {
  const auto remote_version_error_status_packet = testing::CommandStatusPacket(
      hci_spec::kReadRemoteVersionInfo, pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
  const auto le_remote_features_complete_packet =
      testing::LEReadRemoteFeaturesCompletePacket(kConnectionHandle, /*features=*/{0});
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &remote_version_error_status_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp, &le_remote_features_complete_packet);

  std::optional<hci::Result<>> status;
  interrogator()->Start([&status](hci::Result<> cb_status) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_FALSE(status->is_ok());
  EXPECT_FALSE(peer()->version());
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

  std::optional<hci::Result<>> result;
  interrogator()->Start([&result](hci::Result<> cb_result) { result = cb_result; });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  interrogator()->Cancel();
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_error());
  EXPECT_EQ(result.value(), ToResult(HostError::kCanceled));
  result.reset();

  test_device()->SendCommandChannelPacket(le_remote_features_complete_packet);
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());
  // The read remote features handler should not update the features of a canceled interrogation.
  EXPECT_FALSE(peer()->le()->features().has_value());
}

TEST_F(LowEnergyInterrogatorTest, ReadRemoteVersionCallbackHandlesCanceledInterrogation) {
  const auto remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const auto le_remote_features_complete_packet = testing::LEReadRemoteFeaturesCompletePacket(
      kConnectionHandle, hci_spec::LESupportedFeatures{0});

  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::LEReadRemoteFeaturesPacket(kConnectionHandle),
                        &kLEReadRemoteFeaturesRsp, &le_remote_features_complete_packet);

  std::optional<hci::Result<>> result;
  interrogator()->Start([&result](hci::Result<> cb_result) { result = cb_result; });
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());

  interrogator()->Cancel();
  RunLoopUntilIdle();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_error());
  EXPECT_EQ(result.value(), ToResult(HostError::kCanceled));
  result.reset();

  test_device()->SendCommandChannelPacket(remote_version_complete_packet);
  RunLoopUntilIdle();
  EXPECT_FALSE(result.has_value());
  // The read remote version handler should not update the version after a canceled interrogation.
  EXPECT_FALSE(peer()->version());
}

TEST_F(LowEnergyInterrogatorTest, DestroyInterrogatorInCompleteCallback) {
  const hci_spec::LESupportedFeatures kFeatures{0x0123456789abcdef};
  QueueSuccessfulInterrogation(kConnectionHandle, kFeatures);

  std::optional<hci::Result<>> status;
  interrogator()->Start([this, &status](hci::Result<> cb_status) {
    status = cb_status;
    DestroyInterrogator();
  });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_ok());
}

}  // namespace bt::gap
