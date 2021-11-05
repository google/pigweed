// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bredr_signaling_channel.h"

#include "fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::l2cap::internal {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
constexpr uint8_t kTestCmdId = 97;
constexpr hci::Connection::Role kDeviceRole = hci::Connection::Role::kCentral;

class BrEdrSignalingChannelTest : public testing::FakeChannelTest {
 public:
  BrEdrSignalingChannelTest() = default;
  ~BrEdrSignalingChannelTest() override = default;

  BrEdrSignalingChannelTest(const BrEdrSignalingChannelTest&) = delete;
  BrEdrSignalingChannelTest& operator=(const BrEdrSignalingChannelTest&) = delete;

 protected:
  void SetUp() override {
    ChannelOptions options(kSignalingChannelId);
    options.conn_handle = kTestHandle;

    auto fake_chan = CreateFakeChannel(options);
    sig_ = std::make_unique<BrEdrSignalingChannel>(std::move(fake_chan), kDeviceRole);
  }

  void TearDown() override { sig_ = nullptr; }

  BrEdrSignalingChannel* sig() const { return sig_.get(); }

 private:
  std::unique_ptr<BrEdrSignalingChannel> sig_;
};

TEST_F(BrEdrSignalingChannelTest, RespondsToEchoRequest) {
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

TEST_F(BrEdrSignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunLoopUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(BrEdrSignalingChannelTest, RejectMalformedAdditionalCommand) {
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

TEST_F(BrEdrSignalingChannelTest, HandleMultipleCommands) {
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
  auto send_cb = [&echo_rsp0, &reject_rsp1, &echo_rsp2, &cb_times_called](auto packet) {
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

TEST_F(BrEdrSignalingChannelTest, SendAndReceiveEcho) {
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
  EXPECT_TRUE(sig()->TestLink(req_data, [&rx_success, &rsp_data](const auto& data) {
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

}  // namespace
}  // namespace bt::l2cap::internal
