// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_channel_test.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace l2cap {
namespace testing {

fbl::RefPtr<FakeChannel> FakeChannelTest::CreateFakeChannel(const ChannelOptions& options) {
  auto fake_chan = fbl::AdoptRef(
      new FakeChannel(options.id, options.remote_id, options.conn_handle, options.link_type));
  fake_chan_ = fake_chan->AsWeakPtr();
  return fake_chan;
}

bool FakeChannelTest::Expect(const ByteBuffer& expected) {
  if (!fake_chan()) {
    bt_log(ERROR, "testing", "no channel, failing!");
    return false;
  }

  bool success = false;
  auto cb = [&expected, &success](auto cb_packet) {
    success = ContainersEqual(expected, *cb_packet);
  };

  fake_chan()->SetSendCallback(cb, dispatcher());
  RunLoopUntilIdle();

  return success;
}

bool FakeChannelTest::ReceiveAndExpect(const ByteBuffer& packet,
                                       const ByteBuffer& expected_response) {
  if (!fake_chan()) {
    bt_log(ERROR, "testing", "no channel, failing!");
    return false;
  }

  fake_chan()->Receive(packet);

  return Expect(expected_response);
}

}  // namespace testing
}  // namespace l2cap
}  // namespace bt
