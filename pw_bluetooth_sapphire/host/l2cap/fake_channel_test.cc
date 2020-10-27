// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_channel_test.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt::l2cap::testing {

void FakeChannelTest::SetUp() {}

fbl::RefPtr<FakeChannel> FakeChannelTest::CreateFakeChannel(const ChannelOptions& options) {
  auto fake_chan = fbl::AdoptRef(
      new FakeChannel(options.id, options.remote_id, options.conn_handle, options.link_type,
                      ChannelInfo::MakeBasicMode(options.mtu, options.mtu)));
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

bool FakeChannelTest::ExpectAfterMaybeReceiving(std::optional<BufferView> packet,
                                                const ByteBuffer& expected) {
  if (!fake_chan()) {
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
  RunLoopUntilIdle();

  return success;
}

}  // namespace bt::l2cap::testing
