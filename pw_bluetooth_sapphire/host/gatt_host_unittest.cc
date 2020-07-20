// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_host.h"

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"

namespace bthost {
namespace {

class GattHostTest : public bt::gatt::testing::FakeLayerTest {
 public:
  GattHostTest() = default;
  ~GattHostTest() override = default;

 protected:
  void SetUp() override {
    auto fake_domain = std::make_unique<bt::gatt::testing::FakeLayer>();
    fake_domain_ = fake_domain.get();
    gatt_host_ = std::make_unique<GattHost>(std::move(fake_domain));
  }

  void TearDown() override {
    gatt_host_ = nullptr;
    fake_domain_ = nullptr;
    RunLoopUntilIdle();  // Run all pending tasks.
  }

  bt::gatt::testing::FakeLayer* fake_domain() const { return fake_domain_; }
  GattHost* gatt_host() const { return gatt_host_.get(); }

 private:
  bt::gatt::testing::FakeLayer* fake_domain_;
  std::unique_ptr<GattHost> gatt_host_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(GattHostTest);
};

TEST_F(GattHostTest, RemoteServiceWatcher) {
  bool called = false;
  gatt_host()->SetRemoteServiceWatcher([&](auto peer_id, auto svc) {
    called = true;

    // It should be possible to modify the callback without hitting a deadlock.
    gatt_host()->SetRemoteServiceWatcher([](auto, auto) {});
  });

  fake_domain()->AddPeerService(bt::PeerId(1), bt::gatt::ServiceData(), /*notify=*/true);
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace bthost
