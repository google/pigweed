// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_request.h"

#include <lib/inspect/testing/cpp/inspect.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"

namespace bt::gap {
namespace {

using namespace inspect::testing;

const DeviceAddress kTestAddr(DeviceAddress::Type::kBREDR, {1});
const PeerId kPeerId;

TEST(BrEdrConnectionRequestTests, IncomingRequestStatusTracked) {
  // A freshly created request is not yet incoming
  auto req = BrEdrConnectionRequest(kTestAddr, kPeerId, Peer::InitializingConnectionToken([] {}));
  EXPECT_FALSE(req.HasIncoming());

  req.BeginIncoming();
  // We should now have an incoming request, but still not an outgoing
  EXPECT_TRUE(req.HasIncoming());
  EXPECT_FALSE(req.AwaitingOutgoing());

  // A completed request is no longer incoming
  req.CompleteIncoming();
  EXPECT_FALSE(req.HasIncoming());
}

TEST(BrEdrConnectionRequestTests, CallbacksExecuted) {
  bool callback_called = false;
  bool token_destroyed = false;
  auto req = BrEdrConnectionRequest(
      kTestAddr, kPeerId,
      Peer::InitializingConnectionToken([&token_destroyed] { token_destroyed = true; }),
      [&callback_called](auto, auto) { callback_called = true; });

  // A freshly created request with a callback is awaiting outgoing
  EXPECT_TRUE(req.AwaitingOutgoing());
  // Notifying callbacks triggers the callback
  req.NotifyCallbacks(fitx::ok(), [&]() {
    EXPECT_TRUE(token_destroyed);
    return nullptr;
  });
  EXPECT_TRUE(token_destroyed);
  EXPECT_TRUE(callback_called);
}

TEST(BrEdrConnectionRequestTests, Inspect) {
  // inspector must outlive request
  inspect::Inspector inspector;
  BrEdrConnectionRequest req(kTestAddr, kPeerId, Peer::InitializingConnectionToken([] {}),
                             [](auto, auto) {});
  req.BeginIncoming();
  req.AttachInspect(inspector.GetRoot(), "request_name");

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());
  EXPECT_THAT(hierarchy.value(),
              ChildrenMatch(ElementsAre(NodeMatches(
                  AllOf(NameMatches("request_name"),
                        PropertyList(UnorderedElementsAre(StringIs("peer_id", kPeerId.ToString()),
                                                          UintIs("callbacks", 1u),
                                                          BoolIs("has_incoming", true))))))));
}

}  // namespace
}  // namespace bt::gap
