// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "active_phase.h"

#include <memory>

#include <fbl/macros.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {
namespace {
using Listener = PairingPhase::Listener;
using PairingChannelHandler = PairingChannel::Handler;

class ConcreteActivePhase : public ActivePhase, public PairingChannelHandler {
 public:
  ConcreteActivePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                      hci::Connection::Role role,
                      size_t max_packet_size = sizeof(PairingPublicKeyParams))
      : ActivePhase(std::move(chan), std::move(listener), role), weak_ptr_factory_(this) {
    // All concrete pairing phases should set themselves as the pairing channel handler.
    sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
    last_rx_packet_ = DynamicByteBuffer(max_packet_size);
  }

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() final {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // ActivePhase override, not tested as ActivePhase does not implement this pure virtual function.
  void Start() override {}

  // PairingChannelHandler override
  void OnChannelClosed() override { ActivePhase::HandleChannelClosed(); }
  void OnRxBFrame(ByteBufferPtr sdu) override { sdu->Copy(&last_rx_packet_); }

  const ByteBuffer& last_rx_packet() { return last_rx_packet_; }

 private:
  fxl::WeakPtrFactory<ConcreteActivePhase> weak_ptr_factory_;
  DynamicByteBuffer last_rx_packet_;
};

class SMP_ActivePhaseTest : public l2cap::testing::FakeChannelTest {
 public:
  SMP_ActivePhaseTest() = default;
  ~SMP_ActivePhaseTest() override = default;

 protected:
  void SetUp() override { NewActivePhase(); }

  void TearDown() override { active_phase_ = nullptr; }

  void NewActivePhase(hci::Connection::Role role = hci::Connection::Role::kMaster,
                      hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    active_phase_ = std::make_unique<ConcreteActivePhase>(sm_chan_->GetWeakPtr(),
                                                          listener_->as_weak_ptr(), role);
  }

  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  FakeListener* listener() { return listener_.get(); }
  ConcreteActivePhase* active_phase() { return active_phase_.get(); }

 private:
  std::unique_ptr<FakeListener> listener_;
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<ConcreteActivePhase> active_phase_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_ActivePhaseTest);
};

using SMP_ActivePhaseDeathTest = SMP_ActivePhaseTest;

TEST_F(SMP_ActivePhaseDeathTest, CallMethodOnFailedPhaseDies) {
  active_phase()->Abort(ErrorCode::kUnspecifiedReason);
  ASSERT_DEATH_IF_SUPPORTED(active_phase()->OnPairingTimeout(), ".*failed.*");
}

TEST_F(SMP_ActivePhaseTest, ChannelClosedNotifiesListener) {
  ASSERT_EQ(listener()->last_error().error(), HostError::kNoError);
  ASSERT_EQ(listener()->pairing_error_count(), 0);
  fake_chan()->Close();
  RunLoopUntilIdle();
  ASSERT_EQ(listener()->pairing_error_count(), 1);
  ASSERT_EQ(listener()->last_error().error(), HostError::kLinkDisconnected);
}

TEST_F(SMP_ActivePhaseTest, OnFailureNotifiesListener) {
  auto ecode = ErrorCode::kDHKeyCheckFailed;
  ASSERT_EQ(listener()->last_error().error(), HostError::kNoError);
  ASSERT_EQ(listener()->pairing_error_count(), 0);
  active_phase()->OnFailure(Status(ecode));
  RunLoopUntilIdle();
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  ASSERT_EQ(listener()->last_error().protocol_error(), ecode);
  ASSERT_EQ(listener()->pairing_error_count(), 1);
}

TEST_F(SMP_ActivePhaseTest, AbortSendsFailureMessageAndNotifiesListener) {
  ByteBufferPtr msg_sent = nullptr;
  fake_chan()->SetSendCallback([&msg_sent](ByteBufferPtr sdu) { msg_sent = std::move(sdu); },
                               dispatcher());
  ASSERT_EQ(0, listener()->pairing_error_count());

  active_phase()->Abort(ErrorCode::kDHKeyCheckFailed);
  RunLoopUntilIdle();

  // Check the PairingFailed message was sent to the channel
  ASSERT_TRUE(msg_sent);
  auto reader = PacketReader(msg_sent.get());
  ASSERT_EQ(ErrorCode::kDHKeyCheckFailed, reader.payload<ErrorCode>());

  // Check the listener PairingFailed callback was made.
  ASSERT_EQ(1, listener()->pairing_error_count());
  Status failure_status = listener()->last_error();
  ASSERT_TRUE(failure_status.is_protocol_error());
  ASSERT_EQ(ErrorCode::kDHKeyCheckFailed, failure_status.protocol_error());
}

TEST_F(SMP_ActivePhaseTest, PairingTimeoutDisconnectsLinkAndFails) {
  bool link_disconnected = false;
  fake_chan()->SetLinkErrorCallback([&link_disconnected]() { link_disconnected = true; },
                                    dispatcher());
  ASSERT_EQ(0, listener()->pairing_error_count());

  active_phase()->OnPairingTimeout();
  RunLoopUntilIdle();

  ASSERT_TRUE(link_disconnected);

  // Check the listener PairingFailed callback was made.
  ASSERT_EQ(1, listener()->pairing_error_count());
  Status failure_status = listener()->last_error();
  ASSERT_EQ(HostError::kTimedOut, failure_status.error());
}

}  // namespace
}  // namespace sm
}  // namespace bt
