// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_host.h"

#include <fbl/macros.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"

namespace bthost {
namespace {

class GattHostTest : public ::gtest::TestLoopFixture {
 public:
  GattHostTest() = default;
  ~GattHostTest() override = default;

 protected:
  void SetUp() override {
    fake_domain_ = bt::gatt::testing::FakeLayer::Create();
    gatt_host_ = GattHost::CreateForTesting(dispatcher(), fake_domain_);
  }

  void TearDown() override {
    gatt_host_->ShutDown();
    RunLoopUntilIdle();  // Run all pending tasks.
  }

  bt::gatt::testing::FakeLayer* fake_domain() const { return fake_domain_.get(); }
  GattHost* gatt_host() const { return gatt_host_.get(); }

 private:
  fbl::RefPtr<bt::gatt::testing::FakeLayer> fake_domain_;
  fbl::RefPtr<GattHost> gatt_host_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(GattHostTest);
};

TEST_F(GattHostTest, RemoteServiceWatcher) {
  gatt_host()->Initialize();

  bool called = false;
  gatt_host()->SetRemoteServiceWatcher([&](auto peer_id, auto svc) {
    called = true;

    // It should be possible to modify the callback without hitting a deadlock.
    gatt_host()->SetRemoteServiceWatcher([](auto, auto) {});
  });

  fake_domain()->NotifyRemoteService(bt::PeerId(1), nullptr);
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace bthost
