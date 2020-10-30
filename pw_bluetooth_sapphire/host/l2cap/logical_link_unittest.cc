// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/logical_link.h"

#include "fbl/ref_ptr.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace bt::l2cap::internal {
namespace {
using Conn = hci::Connection;
class L2CAP_LogicalLinkTest : public ::gtest::TestLoopFixture {
 public:
  L2CAP_LogicalLinkTest() = default;
  ~L2CAP_LogicalLinkTest() override = default;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(L2CAP_LogicalLinkTest);

 protected:
  void SetUp() override { NewLogicalLink(); }
  void TearDown() override {
    if (link_) {
      link_->Close();
      link_ = nullptr;
    }
  }
  void NewLogicalLink(Conn::LinkType type = Conn::LinkType::kLE) {
    const hci::ConnectionHandle kConnHandle = 0x0001;
    const size_t kMaxPayload = kDefaultMTU;
    auto send_packets_cb = [](auto, auto) { return true; };
    auto drop_acl_cb = [](hci::ACLPacketPredicate) {};
    auto query_service_cb = [](hci::ConnectionHandle, PSM) { return std::nullopt; };
    auto acl_priority_cb = [](auto, auto, auto) {};
    link_ = LogicalLink::New(kConnHandle, type, Conn::Role::kMaster, kMaxPayload,
                             std::move(send_packets_cb), std::move(drop_acl_cb),
                             std::move(query_service_cb), std::move(acl_priority_cb),
                             /*random_channel_ids=*/true);
  }
  LogicalLink* link() const { return link_.get(); }
  void DeleteLink() { link_ = nullptr; }

 private:
  fbl::RefPtr<LogicalLink> link_;
};

using L2CAP_LogicalLinkDeathTest = L2CAP_LogicalLinkTest;

TEST_F(L2CAP_LogicalLinkDeathTest, DestructedWithoutClosingDies) {
  // Deleting the link without calling `Close` on it should trigger an assertion.
  ASSERT_DEATH_IF_SUPPORTED(DeleteLink(), ".*closed.*");
}

TEST_F(L2CAP_LogicalLinkTest, FixedChannelHasCorrectMtu) {
  fbl::RefPtr<Channel> fixed_chan = link()->OpenFixedChannel(kATTChannelId);
  ASSERT_TRUE(fixed_chan);
  EXPECT_EQ(kMaxMTU, fixed_chan->max_rx_sdu_size());
  EXPECT_EQ(kMaxMTU, fixed_chan->max_tx_sdu_size());
}

}  // namespace
}  // namespace bt::l2cap::internal
