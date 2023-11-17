// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/l2cap/mock_channel_test.h"

#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::l2cap::testing {

MockChannelTest::Transaction::Transaction(
    const ByteBuffer& expected,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta)
    : expected_({DynamicByteBuffer(expected), meta}) {
  for (const ByteBuffer* buffer : replies) {
    replies_.push(DynamicByteBuffer(*buffer));
  }
}

bool MockChannelTest::Transaction::Match(const ByteBuffer& packet) {
  return ContainersEqual(expected_.data, packet);
}

void MockChannelTest::TearDown() {
  while (!transactions_.empty()) {
    Transaction& transaction = transactions_.front();
    ExpectationMetadata meta = transaction.expected().meta;
    ADD_FAILURE_AT(meta.file, meta.line)
        << "Didn't receive expected outbound packet (" << meta.expectation
        << ") {" << ByteContainerToString(transaction.expected().data) << "}";
    transactions_.pop();
  }
}

void MockChannelTest::QueueTransaction(
    const ByteBuffer& expected,
    const std::vector<const ByteBuffer*>& replies,
    ExpectationMetadata meta) {
  transactions_.emplace(expected, replies, meta);
}

FakeChannel::WeakPtr MockChannelTest::CreateFakeChannel(
    const ChannelOptions& options) {
  fake_chan_ = std::make_unique<FakeChannel>(
      options.id,
      options.remote_id,
      options.conn_handle,
      options.link_type,
      ChannelInfo::MakeBasicMode(options.mtu, options.mtu));
  fake_chan_->SetSendCallback(
      fit::bind_member<&MockChannelTest::OnPacketSent>(this));
  return fake_chan_->AsWeakPtr();
}

void MockChannelTest::OnPacketSent(std::unique_ptr<ByteBuffer> packet) {
  if (packet_callback_) {
    packet_callback_(*packet);
  }

  ASSERT_FALSE(transactions_.empty()) << "Received unexpected packet: { "
                                      << ByteContainerToString(*packet) << "}";

  Transaction expected = std::move(transactions_.front());
  transactions_.pop();
  if (!expected.Match(packet->view())) {
    auto meta = expected.expected().meta;
    GTEST_FAIL_AT(meta.file, meta.line)
        << "Expected packet (" << meta.expectation << ")";
  }

  while (!expected.replies().empty()) {
    auto reply = std::move(expected.replies().front());
    expected.replies().pop();
    // Post tasks to simulate real inbound packets, which are asynchronous.
    (void)heap_dispatcher().Post(
        [this, reply = std::move(reply)](pw::async::Context /*ctx*/,
                                         pw::Status status) {
          if (status.ok()) {
            fake_chan_->Receive(reply);
          }
        });
  }
}

}  // namespace bt::l2cap::testing
