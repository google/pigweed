// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "signaling_channel.h"

#include "fake_channel_test.h"
#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::l2cap::internal {
namespace {

using Status = SignalingChannelInterface::Status;

constexpr CommandCode kUnknownCommandCode = 0x00;
constexpr CommandCode kCommandCode = 0xFF;
constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr uint16_t kTestMTU = 100;
constexpr CommandId kMaxCommandId = std::numeric_limits<CommandId>::max();

const auto kTestResponseHandler = [](Status status, const ByteBuffer& rsp_payload) {
  return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
};

class TestSignalingChannel : public SignalingChannel {
 public:
  explicit TestSignalingChannel(fbl::RefPtr<Channel> chan)
      : SignalingChannel(std::move(chan), hci::Connection::Role::kMaster) {
    set_mtu(kTestMTU);
  }
  ~TestSignalingChannel() override = default;

  using PacketCallback = fit::function<void(const SignalingPacket& packet)>;
  void set_packet_callback(PacketCallback cb) { packet_cb_ = std::move(cb); }

  // Expose GetNextCommandId() as public so it can be called by tests below.
  using SignalingChannel::GetNextCommandId;

  // Expose ResponderImpl as public so it can be directly tested (rather than
  // passed to RequestDelegate).
  using SignalingChannel::ResponderImpl;

 private:
  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) override {
    ZX_ASSERT(sdu);
    if (sdu->size()) {
      cb(SignalingPacket(sdu.get(), sdu->size() - sizeof(CommandHeader)));
    } else {
      // Silently drop the packet. See documentation in signaling_channel.h.
    }
  }

  bool IsSupportedResponse(CommandCode code) const override {
    switch (code) {
      case kCommandRejectCode:
      case kEchoResponse:
        return true;
    }

    return false;
  }

  bool HandlePacket(const SignalingPacket& packet) override {
    if (packet_cb_)
      packet_cb_(packet);

    return SignalingChannel::HandlePacket(packet);
  }

  PacketCallback packet_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TestSignalingChannel);
};

class SignalingChannelTest : public testing::FakeChannelTest {
 public:
  SignalingChannelTest() = default;
  ~SignalingChannelTest() override = default;

 protected:
  void SetUp() override {
    ChannelOptions options(kLESignalingChannelId);
    options.conn_handle = kTestHandle;

    fake_channel_inst_ = CreateFakeChannel(options);
    sig_ = std::make_unique<TestSignalingChannel>(fake_channel_inst_);
  }

  void TearDown() override {
    // Unless a test called DestroySig(), the signaling channel will outlive the
    // underlying channel.
    fake_channel_inst_ = nullptr;
    DestroySig();
  }

  TestSignalingChannel* sig() const { return sig_.get(); }

  void DestroySig() { sig_ = nullptr; }

 private:
  std::unique_ptr<TestSignalingChannel> sig_;

  // Own the fake channel so that its lifetime can span beyond that of |sig_|.
  fbl::RefPtr<Channel> fake_channel_inst_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SignalingChannelTest);
};

