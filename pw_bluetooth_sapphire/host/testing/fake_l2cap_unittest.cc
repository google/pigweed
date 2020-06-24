// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_l2cap.h"

#include <lib/gtest/test_loop_fixture.h>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace testing {

class TESTING_FakeL2capTest : public gtest::TestLoopFixture {
 public:
  TESTING_FakeL2capTest() = default;
  ~TESTING_FakeL2capTest() override = default;

  void SetUp() override {
    TestLoopFixture::SetUp();
    fake_l2cap_ = std::make_unique<FakeL2cap>();
  }

  FakeL2cap& fake_l2cap() { return *fake_l2cap_; }

 private:
  std::unique_ptr<FakeL2cap> fake_l2cap_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(TESTING_FakeL2capTest);
};

TEST_F(TESTING_FakeL2capTest, RegisterHandler) {
  size_t n_pdus = 0;
  auto cb = [&](auto conn, auto& buffer) {
    ++n_pdus;
    EXPECT_TRUE(ContainersEqual(StaticByteBuffer(0x23), buffer));
  };

  fake_l2cap().RegisterHandler(l2cap::kSignalingChannelId, cb);
  StaticByteBuffer sample_packet = StaticByteBuffer(
      // L2CAP B-Frame header for signaling channel packet.
      // Length 0x0001
      0x01, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Payload: "#"
      0x23);
  fake_l2cap().HandlePdu(0x01, sample_packet);
  EXPECT_EQ(1u, n_pdus);
}

TEST_F(TESTING_FakeL2capTest, CallHandlerMultipleTimes) {
  size_t n_pdus = 0;
  auto cb = [&](auto conn, auto& buffer) {
    ++n_pdus;
    EXPECT_TRUE(ContainersEqual(StaticByteBuffer(0x23), buffer));
  };

  fake_l2cap().RegisterHandler(l2cap::kSignalingChannelId, cb);
  StaticByteBuffer sample_packet = StaticByteBuffer(
      // L2CAP B-Frame header for signaling channel packet.
      // Length 0x0001
      0x01, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Payload: "#"
      0x23);
  fake_l2cap().HandlePdu(0x01, sample_packet);
  EXPECT_EQ(1u, n_pdus);
  fake_l2cap().HandlePdu(0x01, sample_packet);
  EXPECT_EQ(2u, n_pdus);
}

TEST_F(TESTING_FakeL2capTest, CustomUnexpectedPacketHandler) {
  size_t n_pdus = 0;
  auto cb = [&](auto conn, auto& buffer) {
    ++n_pdus;
    EXPECT_TRUE(ContainersEqual(StaticByteBuffer(0x01, 0x00, 0x01, 0x00, 0x23), buffer));
  };

  FakeL2cap fake_l2cap_with_handler(cb);
  StaticByteBuffer sample_packet = StaticByteBuffer(
      // L2CAP B-Frame header for signaling channel packet.
      // Length 0x0001
      0x01, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Payload: "#"
      0x23);
  fake_l2cap_with_handler.HandlePdu(0x01, sample_packet);
  EXPECT_EQ(1u, n_pdus);
}

TEST_F(TESTING_FakeL2capTest, DefaultUnexpectedPacketHandler) {
  size_t n_pdus = 0;
  auto cb = [&](auto conn, auto& buffer) { ++n_pdus; };
  fake_l2cap().RegisterHandler(l2cap::kConnectionlessChannelId, cb);
  StaticByteBuffer sample_packet = StaticByteBuffer(
      // L2CAP B-Frame header for signaling channel packet.
      // Length 0x0001
      0x01, 0x00,
      // Channel Id: 0x0001
      LowerBits(l2cap::kSignalingChannelId), UpperBits(l2cap::kSignalingChannelId),
      // Payload: "#"
      0x23);
  fake_l2cap().HandlePdu(0x01, sample_packet);

  // As the ChannelIds of the registered handler and the received packet are
  // different, cb should not be called and n_pdus should still be 0 as the
  // default packet handler ignores unroutable packets.
  EXPECT_EQ(0u, n_pdus);
}

}  // namespace testing
}  // namespace bt
