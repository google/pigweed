// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_channel.h"

#include <memory>

#include "fbl/ref_ptr.h"
#include "lib/fit/function.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {
namespace {

class FakeChannelHandler : public PairingChannel::Handler {
 public:
  FakeChannelHandler() : weak_ptr_factory_(this) {}

  void OnRxBFrame(ByteBufferPtr data) override {
    last_rx_data_ = std::move(data);
    frames_received_++;
  }

  void OnChannelClosed() override { channel_closed_count_++; }

  ByteBuffer* last_rx_data() { return last_rx_data_.get(); }

  int frames_received() const { return frames_received_; }
  int channel_closed_count() const { return channel_closed_count_; }
  fxl::WeakPtr<PairingChannel::Handler> as_weak_handler() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  ByteBufferPtr last_rx_data_ = nullptr;
  int frames_received_ = 0;
  int channel_closed_count_ = 0;
  fxl::WeakPtrFactory<PairingChannel::Handler> weak_ptr_factory_;
};

class PairingChannelTest : public l2cap::testing::FakeChannelTest {
 protected:
  void SetUp() override { NewPairingChannel(); }

  void TearDown() override {
    sm_chan_ = nullptr;
    fake_sm_chan_ = nullptr;
  }

  void NewPairingChannel(bt::LinkType ll_type = bt::LinkType::kLE,
                         uint16_t mtu = kNoSecureConnectionsMtu) {
    l2cap::ChannelId cid =
        ll_type == bt::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid, mtu);
    options.link_type = ll_type;
    fake_sm_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(
        fake_sm_chan_->GetWeakPtr(), fit::bind_member<&PairingChannelTest::ResetTimer>(this));
  }

  PairingChannel* sm_chan() { return sm_chan_.get(); }
  void set_timer_resetter(fit::closure timer_resetter) {
    timer_resetter_ = std::move(timer_resetter);
  }

 private:
  void ResetTimer() { timer_resetter_(); }

  std::unique_ptr<l2cap::testing::FakeChannel> fake_sm_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  fit::closure timer_resetter_ = []() {};
};

using PairingChannelDeathTest = PairingChannelTest;
TEST_F(PairingChannelDeathTest, L2capChannelMtuTooSmallDies) {
  ASSERT_DEATH_IF_SUPPORTED(NewPairingChannel(bt::LinkType::kLE, kNoSecureConnectionsMtu - 1),
                            ".*max.*_sdu_size.*");
}

TEST_F(PairingChannelDeathTest, SendInvalidMessageDies) {
  // Tests that an invalid SMP code aborts the process
  EXPECT_DEATH_IF_SUPPORTED(sm_chan()->SendMessage(0xFF, ErrorCode::kUnspecifiedReason), ".*end.*");

  // Tests that a valid SMP code with a mismatched payload aborts the process
  EXPECT_DEATH_IF_SUPPORTED(sm_chan()->SendMessage(kPairingFailed, PairingRequestParams{}),
                            ".*sizeof.*");
}

TEST_F(PairingChannelTest, SendMessageWorks) {
  PairingRandomValue kExpectedPayload = {1, 2, 3, 4, 5};
  StaticByteBuffer<util::PacketSize<PairingRandomValue>()> kExpectedPacket;
  PacketWriter w(kPairingRandom, &kExpectedPacket);
  *w.mutable_payload<PairingRandomValue>() = kExpectedPayload;
  bool timer_reset = false;
  set_timer_resetter([&]() { timer_reset = true; });
  sm_chan()->SendMessage(kPairingRandom, kExpectedPayload);
  Expect(kExpectedPacket);
  ASSERT_TRUE(timer_reset);
}

// This checks that PairingChannel doesn't crash when receiving events without a handler set.
TEST_F(PairingChannelTest, NoHandlerSetDataDropped) {
  ASSERT_TRUE(sm_chan());
  const StaticByteBuffer kSmPacket(kPairingFailed, ErrorCode::kPairingNotSupported);

  fake_chan()->Receive(kSmPacket);
  RunLoopUntilIdle();

  fake_chan()->Close();
  RunLoopUntilIdle();
}

TEST_F(PairingChannelTest, SetHandlerReceivesData) {
  ASSERT_TRUE(sm_chan());
  const StaticByteBuffer kSmPacket1(kPairingFailed, ErrorCode::kPairingNotSupported);
  const StaticByteBuffer kSmPacket2(kPairingFailed, ErrorCode::kConfirmValueFailed);
  FakeChannelHandler handler;
  sm_chan()->SetChannelHandler(handler.as_weak_handler());
  ASSERT_EQ(handler.last_rx_data(), nullptr);
  ASSERT_EQ(handler.frames_received(), 0);

  fake_chan()->Receive(kSmPacket1);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  fake_chan()->Receive(kSmPacket2);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket2));
  ASSERT_EQ(handler.frames_received(), 2);

  fake_chan()->Close();
  RunLoopUntilIdle();
  ASSERT_EQ(handler.channel_closed_count(), 1);
}

TEST_F(PairingChannelTest, ChangeHandlerNewHandlerReceivesData) {
  ASSERT_TRUE(sm_chan());
  const StaticByteBuffer kSmPacket1(kPairingFailed, ErrorCode::kPairingNotSupported);
  const StaticByteBuffer kSmPacket2(kPairingFailed, ErrorCode::kConfirmValueFailed);
  FakeChannelHandler handler;
  sm_chan()->SetChannelHandler(handler.as_weak_handler());
  ASSERT_EQ(handler.last_rx_data(), nullptr);
  ASSERT_EQ(handler.frames_received(), 0);

  fake_chan()->Receive(kSmPacket1);
  RunLoopUntilIdle();
  ASSERT_NE(handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  FakeChannelHandler new_handler;
  ASSERT_EQ(new_handler.last_rx_data(), nullptr);
  sm_chan()->SetChannelHandler(new_handler.as_weak_handler());
  fake_chan()->Receive(kSmPacket2);
  RunLoopUntilIdle();
  ASSERT_NE(new_handler.last_rx_data(), nullptr);
  EXPECT_TRUE(ContainersEqual(*new_handler.last_rx_data(), kSmPacket2));
  ASSERT_EQ(new_handler.frames_received(), 1);
  // Check handler's data hasn't changed.
  EXPECT_TRUE(ContainersEqual(*handler.last_rx_data(), kSmPacket1));
  ASSERT_EQ(handler.frames_received(), 1);

  fake_chan()->Close();
  RunLoopUntilIdle();
  ASSERT_EQ(new_handler.channel_closed_count(), 1);
  ASSERT_EQ(handler.channel_closed_count(), 0);
}
}  // namespace
}  // namespace bt::sm
