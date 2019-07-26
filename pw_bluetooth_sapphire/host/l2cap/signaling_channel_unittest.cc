// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "signaling_channel.h"

#include "fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

constexpr CommandCode kUnknownCommandCode = 0x00;
constexpr CommandCode kCommandCode = 0xFF;
constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr uint16_t kTestMTU = 100;

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
  // SignalingChannelInterface overrides
  bool SendRequest(CommandCode req_code, const ByteBuffer& payload, ResponseHandler cb) override {
    return false;
  }
  void ServeRequest(CommandCode req_code, RequestDelegate cb) override {}

  // SignalingChannel overrides
  void DecodeRxUnit(ByteBufferPtr sdu, const SignalingPacketHandler& cb) override {
    ZX_DEBUG_ASSERT(sdu);
    if (sdu->size()) {
      cb(SignalingPacket(sdu.get(), sdu->size() - sizeof(CommandHeader)));
    } else {
      // Silently drop the packet. See documentation in signaling_channel.h.
    }
  }

  bool HandlePacket(const SignalingPacket& packet) override {
    if (packet.header().code == kUnknownCommandCode)
      return false;
    if (packet_cb_)
      packet_cb_(packet);
    return true;
  }

  PacketCallback packet_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TestSignalingChannel);
};

class L2CAP_SignalingChannelTest : public testing::FakeChannelTest {
 public:
  L2CAP_SignalingChannelTest() = default;
  ~L2CAP_SignalingChannelTest() override = default;

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

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(L2CAP_SignalingChannelTest);
};

TEST_F(L2CAP_SignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunLoopUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(L2CAP_SignalingChannelTest, Reject) {
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

TEST_F(L2CAP_SignalingChannelTest, RejectCommandCodeZero) {
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

TEST_F(L2CAP_SignalingChannelTest, RejectNotUnderstoodWithResponder) {
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

TEST_F(L2CAP_SignalingChannelTest, RejectInvalidCIdWithResponder) {
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

TEST_F(L2CAP_SignalingChannelTest, InvalidMTU) {
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

TEST_F(L2CAP_SignalingChannelTest, HandlePacket) {
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

TEST_F(L2CAP_SignalingChannelTest, UseChannelAfterSignalFree) {
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

TEST_F(L2CAP_SignalingChannelTest, ValidRequestCommandIds) {
  EXPECT_EQ(0x01, sig()->GetNextCommandId());
  for (int i = 0; i < 256; i++) {
    EXPECT_NE(0x00, sig()->GetNextCommandId());
  }
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
