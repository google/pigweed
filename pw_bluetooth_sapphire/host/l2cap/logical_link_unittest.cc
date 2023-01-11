// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/logical_link.h"

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::l2cap::internal {
namespace {
using Conn = hci::Connection;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;
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

    TestingBase::TearDown();
  }
  void NewLogicalLink(bt::LinkType type = bt::LinkType::kLE) {
    const hci_spec::ConnectionHandle kConnHandle = 0x0001;
    const size_t kMaxPayload = kDefaultMTU;
    auto query_service_cb = [](hci_spec::ConnectionHandle, PSM) { return std::nullopt; };
    link_ = std::make_unique<LogicalLink>(
        kConnHandle, type, pw::bluetooth::emboss::ConnectionRole::CENTRAL, kMaxPayload,
        std::move(query_service_cb), transport()->acl_data_channel(),
        transport()->command_channel(),
        /*random_channel_ids=*/true);
  }
  LogicalLink* link() const { return link_.get(); }
  void DeleteLink() { link_ = nullptr; }

 private:
  std::unique_ptr<LogicalLink> link_;
};

using LogicalLinkDeathTest = LogicalLinkTest;

TEST_F(LogicalLinkDeathTest, DestructedWithoutClosingDies) {
  // Deleting the link without calling `Close` on it should trigger an assertion.
  ASSERT_DEATH_IF_SUPPORTED(DeleteLink(), ".*closed.*");
}

TEST_F(LogicalLinkTest, FixedChannelHasCorrectMtu) {
  Channel::WeakPtr fixed_chan = link()->OpenFixedChannel(kATTChannelId);
  ASSERT_TRUE(fixed_chan.is_alive());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_rx_sdu_size());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_tx_sdu_size());
}

TEST_F(LogicalLinkTest, DropsBroadcastPackets) {
  link()->Close();
  NewLogicalLink(bt::LinkType::kACL);
  Channel::WeakPtr connectionless_chan = link()->OpenFixedChannel(kConnectionlessChannelId);
  ASSERT_TRUE(connectionless_chan.is_alive());

  size_t rx_count = 0;
  bool activated = connectionless_chan->Activate([&](ByteBufferPtr) { rx_count++; }, []() {});
  ASSERT_TRUE(activated);

  StaticByteBuffer group_frame(0x0A, 0x00,  // Length (PSM + info = 10)
                               0x02, 0x00,  // Connectionless data channel
                               0xF0, 0x0F,  // PSM
                               'S', 'a', 'p', 'p', 'h', 'i', 'r', 'e'  // Info Payload
  );
  auto packet = hci::ACLDataPacket::New(0x0001, hci_spec::ACLPacketBoundaryFlag::kCompletePDU,
                                        hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast,
                                        group_frame.size());
  ASSERT_TRUE(packet);
  packet->mutable_view()->mutable_payload_data().Write(group_frame);

  link()->HandleRxPacket(std::move(packet));

  // Should be dropped.
  EXPECT_EQ(0u, rx_count);
}

#define EXPECT_HIGH_PRIORITY(channel_id) \
  EXPECT_EQ(LogicalLink::ChannelPriority((channel_id)), hci::AclDataChannel::PacketPriority::kHigh)
#define EXPECT_LOW_PRIORITY(channel_id) \
  EXPECT_EQ(LogicalLink::ChannelPriority((channel_id)), hci::AclDataChannel::PacketPriority::kLow)

TEST_F(LogicalLinkTest, ChannelPriority) {
  EXPECT_HIGH_PRIORITY(kSignalingChannelId);
  EXPECT_HIGH_PRIORITY(kLESignalingChannelId);
  EXPECT_HIGH_PRIORITY(kSMPChannelId);
  EXPECT_HIGH_PRIORITY(kLESMPChannelId);

  EXPECT_LOW_PRIORITY(kFirstDynamicChannelId);
  EXPECT_LOW_PRIORITY(kLastACLDynamicChannelId);
  EXPECT_LOW_PRIORITY(kATTChannelId);
}

// LE links are unsupported, so result should be an error. No command should be sent.
TEST_F(LogicalLinkTest, SetBrEdrAutomaticFlushTimeoutFailsForLELink) {
  constexpr zx::duration kTimeout(zx::msec(100));
  link()->Close();
  NewLogicalLink(bt::LinkType::kLE);

  bool cb_called = false;
  link()->SetBrEdrAutomaticFlushTimeout(kTimeout, [&](auto result) {
    cb_called = true;
    ASSERT_TRUE(result.is_error());
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
              result.error_value());
  });
  EXPECT_TRUE(cb_called);
}

TEST_F(LogicalLinkTest, SetAutomaticFlushTimeoutSuccess) {
  link()->Close();
  NewLogicalLink(bt::LinkType::kACL);

  std::optional<hci::Result<>> cb_status;
  auto result_cb = [&](auto status) { cb_status = status; };

  // Test command complete error
  const auto kCommandCompleteError =
      bt::testing::CommandCompletePacket(hci_spec::kWriteAutomaticFlushTimeout,
                                         pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 0),
                        &kCommandCompleteError);
  link()->SetBrEdrAutomaticFlushTimeout(zx::duration::infinite(), result_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  ASSERT_TRUE(cb_status->is_error());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID), *cb_status);
  cb_status.reset();

  // Test flush timeout = 0 (no command should be sent)
  link()->SetBrEdrAutomaticFlushTimeout(zx::msec(0), result_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_TRUE(cb_status->is_error());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
            *cb_status);

  // Test infinite flush timeout (flush timeout of 0 should be sent).
  const auto kCommandComplete = bt::testing::CommandCompletePacket(
      hci_spec::kWriteAutomaticFlushTimeout, pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 0),
                        &kCommandComplete);
  link()->SetBrEdrAutomaticFlushTimeout(zx::duration::infinite(), result_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_EQ(fit::ok(), *cb_status);
  cb_status.reset();

  // Test msec to parameter conversion (hci_spec::kMaxAutomaticFlushTimeoutDuration(1279) *
  // conversion_factor(1.6) = 2046).
  EXPECT_CMD_PACKET_OUT(test_device(),
                        bt::testing::WriteAutomaticFlushTimeoutPacket(link()->handle(), 2046),
                        &kCommandComplete);
  link()->SetBrEdrAutomaticFlushTimeout(hci_spec::kMaxAutomaticFlushTimeoutDuration, result_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_EQ(fit::ok(), *cb_status);
  cb_status.reset();

  // Test too large flush timeout (no command should be sent).
  link()->SetBrEdrAutomaticFlushTimeout(hci_spec::kMaxAutomaticFlushTimeoutDuration + zx::msec(1),
                                        result_cb);
  RunLoopUntilIdle();
  ASSERT_TRUE(cb_status.has_value());
  EXPECT_TRUE(cb_status->is_error());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS),
            *cb_status);
}

}  // namespace
}  // namespace bt::l2cap::internal
