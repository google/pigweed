// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_host.h"

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"

namespace bthost {
namespace {

namespace fgatt = fuchsia::bluetooth::gatt;

constexpr GattHost::Token kToken1 = 1;
constexpr GattHost::Token kToken2 = 2;

constexpr bt::PeerId kPeerId1(1);
constexpr bt::PeerId kPeerId2(2);

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

  // Returns true if the given gatt.Client handle was closed after the event
  // loop finishes processing. Returns false if the handle was not closed.
  bool IsClientHandleClosedAfterLoop(fidl::InterfaceHandle<fgatt::Client> handle) {
    return IsClientHandleClosedAfterLoopUnretained(&handle);
  }

  // Similar to `IsClientHandleClosedAfterLoop` but the ownership of
  // |handle| remains with the caller when this method returns.
  bool IsClientHandleClosedAfterLoopUnretained(fidl::InterfaceHandle<fgatt::Client>* handle) {
    ZX_ASSERT(handle);

    fgatt::ClientPtr proxy;
    proxy.Bind(std::move(*handle));

    bool closed = false;
    proxy.set_error_handler([&](zx_status_t s) {
      EXPECT_EQ(ZX_ERR_PEER_CLOSED, s);
      closed = true;
    });
    RunLoopUntilIdle();

    *handle = proxy.Unbind();
    return closed;
  }

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

TEST_F(GattHostTest, BindGattClientDoubleBindOnSamePeerIdIsNotAllowed) {
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2;

  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken1, kPeerId1, handle2.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle1)));
  EXPECT_TRUE(IsClientHandleClosedAfterLoop(std::move(handle2)));
}

TEST_F(GattHostTest, BindGattClientDoubleBindOnSamePeerIdButDifferentTokensIsAllowed) {
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2;

  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken2, kPeerId1, handle2.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle1)));
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle2)));
}

TEST_F(GattHostTest, BindGattClientMultiplePeerIdsSameToken) {
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2;

  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken1, kPeerId2, handle2.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle1)));
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle2)));
}

TEST_F(GattHostTest, BindGattClientClosingHandleUnbindsIt) {
  // Test that it is possible to register a new handle for a previously used
  // token and peer ID.
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2, handle3;
  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken1, kPeerId2, handle2.NewRequest());
  gatt_host()->BindGattClient(kToken2, kPeerId1, handle3.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle1));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle2));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle3));

  // Drop the handle and wait for GattHost to process the handle closure.
  handle1.TakeChannel();
  RunLoopUntilIdle();

  // It should be possible to register a new channel.
  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle1)));

  // The other two handles (different peer id and different token) should remain
  // untouched.
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle2)));
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle3)));
}

TEST_F(GattHostTest, BindGattClientUnbindSinglePeer) {
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2, handle3;

  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken1, kPeerId2, handle2.NewRequest());
  gatt_host()->BindGattClient(kToken2, kPeerId1, handle3.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle1));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle2));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle3));

  // Unbind one. `handle1` should close but `handle2` should remain open.
  gatt_host()->UnbindGattClient(kToken1, kPeerId1);
  EXPECT_TRUE(IsClientHandleClosedAfterLoop(std::move(handle1)));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle2));

  // Calling unbind again on kPeerId1 should have no effect.
  gatt_host()->UnbindGattClient(kToken1, kPeerId1);
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle2));

  // Unbinding `handle2` should close it.
  gatt_host()->UnbindGattClient(kToken1, kPeerId2);
  EXPECT_TRUE(IsClientHandleClosedAfterLoop(std::move(handle2)));

  // `handle3` should remain open as it was registered with a different token.
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle3)));
}

TEST_F(GattHostTest, BindGattClientUnbindAllPeers) {
  fidl::InterfaceHandle<fgatt::Client> handle1, handle2, handle3;

  gatt_host()->BindGattClient(kToken1, kPeerId1, handle1.NewRequest());
  gatt_host()->BindGattClient(kToken1, kPeerId2, handle2.NewRequest());
  gatt_host()->BindGattClient(kToken2, kPeerId1, handle3.NewRequest());

  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle1));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle2));
  EXPECT_FALSE(IsClientHandleClosedAfterLoopUnretained(&handle3));

  // Unbind all handles associated with kToken1. All handles should close except
  // `handle3`, which is bound to kToken2.
  gatt_host()->UnbindGattClient(kToken1, std::nullopt);
  EXPECT_TRUE(IsClientHandleClosedAfterLoop(std::move(handle1)));
  EXPECT_TRUE(IsClientHandleClosedAfterLoop(std::move(handle2)));
  EXPECT_FALSE(IsClientHandleClosedAfterLoop(std::move(handle3)));
}

}  // namespace
}  // namespace bthost
