// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_TEST_H_

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/lib/fxl/macros.h"
#include "lib/gtest/test_loop_fixture.h"

namespace bt {
namespace l2cap {
namespace testing {

// Provides a common GTest harness base class for protocols tests that operate
// over a L2CAP channel. This harness provides:
//
//   * A simple way to initialize and access a FakeChannel.
//   * Basic command<->response expectation.
class FakeChannelTest : public ::gtest::TestLoopFixture {
 public:
  FakeChannelTest() = default;
  ~FakeChannelTest() override = default;

 protected:
  struct ChannelOptions {
    explicit ChannelOptions(ChannelId id) : ChannelOptions(id, id) {}
    ChannelOptions(ChannelId id, ChannelId remote_id)
        : id(id), remote_id(remote_id) {}

    ChannelId id;
    ChannelId remote_id;
    hci::ConnectionHandle conn_handle = 0x0001;
    hci::Connection::LinkType link_type = hci::Connection::LinkType::kLE;
  };

  void SetUp() override{};

  // Creates a new FakeChannel and returns it. A fxl::WeakPtr to the returned
  // channel is stored internally so that the returned channel can be accessed
  // by tests even if its ownership is passed outside of the test harness.
  fbl::RefPtr<FakeChannel> CreateFakeChannel(const ChannelOptions& options);

  // Runs the event loop and returns true if |expected| is received within a 10
  // second period.
  //
  // Returns false if no such response is received or no FakeChannel has been
  // initialized via CreateFakeChannel().
  //
  // NOTE: This overwrites the underlying FakeChannel's "send callback" by
  // calling FakeChannel::SetSendCallback().
  bool Expect(const common::ByteBuffer& expected);

  // Emulates the receipt of |packet| and returns true if a response that
  // matches |expected_response| is sent back over the underlying FakeChannel.
  // Returns false if no such response is received or no FakeChannel has been
  // initialized via CreateFakeChannel().
  //
  // NOTE: This overwrites the underlying FakeChannel's "send callback" by
  // calling FakeChannel::SetSendCallback().
  bool ReceiveAndExpect(const common::ByteBuffer& packet,
                        const common::ByteBuffer& expected_response);

  fxl::WeakPtr<FakeChannel> fake_chan() const { return fake_chan_; }

 private:
  fxl::WeakPtr<FakeChannel> fake_chan_;

  FXL_DISALLOW_COPY_AND_ASSIGN(FakeChannelTest);
};

}  // namespace testing
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_FAKE_CHANNEL_TEST_H_
