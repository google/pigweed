// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection_request.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"

namespace bt::gap {
namespace {

struct Unit {};

const DeviceAddress kTestAddr(DeviceAddress::Type::kBREDR, {1});

TEST(GAP_ConnectionRequestTests, IncomingRequestStatusTracked) {
  // A freshly created request is not yet incoming
  auto req = ConnectionRequest<Unit>(kTestAddr);
  EXPECT_FALSE(req.HasIncoming());

  req.BeginIncoming();
  // We should now have an incoming request, but still not an outgoing
  EXPECT_TRUE(req.HasIncoming());
  EXPECT_FALSE(req.AwaitingOutgoing());

  // A completed request is no longer incoming
  req.CompleteIncoming();
  EXPECT_FALSE(req.HasIncoming());
}

TEST(GAP_ConnectionRequestTests, CallbacksExecuted) {
  bool callback_called = false;
  auto req = ConnectionRequest<Unit>(kTestAddr,
                                     [&callback_called](auto, auto) { callback_called = true; });

  // A freshly created request with a callback is awaiting outgoing
  EXPECT_TRUE(req.AwaitingOutgoing());
  // Notifying callbacks triggers the callback
  req.NotifyCallbacks(hci::Status(), []() { return Unit{}; });
  ASSERT_TRUE(callback_called);
}

}  // namespace
}  // namespace bt::gap
