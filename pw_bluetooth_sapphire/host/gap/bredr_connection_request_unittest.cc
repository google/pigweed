// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_connection_request.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <pw_async/fake_dispatcher_fixture.h>
#include <pw_async_fuchsia/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect.h"

namespace bt::gap {
namespace {

using namespace inspect::testing;

const DeviceAddress kTestAddr(DeviceAddress::Type::kBREDR, {1});
const PeerId kPeerId;
constexpr hci::Error RetryableError =
    ToResult(pw::bluetooth::emboss::StatusCode::PAGE_TIMEOUT).error_value();

using BrEdrConnectionRequestTests = pw::async::test::FakeDispatcherFixture;

TEST_F(BrEdrConnectionRequestTests, IncomingRequestStatusTracked) {
  // A freshly created request is not yet incoming
  auto req = BrEdrConnectionRequest(dispatcher(), kTestAddr, kPeerId,
                                    Peer::InitializingConnectionToken([] {}));
  EXPECT_FALSE(req.HasIncoming());

  req.BeginIncoming();
  // We should now have an incoming request, but still not an outgoing
  EXPECT_TRUE(req.HasIncoming());
  EXPECT_FALSE(req.AwaitingOutgoing());

  // A completed request is no longer incoming
  req.CompleteIncoming();
  EXPECT_FALSE(req.HasIncoming());
}

TEST_F(BrEdrConnectionRequestTests, CallbacksExecuted) {
  bool callback_called = false;
  bool token_destroyed = false;
  auto req = BrEdrConnectionRequest(
      dispatcher(), kTestAddr, kPeerId,
      Peer::InitializingConnectionToken([&token_destroyed] { token_destroyed = true; }),
      [&callback_called](auto, auto) { callback_called = true; });

  // A freshly created request with a callback is awaiting outgoing
  EXPECT_TRUE(req.AwaitingOutgoing());
  // Notifying callbacks triggers the callback
  req.NotifyCallbacks(fit::ok(), [&]() {
    EXPECT_TRUE(token_destroyed);
    return nullptr;
  });
  EXPECT_TRUE(token_destroyed);
  EXPECT_TRUE(callback_called);
}

#ifndef NINSPECT
TEST_F(BrEdrConnectionRequestTests, Inspect) {
  // inspector must outlive request
  inspect::Inspector inspector;
  BrEdrConnectionRequest req(dispatcher(), kTestAddr, kPeerId,
                             Peer::InitializingConnectionToken([] {}), [](auto, auto) {});
  req.BeginIncoming();
  req.AttachInspect(inspector.GetRoot(), "request_name");

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());
  EXPECT_THAT(hierarchy.value(),
              ChildrenMatch(ElementsAre(NodeMatches(
                  AllOf(NameMatches("request_name"),
                        PropertyList(UnorderedElementsAre(
                            StringIs("peer_id", kPeerId.ToString()), UintIs("callbacks", 1u),
                            BoolIs("has_incoming", true),
                            IntIs("first_create_connection_request_timestamp", -1))))))));
}
#endif  // NINSPECT

class BrEdrConnectionRequestLoopTest : public pw::async::test::FakeDispatcherFixture {
 protected:
  using OnComplete = BrEdrConnectionRequest::OnComplete;

  BrEdrConnectionRequestLoopTest()
      : req_(dispatcher(), kTestAddr, kPeerId, Peer::InitializingConnectionToken([] {}),
             [this](hci::Result<> res, BrEdrConnection* conn) {
               if (handler_) {
                 handler_(res, conn);
               }
             }) {
    // By default, an outbound ConnectionRequest with a complete handler that just logs the result.
    handler_ = [](hci::Result<> res, auto /*ignore*/) {
      bt_log(INFO, "gap-bredr-test", "outbound connection request complete: %s", bt_str(res));
    };
  }

  void set_on_complete(BrEdrConnectionRequest::OnComplete handler) {
    handler_ = std::move(handler);
  }

  BrEdrConnectionRequest& connection_req() { return req_; }

 private:
  BrEdrConnectionRequest req_;
  OnComplete handler_;
};
using BrEdrConnectionRequestLoopDeathTest = BrEdrConnectionRequestLoopTest;

TEST_F(BrEdrConnectionRequestLoopTest, RetryableErrorCodeShouldRetryAfterFirstCreateConnection) {
  connection_req().RecordHciCreateConnectionAttempt();
  RunFor(std::chrono::seconds(1));
  EXPECT_TRUE(connection_req().ShouldRetry(RetryableError));
}

TEST_F(BrEdrConnectionRequestLoopTest, ShouldntRetryBeforeFirstCreateConnection) {
  EXPECT_FALSE(connection_req().ShouldRetry(RetryableError));
}

TEST_F(BrEdrConnectionRequestLoopTest, ShouldntRetryWithNonRetriableErrorCode) {
  connection_req().RecordHciCreateConnectionAttempt();
  RunFor(std::chrono::seconds(1));
  EXPECT_FALSE(connection_req().ShouldRetry(hci::Error(HostError::kCanceled)));
}

TEST_F(BrEdrConnectionRequestLoopTest, ShouldntRetryAfterThirtySeconds) {
  connection_req().RecordHciCreateConnectionAttempt();
  RunFor(std::chrono::seconds(15));
  // Should be OK to retry after 15 seconds
  EXPECT_TRUE(connection_req().ShouldRetry(RetryableError));
  connection_req().RecordHciCreateConnectionAttempt();

  // Should still be OK to retry, even though we've already retried
  RunFor(std::chrono::seconds(14));
  EXPECT_TRUE(connection_req().ShouldRetry(RetryableError));
  connection_req().RecordHciCreateConnectionAttempt();

  RunFor(std::chrono::seconds(1));
  EXPECT_FALSE(connection_req().ShouldRetry(RetryableError));
}

}  // namespace
}  // namespace bt::gap
