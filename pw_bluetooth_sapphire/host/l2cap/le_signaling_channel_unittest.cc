// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "le_signaling_channel.h"

#include "fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/lib/fxl/arraysize.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr uint8_t kTestCmdId = 1;

template <hci::Connection::Role Role = hci::Connection::Role::kMaster>
class LESignalingChannelTest : public testing::FakeChannelTest {
 public:
  LESignalingChannelTest() = default;
  ~LESignalingChannelTest() override = default;

 protected:
  void SetUp() override {
    ChannelOptions options(kLESignalingChannelId);
    options.conn_handle = kTestHandle;

    auto fake_chan = CreateFakeChannel(options);
    sig_ = std::make_unique<LESignalingChannel>(std::move(fake_chan), Role);
  }

  void TearDown() override { sig_ = nullptr; }

  LESignalingChannel* sig() const { return sig_.get(); }

 private:
  std::unique_ptr<LESignalingChannel> sig_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannelTest);
};

using L2CAP_LESignalingChannelTest = LESignalingChannelTest<>;

using L2CAP_LESignalingChannelSlaveTest = LESignalingChannelTest<hci::Connection::Role::kSlave>;

TEST_F(L2CAP_LESignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunLoopUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(L2CAP_LESignalingChannelTest, RejectMalformedTooLarge) {
  // Command Reject packet.
  // clang-format off
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Header-encoded length is less than the otherwise-valid Connection Parameter
  // Update packet's payload size.
  auto cmd_with_oversize_payload = CreateStaticByteBuffer(
      0x12, kTestCmdId, 0x07, 0x00,

      // Valid connection parameters
      0x06, 0x00,
      0x80, 0x0C,
      0xF3, 0x01,
      0x80, 0x0C);
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(cmd_with_oversize_payload, expected));
}

TEST_F(L2CAP_LESignalingChannelTest, RejectMalformedTooSmall) {
  // Command Reject packet.
  // clang-format off
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Header-encoded length is more than the otherwise-valid Connection Parameter
  // Update packet's payload size.
  auto cmd_with_undersize_payload = CreateStaticByteBuffer(
      0x12, kTestCmdId, 0x09, 0x00,

      // Valid connection parameters
      0x06, 0x00,
      0x80, 0x0C,
      0xF3, 0x01,
      0x80, 0x0C);
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(cmd_with_undersize_payload, expected));
}

TEST_F(L2CAP_LESignalingChannelTest, DefaultMTU) {
  constexpr size_t kCommandSize = kMinLEMTU + 1;

  // The channel should start out with the minimum MTU as the default (23
  // octets).
  StaticByteBuffer<kCommandSize> cmd;

  // Make sure that the packet is well formed (the command code does not
  // matter).
  MutableSignalingPacket packet(&cmd, kCommandSize - sizeof(CommandHeader));
  packet.mutable_header()->id = kTestCmdId;
  packet.mutable_header()->length = htole16(packet.payload_size());

  // Command Reject packet.
  // clang-format off
  auto expected = CreateStaticByteBuffer(
      // Command header
      0x01, kTestCmdId, 0x04, 0x00,

      // Reason (Signaling MTU exceeded)
      0x01, 0x00,

      // The supported MTU (23)
      0x17, 0x00);
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(cmd, expected));
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
