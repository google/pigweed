// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/l2cap/logical_link.h"

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/connection.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/test_packets.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"
#include "pw_bluetooth_sapphire/internal/host/transport/link_type.h"

namespace bt::l2cap::internal {
namespace {
using Conn = hci::Connection;

using TestingBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

const hci_spec::ConnectionHandle kConnHandle = 0x0001;

class LogicalLinkTest : public TestingBase {
 public:
  LogicalLinkTest() = default;
  ~LogicalLinkTest() override = default;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LogicalLinkTest);

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();

    NewLogicalLink();
  }
  void TearDown() override {
    if (link_) {
      link_->Close();
      link_ = nullptr;
    }

    a2dp_offload_manager_ = nullptr;

    TestingBase::TearDown();
  }
  void NewLogicalLink(bt::LinkType type = bt::LinkType::kLE) {
    const size_t kMaxPayload = kDefaultMTU;
    auto query_service_cb = [](hci_spec::ConnectionHandle, Psm) {
      return std::nullopt;
    };
    a2dp_offload_manager_ = std::make_unique<A2dpOffloadManager>(
        transport()->command_channel()->AsWeakPtr());
    link_ = std::make_unique<LogicalLink>(
        kConnHandle,
        type,
        pw::bluetooth::emboss::ConnectionRole::CENTRAL,
        kMaxPayload,
        std::move(query_service_cb),
        transport()->acl_data_channel(),
        transport()->command_channel(),
        /*random_channel_ids=*/true,
        *a2dp_offload_manager_,
        dispatcher());
  }
  void ResetAndCreateNewLogicalLink(LinkType type = LinkType::kACL) {
    link()->Close();
    DeleteLink();
    NewLogicalLink(type);
  }

  LogicalLink* link() const { return link_.get(); }
  void DeleteLink() { link_ = nullptr; }

 private:
  std::unique_ptr<LogicalLink> link_;
  std::unique_ptr<A2dpOffloadManager> a2dp_offload_manager_;
};

struct QueueAclConnectionRetVal {
  l2cap::CommandId extended_features_id;
  l2cap::CommandId fixed_channels_supported_id;
};

static constexpr l2cap::ExtendedFeatures kExtendedFeatures =
    l2cap::kExtendedFeaturesBitEnhancedRetransmission;

using LogicalLinkDeathTest = LogicalLinkTest;

TEST_F(LogicalLinkDeathTest, DestructedWithoutClosingDies) {
  // Deleting the link without calling `Close` on it should trigger an
  // assertion.
  ASSERT_DEATH_IF_SUPPORTED(DeleteLink(), ".*closed.*");
}

TEST_F(LogicalLinkTest, FixedChannelHasCorrectMtu) {
  Channel::WeakPtr fixed_chan = link()->OpenFixedChannel(kATTChannelId);
  ASSERT_TRUE(fixed_chan.is_alive());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_rx_sdu_size());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_tx_sdu_size());
}

TEST_F(LogicalLinkTest, DropsBroadcastPackets) {
  ResetAndCreateNewLogicalLink();

  QueueAclConnectionRetVal cmd_ids;
  cmd_ids.extended_features_id = 1;
  cmd_ids.fixed_channels_supported_id = 2;

  const auto kExtFeaturesRsp = l2cap::testing::AclExtFeaturesInfoRsp(
      cmd_ids.extended_features_id, kConnHandle, kExtendedFeatures);
  EXPECT_ACL_PACKET_OUT(test_device(),
                        l2cap::testing::AclExtFeaturesInfoReq(
                            cmd_ids.extended_features_id, kConnHandle),
                        &kExtFeaturesRsp);
  EXPECT_ACL_PACKET_OUT(test_device(),
                        l2cap::testing::AclFixedChannelsSupportedInfoReq(
                            cmd_ids.fixed_channels_supported_id, kConnHandle));

  Channel::WeakPtr connectionless_chan =
      link()->OpenFixedChannel(kConnectionlessChannelId);
  ASSERT_TRUE(connectionless_chan.is_alive());

  size_t rx_count = 0;
  bool activated = connectionless_chan->Activate(
      [&](ByteBufferPtr) { rx_count++; }, []() {});
  ASSERT_TRUE(activated);

  StaticByteBuffer group_frame(0x0A,
                               0x00,  // Length (PSM + info = 10)
                               0x02,
                               0x00,  // Connectionless data channel
                               0xF0,
                               0x0F,  // PSM
                               'S',
                               'a',
                               'p',
                               'p',
                               'h',
                               'i',
                               'r',
                               'e'  // Info Payload
  );
  hci::ACLDataPacketPtr packet = hci::ACLDataPacket::New(
      kConnHandle,
      hci_spec::ACLPacketBoundaryFlag::kCompletePDU,
      hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
      group_frame.size());
  ASSERT_TRUE(packet);
  packet->mutable_view()->mutable_payload_data().Write(group_frame);

  link()->HandleRxPacket(std::move(packet));

  // Should be dropped.
  EXPECT_EQ(0u, rx_count);
}

