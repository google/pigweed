// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_phase.h"

#include <algorithm>
#include <memory>

#include <fbl/macros.h>

#include "lib/fit/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace bt {
namespace sm {
namespace {
using Listener = PairingPhase::Listener;
using PairingChannelHandler = PairingChannel::Handler;

class ConcretePairingPhase : public PairingPhase, public PairingChannelHandler {
 public:
  ConcretePairingPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                       hci::Connection::Role role,
                       size_t max_packet_size = sizeof(PairingPublicKeyParams))
      : PairingPhase(std::move(chan), std::move(listener), role), weak_ptr_factory_(this) {
    // All concrete pairing phases should set themselves as the pairing channel handler.
    sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
    last_rx_packet_ = DynamicByteBuffer(max_packet_size);
  }

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() final {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // PairingChannelHandler overrides
  void OnRxBFrame(ByteBufferPtr sdu) override { sdu->Copy(&last_rx_packet_); }
  void OnChannelClosed() override {}

  // Expose protected methods for testing.
  void SendPairingFailed(ErrorCode ecode) { PairingPhase::SendPairingFailed(ecode); }
  uint8_t mtu() const { return PairingPhase::mtu(); }

  const ByteBuffer& last_rx_packet() { return last_rx_packet_; }

 private:
  fxl::WeakPtrFactory<ConcretePairingPhase> weak_ptr_factory_;
  DynamicByteBuffer last_rx_packet_;
};

class SMP_PairingPhaseTest : public l2cap::testing::FakeChannelTest {
 public:
  SMP_PairingPhaseTest() = default;
  ~SMP_PairingPhaseTest() override = default;

 protected:
  void SetUp() override { NewPairingPhase(); }

  void TearDown() override { pairing_phase_ = nullptr; }

  void NewPairingPhase(hci::Connection::Role role = hci::Connection::Role::kMaster,
                       hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    pairing_phase_ = std::make_unique<ConcretePairingPhase>(sm_chan_->GetWeakPtr(),
                                                            listener_->as_weak_ptr(), role);
  }

  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  FakeListener* listener() { return listener_.get(); }
  ConcretePairingPhase* pairing_phase() { return pairing_phase_.get(); }

 private:
  std::unique_ptr<FakeListener> listener_;
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<ConcretePairingPhase> pairing_phase_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_PairingPhaseTest);
};

TEST_F(SMP_PairingPhaseTest, LePairingPhaseExposesLeMtu) {
  NewPairingPhase(hci::Connection::Role::kMaster, hci::Connection::LinkType::kLE);
  ASSERT_EQ(kLEMTU, pairing_phase()->mtu());
}

TEST_F(SMP_PairingPhaseTest, BrEdrPairingPhaseExposesBrEdrMtu) {
  NewPairingPhase(hci::Connection::Role::kMaster, hci::Connection::LinkType::kACL);
  ASSERT_EQ(kBREDRMTU, pairing_phase()->mtu());
}

TEST_F(SMP_PairingPhaseTest, SendPairingFailed) {
  ErrorCode ecode = ErrorCode::kConfirmValueFailed;
  ByteBufferPtr tx_sdu = nullptr;
  int tx_count = 0;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        tx_sdu = std::move(sdu);
        tx_count++;
      },
      dispatcher());
  pairing_phase()->SendPairingFailed(ecode);
  RunLoopUntilIdle();
  ASSERT_TRUE(tx_sdu);
  ASSERT_EQ(tx_count, 1);
  auto reader = PacketReader(tx_sdu.get());
  ASSERT_EQ(reader.payload_size(), 1ul);
  ASSERT_EQ(reader.code(), kPairingFailed);
  ErrorCode tx_code = reader.payload<ErrorCode>();
  ASSERT_EQ(ecode, tx_code);
}

}  // namespace
}  // namespace sm
}  // namespace bt
