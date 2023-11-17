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

#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel_test.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::l2cap::testing {

void FakeChannelTest::SetUp() {}

std::unique_ptr<FakeChannel> FakeChannelTest::CreateFakeChannel(
    const ChannelOptions& options) {
  auto fake_chan = std::make_unique<FakeChannel>(
      options.id,
      options.remote_id,
      options.conn_handle,
      options.link_type,
      ChannelInfo::MakeBasicMode(options.mtu, options.mtu));
  fake_chan_ = fake_chan->AsWeakPtr();
  return fake_chan;
}

bool FakeChannelTest::Expect(const ByteBuffer& expected) {
  return ExpectAfterMaybeReceiving(std::nullopt, expected);
}

bool FakeChannelTest::ReceiveAndExpect(const ByteBuffer& packet,
                                       const ByteBuffer& expected_response) {
  return ExpectAfterMaybeReceiving(packet.view(), expected_response);
}

bool FakeChannelTest::ExpectAfterMaybeReceiving(
    std::optional<BufferView> packet, const ByteBuffer& expected) {
  if (!fake_chan().is_alive()) {
    bt_log(ERROR, "testing", "no channel, failing!");
    return false;
  }

  bool success = false;
  auto cb = [&expected, &success](auto cb_packet) {
    success = ContainersEqual(expected, *cb_packet);
  };

  fake_chan()->SetSendCallback(cb, dispatcher());
  if (packet.has_value()) {
    fake_chan()->Receive(packet.value());
  }
  RunUntilIdle();

  return success;
}

}  // namespace bt::l2cap::testing
