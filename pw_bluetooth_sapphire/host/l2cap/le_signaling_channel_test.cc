// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/l2cap/le_signaling_channel.h"

#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::l2cap::internal {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
constexpr uint8_t kTestCmdId = 1;

template <pw::bluetooth::emboss::ConnectionRole Role =
              pw::bluetooth::emboss::ConnectionRole::CENTRAL>
class LESignalingChannelTestBase : public testing::FakeChannelTest {
 public:
  LESignalingChannelTestBase() = default;
  ~LESignalingChannelTestBase() override = default;

 protected:
  void SetUp() override {
    ChannelOptions options(kLESignalingChannelId);
    options.conn_handle = kTestHandle;

    fake_sig_chan_ = CreateFakeChannel(options);
    sig_ = std::make_unique<LESignalingChannel>(
        fake_sig_chan_->GetWeakPtr(), Role, dispatcher());
  }

  void TearDown() override { sig_ = nullptr; }

  LESignalingChannel* sig() const { return sig_.get(); }

 private:
  std::unique_ptr<testing::FakeChannel> fake_sig_chan_;
  std::unique_ptr<LESignalingChannel> sig_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LESignalingChannelTestBase);
};

using LESignalingChannelTest = LESignalingChannelTestBase<>;

using LESignalingChannelPeripheralTest = LESignalingChannelTestBase<
    pw::bluetooth::emboss::ConnectionRole::PERIPHERAL>;

TEST_F(LESignalingChannelTest, IgnoreEmptyFrame) {
  bool send_cb_called = false;
  auto send_cb = [&send_cb_called](auto) { send_cb_called = true; };

  fake_chan()->SetSendCallback(std::move(send_cb), dispatcher());
  fake_chan()->Receive(BufferView());

  RunUntilIdle();
  EXPECT_FALSE(send_cb_called);
}

TEST_F(LESignalingChannelTest, RejectMalformedTooLarge) {
  // Command Reject packet.
  // clang-format off
  StaticByteBuffer expected(
      // Command header
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Header-encoded length is less than the otherwise-valid Connection Parameter
  // Update packet's payload size.
  StaticByteBuffer cmd_with_oversize_payload(
      0x12, kTestCmdId, 0x07, 0x00,

      // Valid connection parameters
      0x06, 0x00,
      0x80, 0x0C,
      0xF3, 0x01,
      0x80, 0x0C);
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(cmd_with_oversize_payload, expected));
}

TEST_F(LESignalingChannelTest, RejectMalformedTooSmall) {
  // Command Reject packet.
  // clang-format off
  StaticByteBuffer expected(
      // Command header
      0x01, kTestCmdId, 0x02, 0x00,

      // Reason (Command not understood)
      0x00, 0x00);

  // Header-encoded length is more than the otherwise-valid Connection Parameter
  // Update packet's payload size.
  StaticByteBuffer cmd_with_undersize_payload(
      0x12, kTestCmdId, 0x09, 0x00,

      // Valid connection parameters
      0x06, 0x00,
      0x80, 0x0C,
      0xF3, 0x01,
      0x80, 0x0C);
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(cmd_with_undersize_payload, expected));
}

TEST_F(LESignalingChannelTest, DefaultMTU) {
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
  StaticByteBuffer expected(
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
}  // namespace bt::l2cap::internal
