// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/command_handler.h"

#include <pw_async_fuchsia/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::l2cap::internal {
namespace {

constexpr ChannelId kLocalCId = 0x0040;
constexpr ChannelId kRemoteCId = 0x60a3;

struct TestPayload {
  uint8_t value;
};

class TestCommandHandler final : public CommandHandler {
 public:
  // Inherit ctor
  using CommandHandler::CommandHandler;

  // A response that decoding always fails for.
  class UndecodableResponse final : public CommandHandler::Response {
   public:
    using PayloadT = TestPayload;
    static constexpr const char* kName = "Undecodable Response";

    using Response::Response;  // Inherit ctor
    bool Decode(const ByteBuffer& payload_buf) { return false; }
  };

  using UndecodableResponseCallback = fit::function<void(const UndecodableResponse& rsp)>;

  bool SendRequestWithUndecodableResponse(CommandCode code, const ByteBuffer& payload,
                                          UndecodableResponseCallback cb) {
    auto on_rsp = BuildResponseHandler<UndecodableResponse>(std::move(cb));
    return sig()->SendRequest(code, payload, std::move(on_rsp));
  }
};

class CommandHandlerTest : public ::gtest::TestLoopFixture {
 public:
  CommandHandlerTest() = default;
  ~CommandHandlerTest() override = default;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(CommandHandlerTest);

 protected:
  // TestLoopFixture overrides
  void SetUp() override {
    TestLoopFixture::SetUp();
    signaling_channel_ = std::make_unique<testing::FakeSignalingChannel>(pw_dispatcher_);
    command_handler_ = std::make_unique<TestCommandHandler>(
        fake_sig(), fit::bind_member<&CommandHandlerTest::OnRequestFail>(this));
    request_fail_callback_ = nullptr;
    failed_requests_ = 0;
  }

  void TearDown() override {
    request_fail_callback_ = nullptr;
    signaling_channel_ = nullptr;
    command_handler_ = nullptr;
    TestLoopFixture::TearDown();
  }

  testing::FakeSignalingChannel* fake_sig() const { return signaling_channel_.get(); }
  TestCommandHandler* cmd_handler() const { return command_handler_.get(); }
  size_t failed_requests() const { return failed_requests_; }

  void set_request_fail_callback(fit::closure request_fail_callback) {
    BT_ASSERT(!request_fail_callback_);
    request_fail_callback_ = std::move(request_fail_callback);
  }

 private:
  void OnRequestFail() {
    failed_requests_++;
    if (request_fail_callback_) {
      request_fail_callback_();
    }
  }

  std::unique_ptr<testing::FakeSignalingChannel> signaling_channel_;
  std::unique_ptr<TestCommandHandler> command_handler_;
  fit::closure request_fail_callback_;
  size_t failed_requests_;
  pw::async::fuchsia::FuchsiaDispatcher pw_dispatcher_{dispatcher()};
};

TEST_F(CommandHandlerTest, OutboundDisconReqRspOk) {
  // Disconnect Request payload
  StaticByteBuffer expected_discon_req(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Disconnect Response payload
  // Channel endpoint roles (source, destination) are relative to requester so
  // the response's payload should be the same as the request's
  const ByteBuffer& ok_discon_rsp = expected_discon_req;

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kSuccess, ok_discon_rsp.view()});

