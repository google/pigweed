// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/logical_link.h"

#include "fbl/ref_ptr.h"
#include "lib/fpromise/single_threaded_executor.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/mock_acl_data_channel.h"

namespace bt::l2cap::internal {
namespace {
using Conn = hci::Connection;
class LogicalLinkTest : public ::gtest::TestLoopFixture {
 public:
  LogicalLinkTest() = default;
  ~LogicalLinkTest() override = default;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LogicalLinkTest);

 protected:
  void SetUp() override { NewLogicalLink(); }
  void TearDown() override {
    if (link_) {
      link_->Close();
      link_ = nullptr;
    }
  }
  void NewLogicalLink(bt::LinkType type = bt::LinkType::kLE) {
    const hci_spec::ConnectionHandle kConnHandle = 0x0001;
    const size_t kMaxPayload = kDefaultMTU;
    auto query_service_cb = [](hci_spec::ConnectionHandle, PSM) { return std::nullopt; };
    link_ = LogicalLink::New(kConnHandle, type, Conn::Role::kMaster, &executor_, kMaxPayload,
                             std::move(query_service_cb), &acl_data_channel_,
                             /*random_channel_ids=*/true);
  }
  LogicalLink* link() const { return link_.get(); }
  void DeleteLink() { link_ = nullptr; }

  hci::testing::MockAclDataChannel* acl_data_channel() { return &acl_data_channel_; }

 private:
  fbl::RefPtr<LogicalLink> link_;
  fpromise::single_threaded_executor executor_;
  hci::testing::MockAclDataChannel acl_data_channel_;
};

using LogicalLinkDeathTest = LogicalLinkTest;

TEST_F(LogicalLinkDeathTest, DestructedWithoutClosingDies) {
  // Deleting the link without calling `Close` on it should trigger an assertion.
  ASSERT_DEATH_IF_SUPPORTED(DeleteLink(), ".*closed.*");
}

TEST_F(LogicalLinkTest, FixedChannelHasCorrectMtu) {
  fbl::RefPtr<Channel> fixed_chan = link()->OpenFixedChannel(kATTChannelId);
  ASSERT_TRUE(fixed_chan);
  EXPECT_EQ(kMaxMTU, fixed_chan->max_rx_sdu_size());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_tx_sdu_size());
}

TEST_F(LogicalLinkTest, DropsBroadcastPackets) {
  link()->Close();
  NewLogicalLink(bt::LinkType::kACL);
  fbl::RefPtr<Channel> connectionless_chan = link()->OpenFixedChannel(kConnectionlessChannelId);
  ASSERT_TRUE(connectionless_chan);

  size_t rx_count = 0;
  bool activated = connectionless_chan->Activate([&](ByteBufferPtr) { rx_count++; }, []() {});
  ASSERT_TRUE(activated);

  auto group_frame = CreateStaticByteBuffer(0x0A, 0x00,  // Length (PSM + info = 10)
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

TEST_F(LogicalLinkTest, SetBrEdrAutomaticFlushTimeoutSucceeds) {
  link()->Close();
  NewLogicalLink(bt::LinkType::kACL);
  constexpr zx::duration kTimeout(zx::msec(100));
  acl_data_channel()->set_set_bredr_automatic_flush_timeout_cb(
      [&](auto timeout, auto handle, auto cb) {
        EXPECT_EQ(timeout, kTimeout);
        EXPECT_EQ(handle, link()->handle());
        cb(fpromise::ok());
      });

  bool cb_called = false;
  link()->SetBrEdrAutomaticFlushTimeout(kTimeout, [&](auto result) {
    cb_called = true;
    EXPECT_TRUE(result.is_ok());
  });
  EXPECT_TRUE(cb_called);
}

TEST_F(LogicalLinkTest, SetBrEdrAutomaticFlushTimeoutFailsForLELink) {
  constexpr zx::duration kTimeout(zx::msec(100));
  // LE links are unsupported, so result should be an error.
  link()->Close();
  NewLogicalLink(bt::LinkType::kLE);

  // No command should be sent.
  acl_data_channel()->set_set_bredr_automatic_flush_timeout_cb(
      [&](auto timeout, auto handle, auto cb) { FAIL(); });

  bool cb_called = false;
  link()->SetBrEdrAutomaticFlushTimeout(kTimeout, [&](auto result) {
    cb_called = true;
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error(), hci_spec::StatusCode::kInvalidHCICommandParameters);
  });
  EXPECT_TRUE(cb_called);
}

}  // namespace
}  // namespace bt::l2cap::internal