TEST_F(SignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunLoopUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(SignalingChannelTest, Reject) {
  constexpr uint8_t kTestId = 14;

  // Command Reject packet.
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // A command that TestSignalingChannel does not support.
  auto cmd = CreateStaticByteBuffer(
      // header
      kUnknownCommandCode, kTestId, 0x04, 0x00,

      // data
      'L', 'O', 'L', 'Z');

  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

TEST_F(SignalingChannelTest, RejectCommandCodeZero) {
  constexpr uint8_t kTestId = 14;

  // Command Reject packet.
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // A command that TestSignalingChannel does not support.
  auto cmd = CreateStaticByteBuffer(
      // header
      0x00, kTestId, 0x04, 0x00,

      // data
      'L', 'O', 'L', 'Z');

  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

TEST_F(SignalingChannelTest, RejectNotUnderstoodWithResponder) {
  constexpr uint8_t kTestId = 14;

  auto expected = CreateStaticByteBuffer(
      // Command header (Command Reject, ID, length)
      0x01, kTestId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  bool cb_called = false;
  auto send_cb = [&expected, &cb_called](auto packet) {
    cb_called = true;
    EXPECT_TRUE(ContainersEqual(expected, *packet));
  };
  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());

  TestSignalingChannel::ResponderImpl responder(sig(), kCommandCode, kTestId);
  responder.RejectNotUnderstood();

  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(SignalingChannelTest, RejectInvalidCIdWithResponder) {
  constexpr uint8_t kTestId = 14;
  constexpr uint16_t kLocalCId = 0xf00d;
  constexpr uint16_t kRemoteCId = 0xcafe;

  auto expected = CreateStaticByteBuffer(
      // Command header (Command Reject, ID, length)
      0x01, kTestId, 0x06, 0x00,

      // Reason (Invalid channel ID)
      0x02, 0x00,

      // Data (Channel IDs),
      LowerBits(kLocalCId), UpperBits(kLocalCId), LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  bool cb_called = false;
  auto send_cb = [&expected, &cb_called](auto packet) {
    cb_called = true;
    EXPECT_TRUE(ContainersEqual(expected, *packet));
  };
  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());

  TestSignalingChannel::ResponderImpl responder(sig(), kCommandCode, kTestId);
  responder.RejectInvalidChannelId(kLocalCId, kRemoteCId);

  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(SignalingChannelTest, InvalidMTU) {
  constexpr uint8_t kTestId = 14;
  constexpr uint16_t kTooSmallMTU = 7;

  // Command Reject packet.
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestId, 0x04, 0x00,

      // Reason (Signaling MTU exceeded)
      0x01, 0x00,

      // The supported MTU
      static_cast<uint8_t>(kTooSmallMTU), 0x00);

  // A command that is one octet larger than the MTU.
  auto cmd = CreateStaticByteBuffer(
      // header
      kCommandCode, kTestId, 0x04, 0x00,

      // data
      'L', 'O', 'L', 'Z');

  sig()->set_mtu(kTooSmallMTU);
  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

TEST_F(SignalingChannelTest, HandlePacket) {
  constexpr uint8_t kTestId = 14;

  // A command that TestSignalingChannel supports.
  auto cmd = CreateStaticByteBuffer(
      // header
      kCommandCode, kTestId, 0x04, 0x00,

      // data
      'L', 'O', 'L', 'Z');

  bool called = false;
  sig()->set_packet_callback([&cmd, &called](auto packet) {
    EXPECT_TRUE(ContainersEqual(cmd, packet.data()));
    called = true;
  });

  fake_chan()->Receive(cmd);

  RunLoopUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(SignalingChannelTest, UseChannelAfterSignalFree) {
  // Destroy the underlying channel's user (SignalingChannel).
  DestroySig();

  // Ensure that the underlying channel is still alive.
  ASSERT_TRUE(static_cast<bool>(fake_chan()));

  // SignalingChannel is expected to deactivate the channel if it doesn't own
  // it. Either way, the channel isn't in a state that can receive test data.
  EXPECT_FALSE(fake_chan()->activated());

  // Ensure that closing the channel (possibly firing callback) is OK.
  fake_chan()->Close();

  RunLoopUntilIdle();
}

TEST_F(SignalingChannelTest, ValidRequestCommandIds) {
  EXPECT_EQ(0x01, sig()->GetNextCommandId());
  for (int i = 0; i < kMaxCommandId + 1; i++) {
    EXPECT_NE(0x00, sig()->GetNextCommandId());
  }
}

TEST_F(SignalingChannelTest, DoNotRejectUnsolicitedResponse) {
  constexpr CommandId kTestCmdId = 97;
  auto cmd = CreateStaticByteBuffer(
      // Command header (Echo Response, length 1)
      0x09, kTestCmdId, 0x01, 0x00,

      // Payload
      0x23);

  size_t send_count = 0;
  auto send_cb = [&](auto) { send_count++; };
  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());

  fake_chan()->Receive(cmd);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, send_count);
}

TEST_F(SignalingChannelTest, RejectRemoteResponseWithWrongType) {
  constexpr CommandId kReqId = 1;

  // Remote's response with the correct ID but wrong type of response.
  const ByteBuffer& rsp_invalid_id = CreateStaticByteBuffer(
      // Disconnection Response with plausible 4-byte payload.
      0x07, kReqId, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);
  const ByteBuffer& req_data = CreateStaticByteBuffer('P', 'W', 'N');

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; }, dispatcher());

  bool echo_cb_called = false;
  EXPECT_TRUE(sig()->SendRequest(kEchoRequest, req_data, [&echo_cb_called](auto, auto&) {
    echo_cb_called = true;
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  const ByteBuffer& reject_rsp = CreateStaticByteBuffer(
      // Command header (Command Rejected)
      0x01, kReqId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);
  bool reject_sent = false;
  fake_chan()->SetSendCallback(
      [&reject_rsp, &reject_sent](auto cb_packet) {
        reject_sent = ContainersEqual(reject_rsp, *cb_packet);
      },
      dispatcher());

  fake_chan()->Receive(rsp_invalid_id);

  RunLoopUntilIdle();
  EXPECT_FALSE(echo_cb_called);
  EXPECT_TRUE(reject_sent);
}

// Ensure that the signaling channel can reuse outgoing command IDs. In the case
// that it's expecting a response on every single valid command ID, requests
// should fail.
TEST_F(SignalingChannelTest, ReuseCommandIdsUntilExhausted) {
  int req_count = 0;
  constexpr CommandId kRspId = 0x0c;

  auto check_header_id = [&req_count, kRspId](auto cb_packet) {
    req_count++;
    SignalingPacket sent_sig_pkt(cb_packet.get());
    if (req_count == kMaxCommandId + 1) {
      EXPECT_EQ(kRspId, sent_sig_pkt.header().id);
    } else {
      EXPECT_EQ(req_count, sent_sig_pkt.header().id);
    }
  };
  fake_chan()->SetSendCallback(std::move(check_header_id), dispatcher());

  const ByteBuffer& req_data = CreateStaticByteBuffer('y', 'o', 'o', 'o', 'o', '\0');

  for (int i = 0; i < kMaxCommandId; i++) {
    EXPECT_TRUE(sig()->SendRequest(kEchoRequest, req_data, kTestResponseHandler));
  }

  // All command IDs should be exhausted at this point, so no commands of this
  // type should be allowed to be sent.
  EXPECT_FALSE(sig()->SendRequest(kEchoRequest, req_data, kTestResponseHandler));

  RunLoopUntilIdle();
  EXPECT_EQ(kMaxCommandId, req_count);

  // Remote finally responds to a request, but not in order requests were sent.
  // This will free a command ID.
  const ByteBuffer& echo_rsp = CreateStaticByteBuffer(
      // Echo response with no payload.
      0x09, kRspId, 0x00, 0x00);
  fake_chan()->Receive(echo_rsp);

  RunLoopUntilIdle();

  // Request should use freed command ID.
  EXPECT_TRUE(sig()->SendRequest(kEchoRequest, req_data, kTestResponseHandler));

  RunLoopUntilIdle();
  EXPECT_EQ(kMaxCommandId + 1, req_count);
}

// Ensure that a response handler may destroy the signaling channel.
TEST_F(SignalingChannelTest, ResponseHandlerThatDestroysSigDoesNotCrash) {
  fake_chan()->SetSendCallback([](auto) {}, dispatcher());

  const StaticByteBuffer req_data('h', 'e', 'l', 'l', 'o');
  bool rx_success = false;
  EXPECT_TRUE(
      sig()->SendRequest(kEchoRequest, req_data, [this, &rx_success](Status, const ByteBuffer&) {
        rx_success = true;
        DestroySig();
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      }));

  constexpr CommandId kReqId = 1;
  const StaticByteBuffer echo_rsp(
      // Command header (Echo Response, length 1)
      kEchoResponse, kReqId, 0x01, 0x00,

      // Payload
      0x23);
  fake_chan()->Receive(echo_rsp);

  RunLoopUntilIdle();
  EXPECT_FALSE(sig());
  EXPECT_TRUE(rx_success);
}

// Ensure that the signaling channel plumbs a rejection command from remote to
// the appropriate response handler.
TEST_F(SignalingChannelTest, RemoteRejectionPassedToHandler) {
  const ByteBuffer& reject_rsp = StaticByteBuffer(
      // Command header (Command Rejected)
      0x01, 0x01, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; }, dispatcher());

  const ByteBuffer& req_data = StaticByteBuffer('h', 'e', 'l', 'l', 'o');
  bool rx_success = false;
  EXPECT_TRUE(sig()->SendRequest(
      kEchoRequest, req_data,
      [&rx_success, &reject_rsp](Status status, const ByteBuffer& rsp_payload) {
        rx_success = true;
        EXPECT_EQ(Status::kReject, status);
        EXPECT_TRUE(ContainersEqual(reject_rsp.view(sizeof(CommandHeader)), rsp_payload));
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  // Remote sends back a rejection.
  fake_chan()->Receive(reject_rsp);

  RunLoopUntilIdle();
  EXPECT_TRUE(rx_success);
}

TEST_F(SignalingChannelTest, HandlerCompletedByResponseNotCalledAgainAfterRtxTimeout) {
  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; }, dispatcher());

  const StaticByteBuffer req_data('h', 'e', 'l', 'l', 'o');
  int rx_cb_count = 0;
  EXPECT_TRUE(
      sig()->SendRequest(kEchoRequest, req_data, [&rx_cb_count](Status status, const ByteBuffer&) {
        rx_cb_count++;
        EXPECT_EQ(Status::kSuccess, status);
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      }));

  const StaticByteBuffer echo_rsp(
      // Echo response with no payload.
      0x09, 0x01, 0x00, 0x00);
  fake_chan()->Receive(echo_rsp);

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);
  EXPECT_EQ(1, rx_cb_count);

  RunLoopFor(kSignalingChannelResponseTimeout);
  EXPECT_EQ(1, rx_cb_count);
}

// Ensure that the signaling channel calls ResponseHandler with Status::kTimeOut after a request
// times out waiting for a peer response.
TEST_F(SignalingChannelTest, CallHandlerCalledAfterMaxNumberOfRtxTimeoutRetransmissions) {
  size_t send_cb_count = 0;
  auto send_cb = [&](auto cb_packet) {
    SignalingPacket pkt(cb_packet.get());
    EXPECT_EQ(pkt.header().id, 1u);
    send_cb_count++;
  };
  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());

  const StaticByteBuffer req_data('h', 'e', 'l', 'l', 'o');
  bool rx_cb_called = false;
  EXPECT_TRUE(
      sig()->SendRequest(kEchoRequest, req_data, [&rx_cb_called](Status status, const ByteBuffer&) {
        rx_cb_called = true;
        EXPECT_EQ(Status::kTimeOut, status);
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      }));

  RunLoopUntilIdle();
  EXPECT_EQ(send_cb_count, 1u);
  EXPECT_FALSE(rx_cb_called);

  auto timeout = kSignalingChannelResponseTimeout;
  for (size_t i = 1; i < kMaxSignalingChannelTransmissions; i++) {
    // Ensure retransmission doesn't happen before timeout.
    RunLoopFor(timeout - zx::msec(100));
    EXPECT_EQ(send_cb_count, i);

    RunLoopFor(zx::msec(100));
    EXPECT_EQ(send_cb_count, 1 + i);
    EXPECT_FALSE(rx_cb_called);

    timeout *= 2;
  }

  send_cb_count = 0;
  RunLoopFor(timeout);
  EXPECT_EQ(send_cb_count, 0u);
  EXPECT_TRUE(rx_cb_called);
}

TEST_F(SignalingChannelTest, TwoResponsesToARetransmittedOutboundRequest) {
  size_t send_cb_count = 0;
  auto send_cb = [&](auto cb_packet) {
    SignalingPacket pkt(cb_packet.get());
    EXPECT_EQ(pkt.header().id, 1u);
    send_cb_count++;
  };
  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());

  const StaticByteBuffer req_data('h', 'e', 'l', 'l', 'o');
  size_t rx_cb_count = 0;
  EXPECT_TRUE(
      sig()->SendRequest(kEchoRequest, req_data, [&rx_cb_count](Status status, const ByteBuffer&) {
        rx_cb_count++;
        EXPECT_EQ(Status::kSuccess, status);
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      }));

  RunLoopUntilIdle();
  EXPECT_EQ(1u, send_cb_count);
  EXPECT_EQ(0u, rx_cb_count);

  RunLoopFor(kSignalingChannelResponseTimeout);
  EXPECT_EQ(2u, send_cb_count);
  EXPECT_EQ(0u, rx_cb_count);

  const StaticByteBuffer echo_rsp(kEchoResponse, 0x01, 0x00, 0x00);
  fake_chan()->Receive(echo_rsp);
  EXPECT_EQ(2u, send_cb_count);
  EXPECT_EQ(1u, rx_cb_count);

  // Second response should be ignored as it is unexpected.
  fake_chan()->Receive(echo_rsp);
  EXPECT_EQ(2u, send_cb_count);
  EXPECT_EQ(1u, rx_cb_count);
}

// When the response handler expects more responses, use the longer ERTX timeout for the following
// response.
TEST_F(SignalingChannelTest, ExpectAdditionalResponseExtendsRtxTimeoutToErtxTimeout) {
  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; }, dispatcher());

  const StaticByteBuffer req_data{'h', 'e', 'l', 'l', 'o'};
  int rx_cb_calls = 0;
  EXPECT_TRUE(
      sig()->SendRequest(kEchoRequest, req_data, [&rx_cb_calls](Status status, const ByteBuffer&) {
        rx_cb_calls++;
        if (rx_cb_calls <= 2) {
          EXPECT_EQ(Status::kSuccess, status);
        } else {
          EXPECT_EQ(Status::kTimeOut, status);
        }
        return SignalingChannel::ResponseHandlerAction::kExpectAdditionalResponse;
      }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);
  EXPECT_EQ(0, rx_cb_calls);

  const StaticByteBuffer echo_rsp(
      // Echo response with no payload.
      0x09, 0x01, 0x00, 0x00);
  fake_chan()->Receive(echo_rsp);
  EXPECT_EQ(1, rx_cb_calls);

  // The handler expects more responses so the RTX timer shouldn't have expired.
  RunLoopFor(kSignalingChannelResponseTimeout);

  fake_chan()->Receive(echo_rsp);
  EXPECT_EQ(2, rx_cb_calls);

  // The second response should have reset the ERTX timer, so it shouldn't fire yet.
  RunLoopFor(kSignalingChannelExtendedResponseTimeout - zx::msec(100));

  // If the renewed ERTX timer expires without a third response, receive a kTimeOut "response."
  RunLoopFor(zx::sec(1));
  EXPECT_EQ(3, rx_cb_calls);
}

TEST_F(SignalingChannelTest, RegisterRequestResponder) {
  const ByteBuffer& remote_req = StaticByteBuffer(
      // Disconnection Request.
      0x06, 0x01, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);
  const BufferView& expected_payload = remote_req.view(sizeof(CommandHeader));

  auto expected_rej = StaticByteBuffer(
      // Command header (Command rejected, length 2)
      0x01, 0x01, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Receive remote's request before a handler is assigned, expecting an
  // outbound rejection.
  ReceiveAndExpect(remote_req, expected_rej);

  // Register the handler.
  bool cb_called = false;
  sig()->ServeRequest(kDisconnectionRequest,
                      [&cb_called, &expected_payload](const ByteBuffer& req_payload,
                                                      SignalingChannel::Responder* responder) {
                        cb_called = true;
                        EXPECT_TRUE(ContainersEqual(expected_payload, req_payload));
                        responder->Send(req_payload);
                      });

  const ByteBuffer& local_rsp = StaticByteBuffer(
      // Disconnection Response.
      0x07, 0x01, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);

  // Receive the same command again.
  ReceiveAndExpect(remote_req, local_rsp);
  EXPECT_TRUE(cb_called);
}

TEST_F(SignalingChannelTest, DoNotRejectRemoteResponseInvalidId) {
  // Request will use ID = 1.
  constexpr CommandId kIncorrectId = 2;
  // Remote's echo response that has a different ID to what will be in the
  // request header.
  const ByteBuffer& rsp_invalid_id = CreateStaticByteBuffer(
      // Echo response with 4-byte payload.
      0x09, kIncorrectId, 0x04, 0x00,

      // Payload
      'L', '3', '3', 'T');
  const BufferView req_data = rsp_invalid_id.view(sizeof(CommandHeader));

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; }, dispatcher());

  bool echo_cb_called = false;
  EXPECT_TRUE(sig()->SendRequest(kEchoRequest, req_data, [&echo_cb_called](auto, auto&) {
    echo_cb_called = true;
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  bool reject_sent = false;
  fake_chan()->SetSendCallback([&reject_sent](auto cb_packet) { reject_sent = true; },
                               dispatcher());

  fake_chan()->Receive(rsp_invalid_id);

  RunLoopUntilIdle();
  EXPECT_FALSE(echo_cb_called);
  EXPECT_FALSE(reject_sent);
}

}  // namespace
}  // namespace bt::l2cap::internal