  bool cb_called = false;
  CommandHandler::DisconnectionResponseCallback on_discon_rsp =
      [&cb_called](const CommandHandler::DisconnectionResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
      };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(CommandHandlerTest, OutboundDisconReqRej) {
  // Disconnect Request payload
  StaticByteBuffer expected_discon_req(
      // Destination CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Command Reject payload
  StaticByteBuffer rej_cid(
      // Reject Reason (invalid channel ID)
      LowerBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),
      UpperBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),

      // Source CID (relative to rejecter)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Destination CID (relative to rejecter)
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kReject, rej_cid.view()});

  bool cb_called = false;
  CommandHandler::DisconnectionResponseCallback on_discon_rsp =
      [&cb_called](const CommandHandler::DisconnectionResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kReject, rsp.status());
        EXPECT_EQ(RejectReason::kInvalidCID, rsp.reject_reason());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
      };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(CommandHandlerTest, OutboundDisconReqRejNotEnoughBytes) {
  constexpr ChannelId kBadLocalCId = 0x0005;  // Not a dynamic channel

  // Disconnect Request payload
  auto expected_discon_req = StaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kBadLocalCId), UpperBits(kBadLocalCId));

  // Invalid Command Reject payload (size is too small)
  auto rej_rsp = StaticByteBuffer(0x01);

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kReject, rej_rsp.view()});

  bool cb_called = false;
  auto on_discon_rsp = [&cb_called](const CommandHandler::DisconnectionResponse& rsp) {
    cb_called = true;
  };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kBadLocalCId, std::move(on_discon_rsp)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(CommandHandlerTest, OutboundDisconReqRejInvalidCIDNotEnoughBytes) {
  constexpr ChannelId kBadLocalCId = 0x0005;  // Not a dynamic channel

  // Disconnect Request payload
  auto expected_discon_req = StaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kBadLocalCId), UpperBits(kBadLocalCId));

  // Command Reject payload (the invalid channel IDs are missing)
  auto rej_rsp = StaticByteBuffer(
      // Reject Reason (invalid channel ID)
      LowerBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),
      UpperBits(static_cast<uint16_t>(RejectReason::kInvalidCID)));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kReject, rej_rsp.view()});

  bool cb_called = false;
  auto on_discon_rsp = [&cb_called](const CommandHandler::DisconnectionResponse& rsp) {
    cb_called = true;
  };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kBadLocalCId, std::move(on_discon_rsp)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(CommandHandlerTest, InboundDisconReqRspOk) {
  CommandHandler::DisconnectionRequestCallback cb = [](ChannelId local_cid, ChannelId remote_cid,
                                                       auto responder) {
    EXPECT_EQ(kLocalCId, local_cid);
    EXPECT_EQ(kRemoteCId, remote_cid);
    responder->Send();
  };
  cmd_handler()->ServeDisconnectionRequest(std::move(cb));

  // Disconnection Request payload
  auto discon_req = StaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Disconnection Response payload is identical to request payload.
  auto expected_rsp = discon_req;

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kDisconnectionRequest, discon_req, expected_rsp));
}

TEST_F(CommandHandlerTest, InboundDisconReqRej) {
  CommandHandler::DisconnectionRequestCallback cb = [](ChannelId local_cid, ChannelId remote_cid,
                                                       auto responder) {
    EXPECT_EQ(kLocalCId, local_cid);
    EXPECT_EQ(kRemoteCId, remote_cid);
    responder->RejectInvalidChannelId();
  };
  cmd_handler()->ServeDisconnectionRequest(std::move(cb));

  // Disconnection Request payload
  auto discon_req = StaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Disconnection Response payload
  auto expected_rsp = discon_req;

  RETURN_IF_FATAL(fake_sig()->ReceiveExpectRejectInvalidChannelId(kDisconnectionRequest, discon_req,
                                                                  kLocalCId, kRemoteCId));
}

TEST_F(CommandHandlerTest, OutboundDisconReqRspPayloadNotEnoughBytes) {
  // Disconnect Request payload
  auto expected_discon_req = StaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Disconnect Response payload (should include Source CID)
  auto malformed_discon_rsp = StaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kSuccess, malformed_discon_rsp.view()});

  bool cb_called = false;
  auto on_discon_cb = [&cb_called](const CommandHandler::DisconnectionResponse& rsp) {
    cb_called = true;
  };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_cb)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(CommandHandlerTest, OutboundReqRspDecodeError) {
  auto payload = StaticByteBuffer(0x00);
  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, payload.view(),
                      {SignalingChannel::Status::kSuccess, payload.view()});

  bool cb_called = false;
  auto on_rsp_cb = [&cb_called](const TestCommandHandler::UndecodableResponse& rsp) {
    cb_called = true;
  };

  EXPECT_TRUE(cmd_handler()->SendRequestWithUndecodableResponse(kDisconnectionRequest, payload,
                                                                std::move(on_rsp_cb)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(CommandHandlerTest, OutboundDisconReqRspTimeOut) {
  // Disconnect Request payload
  auto expected_discon_req = StaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
                      {SignalingChannel::Status::kTimeOut, {}});
  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view());

  set_request_fail_callback([this]() {
    // Should still be allowed to send requests even after one failed
    auto on_discon_rsp = [](auto&) {};
    EXPECT_TRUE(
        cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_rsp)));
  });

  auto on_discon_rsp = [](auto&) { ADD_FAILURE(); };

  EXPECT_TRUE(
      cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_rsp)));

  ASSERT_EQ(0u, failed_requests());
  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_EQ(1u, failed_requests());
}

TEST_F(CommandHandlerTest, RejectInvalidChannelId) {
  CommandHandler::DisconnectionRequestCallback cb =
      [](ChannelId local_cid, ChannelId remote_cid,
         CommandHandler::DisconnectionResponder* responder) {
        responder->RejectInvalidChannelId();
      };
  cmd_handler()->ServeDisconnectionRequest(std::move(cb));

  // Disconnection Request payload
  auto discon_req = StaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  RETURN_IF_FATAL(fake_sig()->ReceiveExpectRejectInvalidChannelId(kDisconnectionRequest, discon_req,
                                                                  kLocalCId, kRemoteCId));
}

}  // namespace
}  // namespace bt::l2cap::internal
