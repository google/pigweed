// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_signaling_channel.h"

#include "fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr uint8_t kTestCmdId = 97;
constexpr hci::Connection::Role kDeviceRole = hci::Connection::Role::kMaster;

class L2CAP_BrEdrSignalingChannelTest : public testing::FakeChannelTest {
 public:
  L2CAP_BrEdrSignalingChannelTest() = default;
  ~L2CAP_BrEdrSignalingChannelTest() override = default;

  L2CAP_BrEdrSignalingChannelTest(const L2CAP_BrEdrSignalingChannelTest&) =
      delete;
  L2CAP_BrEdrSignalingChannelTest& operator=(
      const L2CAP_BrEdrSignalingChannelTest&) = delete;

 protected:
  void SetUp() override {
    ChannelOptions options(kSignalingChannelId);
    options.conn_handle = kTestHandle;

    auto fake_chan = CreateFakeChannel(options);
    sig_ = std::make_unique<BrEdrSignalingChannel>(std::move(fake_chan),
                                                   kDeviceRole);
  }

  void TearDown() override { sig_ = nullptr; }

  BrEdrSignalingChannel* sig() const { return sig_.get(); }

 private:
  std::unique_ptr<BrEdrSignalingChannel> sig_;
};

TEST_F(L2CAP_BrEdrSignalingChannelTest, RegisterRequestResponder) {
  const ByteBuffer& remote_req = CreateStaticByteBuffer(
      // Disconnection Request.
      0x06, 0x01, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);
  const BufferView& expected_payload = remote_req.view(4, 4);

  auto expected_rej = CreateStaticByteBuffer(
      // Command header (Command rejected, length 2)
      0x01, 0x01, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Receive remote's request before a handler is assigned, expecting an
  // outbound rejection.
  ReceiveAndExpect(remote_req, expected_rej);

  // Register the handler.
  bool cb_called = false;
  sig()->ServeRequest(
      kDisconnectionRequest,
      [&cb_called, &expected_payload](const ByteBuffer& req_payload,
                                      SignalingChannel::Responder* responder) {
        cb_called = true;
        EXPECT_TRUE(ContainersEqual(expected_payload, req_payload));
        responder->Send(req_payload);
      });

  const ByteBuffer& local_rsp = CreateStaticByteBuffer(
      // Disconnection Response.
      0x07, 0x01, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);

  // Receive the same command again.
  ReceiveAndExpect(remote_req, local_rsp);
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, RespondsToEchoRequest) {
  auto cmd = CreateStaticByteBuffer(
      // Command header (Echo Request, length 1)
      0x08, kTestCmdId, 0x01, 0x00,

      // Payload
      0x23);

  bool called = false;
  auto cb = [&called, &cmd](auto packet) {
    called = true;
    EXPECT_EQ((*packet)[0], 0x09);  // ID for Echo Response
    // Command ID, payload length, and payload should match those of request.
    EXPECT_TRUE(ContainersEqual(cmd.view(1), packet->view(1)));
  };

  fake_chan()->SetSendCallback(std::move(cb), dispatcher());
  fake_chan()->Receive(cmd);

  RunLoopUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, RejectUnsolicitedEchoResponse) {
  auto cmd = CreateStaticByteBuffer(
      // Command header (Echo Response, length 1)
      0x09, kTestCmdId, 0x01, 0x00,

      // Payload
      0x23);

  auto expected = CreateStaticByteBuffer(
      // Command header (Command rejected, length 2)
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunLoopUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, RejectMalformedAdditionalCommand) {
  constexpr uint8_t kTestId0 = 14;
  constexpr uint8_t kTestId1 = 15;

  // Echo Request (see other test for command support), followed by an
  // incomplete command packet
  auto cmd = CreateStaticByteBuffer(
      // Command header (length 3)
      0x08, kTestId0, 0x03, 0x00,

      // Payload data
      'L', 'O', 'L',

      // Second command header
      0x08, kTestId1, 0x01, 0x00);

  // Echo Response packet
  auto rsp0 = CreateStaticByteBuffer(
      // Command header (length 3)
      0x09, kTestId0, 0x03, 0x00,

      // Payload data
      'L', 'O', 'L');

  // Command Reject packet
  auto rsp1 = CreateStaticByteBuffer(
      // Command header
      0x01, kTestId1, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  int cb_times_called = 0;
  auto send_cb = [&rsp0, &rsp1, &cb_times_called](auto packet) {
    if (cb_times_called == 0) {
      EXPECT_TRUE(ContainersEqual(rsp0, *packet));
    } else if (cb_times_called == 1) {
      EXPECT_TRUE(ContainersEqual(rsp1, *packet));
    }

    cb_times_called++;
  };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(cmd);

  RunLoopUntilIdle();
  EXPECT_EQ(2, cb_times_called);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, HandleMultipleCommands) {
  constexpr uint8_t kTestId0 = 14;
  constexpr uint8_t kTestId1 = 15;
  constexpr uint8_t kTestId2 = 16;

  auto cmd = CreateStaticByteBuffer(
      // Command header (Echo Request)
      0x08, kTestId0, 0x04, 0x00,

      // Payload data
      'L', 'O', 'L', 'Z',

      // Header with command to be rejected
      0xFF, kTestId1, 0x03, 0x00,

      // Payload data
      'L', 'O', 'L',

      // Command header (Echo Request, no payload data)
      0x08, kTestId2, 0x00, 0x00,

      // Additional command fragment to be dropped
      0xFF, 0x00);

  auto echo_rsp0 = CreateStaticByteBuffer(
      // Command header (Echo Response)
      0x09, kTestId0, 0x04, 0x00,

      // Payload data
      'L', 'O', 'L', 'Z');

  auto reject_rsp1 = CreateStaticByteBuffer(
      // Command header (Command Rejected)
      0x01, kTestId1, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  auto echo_rsp2 = CreateStaticByteBuffer(
      // Command header (Echo Response)
      0x09, kTestId2, 0x00, 0x00);

  int cb_times_called = 0;
  auto send_cb = [&echo_rsp0, &reject_rsp1, &echo_rsp2,
                  &cb_times_called](auto packet) {
    if (cb_times_called == 0) {
      EXPECT_TRUE(ContainersEqual(echo_rsp0, *packet));
    } else if (cb_times_called == 1) {
      EXPECT_TRUE(ContainersEqual(reject_rsp1, *packet));
    } else if (cb_times_called == 2) {
      EXPECT_TRUE(ContainersEqual(echo_rsp2, *packet));
    }

    cb_times_called++;
  };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(cmd);

  RunLoopUntilIdle();
  EXPECT_EQ(3, cb_times_called);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, SendAndReceiveEcho) {
  const ByteBuffer& expected_req = CreateStaticByteBuffer(
      // Echo request with 3-byte payload.
      0x08, 0x01, 0x03, 0x00,

      // Payload
      'P', 'W', 'N');
  const BufferView req_data = expected_req.view(4, 3);

  // Check the request sent.
  bool tx_success = false;
  fake_chan()->SetSendCallback(
      [&expected_req, &tx_success](auto cb_packet) {
        tx_success = ContainersEqual(expected_req, *cb_packet);
      },
      dispatcher());

  const ByteBuffer& expected_rsp = CreateStaticByteBuffer(
      // Echo response with 4-byte payload.
      0x09, 0x01, 0x04, 0x00,

      // Payload
      'L', '3', '3', 'T');
  const BufferView rsp_data = expected_rsp.view(4, 4);

  bool rx_success = false;
  EXPECT_TRUE(
      sig()->TestLink(req_data, [&rx_success, &rsp_data](const auto& data) {
        rx_success = ContainersEqual(rsp_data, data);
      }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  // Remote sends back an echo response with a different payload than in local
  // request (this is allowed).
  if (tx_success) {
    fake_chan()->Receive(expected_rsp);
  }

  RunLoopUntilIdle();
  EXPECT_TRUE(rx_success);
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, RejectUnhandledResponseCommand) {
  auto cmd = CreateStaticByteBuffer(
      // Command header (Information Response, length 4)
      0x0B, kTestCmdId, 0x04, 0x00,

      // InfoType (Connectionless MTU)
      0x01, 0x00,

      // Result (Not supported)
      0x01, 0x00);

  auto expected = CreateStaticByteBuffer(
      // Command header (Command rejected, length 2)
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

TEST_F(L2CAP_BrEdrSignalingChannelTest, RejectRemoteResponseInvalidId) {
  // Remote's echo response that has a different ID to what will be in the
  // request header (see SendAndReceiveEcho).
  const ByteBuffer& rsp_invalid_id = CreateStaticByteBuffer(
      // Echo response with 4-byte payload.
      0x09, 0x02, 0x04, 0x00,

      // Payload
      'L', '3', '3', 'T');
  const BufferView req_data = rsp_invalid_id.view(4, 4);

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; },
                               dispatcher());

  bool echo_cb_called = false;
  EXPECT_TRUE(sig()->TestLink(
      req_data, [&echo_cb_called](auto&) { echo_cb_called = true; }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  const ByteBuffer& reject_rsp = CreateStaticByteBuffer(
      // Command header (Command Rejected)
      0x01, 0x02, 0x02, 0x00,

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

TEST_F(L2CAP_BrEdrSignalingChannelTest, RejectRemoteResponseWrongType) {
  // Remote's response with the correct ID but wrong type of response.
  const ByteBuffer& rsp_invalid_id = CreateStaticByteBuffer(
      // Disconnection Response with plausible 4-byte payload.
      0x07, 0x01, 0x04, 0x00,

      // Payload
      0x0A, 0x00, 0x08, 0x00);
  const ByteBuffer& req_data = CreateStaticByteBuffer('P', 'W', 'N');

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; },
                               dispatcher());

  bool echo_cb_called = false;
  EXPECT_TRUE(sig()->TestLink(
      req_data, [&echo_cb_called](auto&) { echo_cb_called = true; }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  const ByteBuffer& reject_rsp = CreateStaticByteBuffer(
      // Command header (Command Rejected)
      0x01, 0x01, 0x02, 0x00,

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
TEST_F(L2CAP_BrEdrSignalingChannelTest, ReuseCommandIds) {
  int req_count = 0;
  auto check_header_id = [&req_count](auto cb_packet) {
    req_count++;
    SignalingPacket sent_sig_pkt(cb_packet.get());
    if (req_count == 256) {
      EXPECT_EQ(0x0c, sent_sig_pkt.header().id);
    } else {
      EXPECT_EQ(req_count, sent_sig_pkt.header().id);
    }
  };
  fake_chan()->SetSendCallback(std::move(check_header_id), dispatcher());

  const ByteBuffer& req_data =
      CreateStaticByteBuffer('y', 'o', 'o', 'o', 'o', '\0');

  for (int i = 0; i < 255; i++) {
    EXPECT_TRUE(sig()->TestLink(req_data, [](auto&) {}));
  }

  // All command IDs should be exhausted at this point, so no commands of this
  // type should be allowed to be sent.
  EXPECT_FALSE(sig()->TestLink(req_data, [](auto&) {}));

  RunLoopUntilIdle();
  EXPECT_EQ(255, req_count);

  // Remote finally responds to a request, but not in order requests were sent.
  const ByteBuffer& echo_rsp = CreateStaticByteBuffer(
      // Echo response with no payload.
      0x09, 0x0c, 0x00, 0x00);
  fake_chan()->Receive(echo_rsp);

  RunLoopUntilIdle();

  EXPECT_TRUE(sig()->TestLink(req_data, [](auto&) {}));

  RunLoopUntilIdle();
  EXPECT_EQ(256, req_count);
}

// Ensure that the signaling channel plumbs a rejection command from remote to
// the appropriate response handler.
TEST_F(L2CAP_BrEdrSignalingChannelTest, EchoRemoteRejection) {
  const ByteBuffer& reject_rsp = CreateStaticByteBuffer(
      // Command header (Command Rejected)
      0x01, 0x01, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  bool tx_success = false;
  fake_chan()->SetSendCallback([&tx_success](auto) { tx_success = true; },
                               dispatcher());

  const ByteBuffer& req_data = CreateStaticByteBuffer('h', 'i');
  bool rx_success = false;
  EXPECT_TRUE(sig()->TestLink(req_data, [&rx_success](const ByteBuffer& data) {
    rx_success = true;
    EXPECT_EQ(0U, data.size());
  }));

  RunLoopUntilIdle();
  EXPECT_TRUE(tx_success);

  // Remote sends back a rejection.
  fake_chan()->Receive(reject_rsp);

  RunLoopUntilIdle();
  EXPECT_TRUE(rx_success);
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
