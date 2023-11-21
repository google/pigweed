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

#include "pw_bluetooth_sapphire/internal/host/sm/pairing_phase.h"

#include <gtest/gtest.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel_test.h"
#include "pw_bluetooth_sapphire/internal/host/sm/fake_phase_listener.h"
#include "pw_bluetooth_sapphire/internal/host/sm/packet.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

namespace bt::sm {
namespace {
using Listener = PairingPhase::Listener;
using PairingChannelHandler = PairingChannel::Handler;

class ConcretePairingPhase : public PairingPhase, public PairingChannelHandler {
 public:
  ConcretePairingPhase(PairingChannel::WeakPtr chan,
                       Listener::WeakPtr listener,
                       Role role,
                       size_t max_packet_size = sizeof(PairingPublicKeyParams))
      : PairingPhase(std::move(chan), std::move(listener), role),
        weak_handler_(this) {
    // All concrete pairing phases should set themselves as the pairing channel
    // handler.
    SetPairingChannelHandler(*this);
    last_rx_packet_ = DynamicByteBuffer(max_packet_size);
  }

  // All concrete pairing phases should invalidate the channel handler in their
  // destructor.
  ~ConcretePairingPhase() override { InvalidatePairingChannelHandler(); }

  // PairingPhase overrides.
  std::string ToStringInternal() override { return ""; }

  // PairingPhase override, not tested as PairingPhase does not implement this
  // pure virtual function.
  void Start() override {}

  // PairingChannelHandler override
  void OnChannelClosed() override { PairingPhase::HandleChannelClosed(); }
  void OnRxBFrame(ByteBufferPtr sdu) override { sdu->Copy(&last_rx_packet_); }

  const ByteBuffer& last_rx_packet() { return last_rx_packet_; }

 private:
  WeakSelf<PairingChannelHandler> weak_handler_;
  DynamicByteBuffer last_rx_packet_;
};

class PairingPhaseTest : public l2cap::testing::FakeChannelTest {
 public:
  PairingPhaseTest() = default;
  ~PairingPhaseTest() override = default;

 protected:
  void SetUp() override { NewPairingPhase(); }

  void TearDown() override { pairing_phase_ = nullptr; }

  void NewPairingPhase(Role role = Role::kInitiator,
                       bt::LinkType ll_type = bt::LinkType::kLE) {
    l2cap::ChannelId cid = ll_type == bt::LinkType::kLE ? l2cap::kLESMPChannelId
                                                        : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_->GetWeakPtr());
    pairing_phase_ = std::make_unique<ConcretePairingPhase>(
        sm_chan_->GetWeakPtr(), listener_->as_weak_ptr(), role);
  }

  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  FakeListener* listener() { return listener_.get(); }
  ConcretePairingPhase* pairing_phase() { return pairing_phase_.get(); }

 private:
  std::unique_ptr<FakeListener> listener_;
  std::unique_ptr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<ConcretePairingPhase> pairing_phase_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingPhaseTest);
};

using PairingPhaseDeathTest = PairingPhaseTest;

TEST_F(PairingPhaseDeathTest, CallMethodOnFailedPhaseDies) {
  pairing_phase()->Abort(ErrorCode::kUnspecifiedReason);
  ASSERT_DEATH_IF_SUPPORTED(
      pairing_phase()->OnFailure(Error(HostError::kFailed)), ".*failed.*");
}

TEST_F(PairingPhaseTest, ChannelClosedNotifiesListener) {
  ASSERT_EQ(listener()->pairing_error_count(), 0);
  fake_chan()->Close();
  RunUntilIdle();
  ASSERT_EQ(listener()->pairing_error_count(), 1);
  EXPECT_EQ(Error(HostError::kLinkDisconnected), listener()->last_error());
}

TEST_F(PairingPhaseTest, OnFailureNotifiesListener) {
  auto ecode = ErrorCode::kDHKeyCheckFailed;
  ASSERT_EQ(listener()->pairing_error_count(), 0);
  pairing_phase()->OnFailure(Error(ecode));
  RunUntilIdle();
  ASSERT_EQ(listener()->pairing_error_count(), 1);
  EXPECT_EQ(Error(ecode), listener()->last_error());
}

TEST_F(PairingPhaseTest, AbortSendsFailureMessageAndNotifiesListener) {
  ByteBufferPtr msg_sent = nullptr;
  fake_chan()->SetSendCallback(
      [&msg_sent](ByteBufferPtr sdu) { msg_sent = std::move(sdu); },
      dispatcher());
  ASSERT_EQ(0, listener()->pairing_error_count());

  pairing_phase()->Abort(ErrorCode::kDHKeyCheckFailed);
  RunUntilIdle();

  // Check the PairingFailed message was sent to the channel
  ASSERT_TRUE(msg_sent);
  auto reader = PacketReader(msg_sent.get());
  ASSERT_EQ(ErrorCode::kDHKeyCheckFailed, reader.payload<ErrorCode>());

  // Check the listener PairingFailed callback was made.
  ASSERT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kDHKeyCheckFailed), listener()->last_error());
}

}  // namespace
}  // namespace bt::sm