// LE links are unsupported, so result should be an error. No command should be
// sent.
TEST_F(LogicalLinkTest, SetBrEdrAutomaticFlushTimeoutFailsForLELink) {
  constexpr std::chrono::milliseconds kTimeout(100);
  ResetAndCreateNewLogicalLink(LinkType::kLE);

  bool cb_called = false;
  link()->SetBrEdrAutomaticFlushTimeout(kTimeout, [&](auto result) {
    cb_called = true;
    ASSERT_TRUE(result.is_error());
    EXPECT_EQ(
        ToResult(
            pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
        result.error_value());
  });
  EXPECT_TRUE(cb_called);
}

TEST_F(LogicalLinkTest, SetAutomaticFlushTimeoutSuccess) {
  ResetAndCreateNewLogicalLink();

  QueueAclConnectionRetVal cmd_ids;
  cmd_ids.extended_features_id = 1;
  cmd_ids.fixed_channels_supported_id = 2;

  const auto kExtFeaturesRsp = l2cap::testing::AclExtFeaturesInfoRsp(
      cmd_ids.extended_features_id, kConnHandle, kExtendedFeatures);
  EXPECT_ACL_PACKET_OUT(test_device(),
                        l2cap::testing::AclExtFeaturesInfoReq(
                            cmd_ids.extended_features_id, kConnHandle),
                        &kExtFeaturesRsp);
  EXPECT_ACL_PACKET_OUT(test_device(),
                        l2cap::testing::AclFixedChannelsSupportedInfoReq(
                            cmd_ids.fixed_channels_supported_id, kConnHandle));

  std::optional<hci::Result<>> cb_status;
  auto result_cb = [&](auto status) { cb_status = status; };

  // Test command complete error
  const auto kCommandCompleteError = bt::testing::CommandCompletePacket(
      hci_spec::kWriteAutomaticFlushTimeout,
      pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 0),
      &kCommandCompleteError);
  link()->SetBrEdrAutomaticFlushTimeout(
      pw::chrono::SystemClock::duration::max(), result_cb);
  RunUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  ASSERT_TRUE(cb_status->is_error());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID),
            *cb_status);
  cb_status.reset();

  // Test flush timeout = 0 (no command should be sent)
  link()->SetBrEdrAutomaticFlushTimeout(std::chrono::milliseconds(0),
                                        result_cb);
  RunUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_TRUE(cb_status->is_error());
  EXPECT_EQ(
      ToResult(
          pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
      *cb_status);

  // Test infinite flush timeout (flush timeout of 0 should be sent).
  const auto kCommandComplete = bt::testing::CommandCompletePacket(
      hci_spec::kWriteAutomaticFlushTimeout,
      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 0),
      &kCommandComplete);
  link()->SetBrEdrAutomaticFlushTimeout(
      pw::chrono::SystemClock::duration::max(), result_cb);
  RunUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_EQ(fit::ok(), *cb_status);
  cb_status.reset();

  // Test msec to parameter conversion
  // (hci_spec::kMaxAutomaticFlushTimeoutDuration(1279) * conversion_factor(1.6)
  // = 2046).
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 2046),
      &kCommandComplete);
  link()->SetBrEdrAutomaticFlushTimeout(
      hci_spec::kMaxAutomaticFlushTimeoutDuration, result_cb);
  RunUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_EQ(fit::ok(), *cb_status);
  cb_status.reset();

  // Test too large flush timeout (no command should be sent).
  link()->SetBrEdrAutomaticFlushTimeout(
      hci_spec::kMaxAutomaticFlushTimeoutDuration +
          std::chrono::milliseconds(1),
      result_cb);
  RunUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_TRUE(cb_status->is_error());
  EXPECT_EQ(
      ToResult(
          pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
      *cb_status);
}

}  // namespace
}  // namespace bt::l2cap::internal
