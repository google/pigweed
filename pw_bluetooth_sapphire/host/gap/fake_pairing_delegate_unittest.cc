// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace gap {
namespace {

TEST(GAP_FakePairingDelegateTest, io_capability) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);
  EXPECT_EQ(sm::IOCapability::kDisplayYesNo, delegate.io_capability());
  delegate.set_io_capability(sm::IOCapability::kNoInputNoOutput);
  EXPECT_EQ(sm::IOCapability::kNoInputNoOutput, delegate.io_capability());
}

TEST(GAP_FakePairingDelegateTest, CompletePairing) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  bool cb_called = false;
  auto cb = [&cb_called](auto id, auto status) {
    cb_called = true;
    EXPECT_EQ(PeerId(5), id);
    EXPECT_EQ(sm::Status(HostError::kFailed), status);
  };
  delegate.SetCompletePairingCallback(std::move(cb));
  delegate.CompletePairing(PeerId(5), sm::Status(HostError::kFailed));
  EXPECT_TRUE(cb_called);
}

TEST(GAP_FakePairingDelegateTest, ConfirmPairing) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  bool cb_called = false;
  auto cb = [&cb_called](auto id, auto confirm) {
    cb_called = true;
    EXPECT_EQ(PeerId(5), id);
    ASSERT_TRUE(confirm);
    confirm(true);
  };
  delegate.SetConfirmPairingCallback(std::move(cb));
  delegate.ConfirmPairing(PeerId(5), [](bool) {});
  EXPECT_TRUE(cb_called);
}

TEST(GAP_FakePairingDelegateTest, DisplayPasskey) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  bool cb_called = false;
  auto cb = [&cb_called](auto id, auto passkey, auto method, auto confirm) {
    cb_called = true;
    EXPECT_EQ(PeerId(5), id);
    EXPECT_EQ(123456U, passkey);
    EXPECT_EQ(PairingDelegate::DisplayMethod::kComparison, method);
    ASSERT_TRUE(confirm);
    confirm(true);
  };
  delegate.SetDisplayPasskeyCallback(std::move(cb));
  delegate.DisplayPasskey(PeerId(5), 123456, PairingDelegate::DisplayMethod::kComparison,
                          [](bool) {});
  EXPECT_TRUE(cb_called);
}

TEST(GAP_FakePairingDelegateTest, RequestPasskey) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  bool cb_called = false;
  auto cb = [&cb_called](auto id, auto respond) {
    cb_called = true;
    EXPECT_EQ(PeerId(5), id);
    ASSERT_TRUE(respond);
    respond(-1);
  };
  delegate.SetRequestPasskeyCallback(std::move(cb));
  delegate.RequestPasskey(PeerId(5), [](uint64_t) {});
  EXPECT_TRUE(cb_called);
}

TEST(GAP_FakePairingDelegateTest, UnexpectedCalls) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  // Each of the following calls should generate failure(s).
  // delegate.CompletePairing(PeerId(5), sm::Status(HostError::kFailed));
  // delegate.ConfirmPairing(PeerId(5), [](bool) {});
  // delegate.DisplayPasskey(PeerId(5), 123456, PairingDelegate::DisplayMethod::kComparison,
  //                         [](bool) {});
  // delegate.RequestPasskey(PeerId(5), [](uint64_t) {});
}

TEST(GAP_FakePairingDelegateTest, ExpectCallNotCalled) {
  FakePairingDelegate delegate(sm::IOCapability::kDisplayYesNo);

  // delegate.SetCompletePairingCallback([](auto, auto) {});
  // delegate.SetConfirmPairingCallback([](auto, auto) {});
  // delegate.SetDisplayPasskeyCallback([](auto, auto, auto, auto) {});
  // delegate.SetRequestPasskeyCallback([](auto, auto) {});
  // Each of the preceding calls should generate failure(s) when |delegate| goes out of scope.
}

}  // namespace
}  // namespace gap
}  // namespace bt
