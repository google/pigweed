// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"

#include <lib/async/cpp/executor.h>
#include <lib/inspect/testing/cpp/inspect.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/manufacturer_names.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"

namespace bt::gap {
namespace {

using namespace inspect::testing;

constexpr uint16_t kManufacturer = 0x0001;
constexpr uint16_t kSubversion = 0x0002;

class GAP_PeerTest : public ::gtest::TestLoopFixture {
 public:
  GAP_PeerTest() : executor_(dispatcher()) {}

  void SetUp() override {
    TestLoopFixture::SetUp();
    auto connectable = true;
    peer_ = std::make_unique<Peer>(fit::bind_member(this, &GAP_PeerTest::NotifyListenersCallback),
                                   fit::bind_member(this, &GAP_PeerTest::UpdateExpiryCallback),
                                   fit::bind_member(this, &GAP_PeerTest::DualModeCallback),
                                   RandomPeerId(), address_, connectable);
  }

  void TearDown() override {
    peer_.reset();
    TestLoopFixture::TearDown();
  }

 protected:
  Peer& peer() { return *peer_; }
  void set_notify_listeners_cb(Peer::DeviceCallback cb) { notify_listeners_cb_ = std::move(cb); }
  void set_update_expiry_cb(Peer::DeviceCallback cb) { update_expiry_cb_ = std::move(cb); }
  void set_dual_mode_cb(Peer::DeviceCallback cb) { dual_mode_cb_ = std::move(cb); }

  // Run a promise to completion on the default async executor.
  void RunPromiseToCompletion(fit::promise<> promise) {
    bool done = false;
    executor_.schedule_task(std::move(promise).and_then([&]() { done = true; }));
    RunLoopUntilIdle();
  }

 private:
  void NotifyListenersCallback(const Peer& peer) {
    if (notify_listeners_cb_) {
      notify_listeners_cb_(peer);
    }
  }

  void UpdateExpiryCallback(const Peer& peer) {
    if (update_expiry_cb_) {
      update_expiry_cb_(peer);
    }
  }

  void DualModeCallback(const Peer& peer) {
    if (dual_mode_cb_) {
      dual_mode_cb_(peer);
    }
  }

  std::unique_ptr<Peer> peer_;
  DeviceAddress address_;
  Peer::DeviceCallback notify_listeners_cb_;
  Peer::DeviceCallback update_expiry_cb_;
  Peer::DeviceCallback dual_mode_cb_;
  async::Executor executor_;
};

TEST_F(GAP_PeerTest, InspectHierarchy) {
  inspect::Inspector inspector;
  peer().AttachInspect(inspector.GetRoot());

  peer().set_version(hci::HCIVersion::k5_0, kManufacturer, kSubversion);
  ASSERT_TRUE(peer().bredr().has_value());

  // Initialize le_data
  peer().MutLe();
  ASSERT_TRUE(peer().le().has_value());

  peer().MutLe().SetFeatures(hci::LESupportedFeatures{0x0000000000000001});

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());

  // clang-format off
  auto bredr_data_matcher = AllOf(
    NodeMatches(AllOf(
      NameMatches(Peer::BrEdrData::kInspectNodeName),
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::BrEdrData::kInspectConnectionStateName,
                 Peer::ConnectionStateToString(peer().bredr()->connection_state())),
        BoolIs(Peer::BrEdrData::kInspectLinkKeyName, peer().bredr()->bonded())
        )))));

  auto le_data_matcher = AllOf(
    NodeMatches(AllOf(
      NameMatches(Peer::LowEnergyData::kInspectNodeName),
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::LowEnergyData::kInspectConnectionStateName,
                 Peer::ConnectionStateToString(peer().le()->connection_state())),
        BoolIs(Peer::LowEnergyData::kInspectBondDataName, peer().le()->bonded()),
        StringIs(Peer::LowEnergyData::kInspectFeaturesName, "0x0000000000000001")
        )))));

  auto peer_matcher = AllOf(
    NodeMatches(
      PropertyList(UnorderedElementsAre(
        StringIs(Peer::kInspectPeerIdName, peer().identifier().ToString()),
        StringIs(Peer::kInspectTechnologyName, TechnologyTypeToString(peer().technology())),
        StringIs(Peer::kInspectAddressName, peer().address().ToString()),
        BoolIs(Peer::kInspectConnectableName, peer().connectable()),
        BoolIs(Peer::kInspectTemporaryName, peer().temporary()),
        StringIs(Peer::kInspectFeaturesName, peer().features().ToString()),
        StringIs(Peer::kInspectVersionName, hci::HCIVersionToString(peer().version().value())),
        StringIs(Peer::kInspectManufacturerName, GetManufacturerName(kManufacturer))
        ))),
    ChildrenMatch(UnorderedElementsAre(bredr_data_matcher, le_data_matcher)));
  // clang-format on
  EXPECT_THAT(hierarchy.value(), AllOf(ChildrenMatch(UnorderedElementsAre(peer_matcher))));
}

}  // namespace
}  // namespace bt::gap
