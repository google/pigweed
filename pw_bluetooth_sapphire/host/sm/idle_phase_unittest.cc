// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "idle_phase.h"

#include <memory>

#include <fbl/macros.h>
#include <gtest/gtest.h>

#include "lib/async/cpp/task.h"
#include "lib/fit/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {
namespace {

class SMP_IdlePhaseTest : public l2cap::testing::FakeChannelTest {
 public:
  SMP_IdlePhaseTest() = default;
  ~SMP_IdlePhaseTest() override = default;

 protected:
  void SetUp() override { NewIdlePhase(); }

  void TearDown() override { idle_phase_ = nullptr; }

  void NewIdlePhase(Role role = Role::kInitiator,
                    hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    fake_listener_ = std::make_unique<FakeListener>();
    idle_phase_ = std::make_unique<IdlePhase>(
        sm_chan_->GetWeakPtr(), fake_listener_->as_weak_ptr(), role,
        [this](PairingRequestParams preq) { last_pairing_req_ = preq; },
        [this](AuthReqField auth_req) { last_security_req_ = auth_req; });
  }

  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  IdlePhase* idle_phase() { return idle_phase_.get(); }

  std::optional<PairingRequestParams> last_pairing_req() { return last_pairing_req_; }
  std::optional<AuthReqField> last_security_req() { return last_security_req_; }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<FakeListener> fake_listener_;
  std::unique_ptr<IdlePhase> idle_phase_;

  std::optional<PairingRequestParams> last_pairing_req_;
  std::optional<AuthReqField> last_security_req_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_IdlePhaseTest);
};

using SMP_IdlePhaseDeathTest = SMP_IdlePhaseTest;

TEST_F(SMP_IdlePhaseDeathTest, MakeInvalidSecurityRequestsDies) {
  // Only the SMP responder may send the Security Request
  NewIdlePhase(Role::kInitiator);
  EXPECT_DEATH_IF_SUPPORTED(
      idle_phase()->MakeSecurityRequest(SecurityLevel::kEncrypted, BondableMode::Bondable),
      ".*Responder.*");
  // Security Requests must actually be requesting a level of security
  NewIdlePhase(Role::kResponder);
  EXPECT_DEATH_IF_SUPPORTED(
      idle_phase()->MakeSecurityRequest(SecurityLevel::kNoSecurity, BondableMode::Bondable),
      ".*Encrypted.*");
}

TEST_F(SMP_IdlePhaseTest, MakeEncryptedBondableSecurityRequest) {
  NewIdlePhase(Role::kResponder);
  StaticByteBuffer kExpectedReq = {kSecurityRequest, AuthReq::kBondingFlag};
  async::PostTask(dispatcher(), [this] {
    idle_phase()->MakeSecurityRequest(SecurityLevel::kEncrypted, BondableMode::Bondable);
  });
  ASSERT_TRUE(Expect(kExpectedReq));
  EXPECT_EQ(SecurityLevel::kEncrypted, *idle_phase()->pending_security_request());
}

TEST_F(SMP_IdlePhaseTest, MakeAuthenticatedNonBondableSecurityRequest) {
  NewIdlePhase(Role::kResponder);
  StaticByteBuffer kExpectedReq = {kSecurityRequest, AuthReq::kMITM};
  async::PostTask(dispatcher(), [this] {
    idle_phase()->MakeSecurityRequest(SecurityLevel::kAuthenticated, BondableMode::NonBondable);
  });
  ASSERT_TRUE(Expect(kExpectedReq));
  EXPECT_EQ(SecurityLevel::kAuthenticated, *idle_phase()->pending_security_request());
}

TEST_F(SMP_IdlePhaseTest, HandlesChannelClosedGracefully) {
  fake_chan()->Close();
  RunLoopUntilIdle();
}

TEST_F(SMP_IdlePhaseTest, PairingRequestAsResponderPassedThrough) {
  NewIdlePhase(Role::kResponder);
  StaticByteBuffer<util::PacketSize<PairingRequestParams>()> preq_packet;
  PacketWriter writer(kPairingRequest, &preq_packet);
  auto preq = PairingRequestParams{.auth_req = 0x01, .responder_key_dist_gen = 0x03};
  *writer.mutable_payload<PairingRequestParams>() = preq;
  ASSERT_FALSE(last_pairing_req().has_value());
  fake_chan()->Receive(preq_packet);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pairing_req().has_value());
  PairingRequestParams last_preq = last_pairing_req().value();
  ASSERT_EQ(0, memcmp(&last_preq, &preq, sizeof(PairingRequestParams)));
}

TEST_F(SMP_IdlePhaseTest, PairingRequestAsInitiatorSendsPairingFailed) {
  StaticByteBuffer<util::PacketSize<PairingRequestParams>()> preq_packet;
  PacketWriter writer(kPairingRequest, &preq_packet);
  auto preq = PairingRequestParams{.auth_req = 0x01, .responder_key_dist_gen = 0x03};
  *writer.mutable_payload<PairingRequestParams>() = preq;

  Code packet_header = kPairingConfirm;  // random, non-failure packet header
  ErrorCode ecode = ErrorCode::kNoError;
  fake_chan()->SetSendCallback(
      [&ecode, &packet_header](ByteBufferPtr sdu) {
        PacketReader reader(sdu.get());
        packet_header = reader.code();
        ecode = reader.payload<ErrorCode>();
      },
      dispatcher());

  ASSERT_FALSE(last_pairing_req().has_value());
  fake_chan()->Receive(preq_packet);
  RunLoopUntilIdle();
  ASSERT_FALSE(last_pairing_req().has_value());
  ASSERT_EQ(packet_header, kPairingFailed);
  ASSERT_EQ(ecode, ErrorCode::kCommandNotSupported);
}

TEST_F(SMP_IdlePhaseTest, SecurityRequestAsInitiatorPassedThrough) {
  AuthReqField generic_auth_req = 0x03;
  auto security_req = CreateStaticByteBuffer(kSecurityRequest, generic_auth_req);
  ASSERT_FALSE(last_security_req().has_value());
  fake_chan()->Receive(security_req);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_security_req().has_value());
  AuthReqField last_security_req_payload = last_security_req().value();
  ASSERT_EQ(generic_auth_req, last_security_req_payload);
}

TEST_F(SMP_IdlePhaseTest, SecurityRequestAsResponderSendsPairingFailed) {
  NewIdlePhase(Role::kResponder);
  AuthReqField generic_auth_req = 0x03;
  auto security_req = CreateStaticByteBuffer(kSecurityRequest, generic_auth_req);

  Code packet_header = kPairingConfirm;  // random, non-failure packet header
  ErrorCode ecode = ErrorCode::kNoError;
  fake_chan()->SetSendCallback(
      [&ecode, &packet_header](ByteBufferPtr sdu) {
        PacketReader reader(sdu.get());
        packet_header = reader.code();
        ecode = reader.payload<ErrorCode>();
      },
      dispatcher());

  ASSERT_FALSE(last_security_req().has_value());
  fake_chan()->Receive(security_req);
  RunLoopUntilIdle();
  ASSERT_FALSE(last_security_req().has_value());
  ASSERT_EQ(packet_header, kPairingFailed);
  ASSERT_EQ(ecode, ErrorCode::kCommandNotSupported);
}

TEST_F(SMP_IdlePhaseTest, NonSecurityPairingRequestMessageDropped) {
  StaticByteBuffer<util::PacketSize<PairingResponseParams>()> pres_packet;
  PacketWriter writer(kPairingResponse, &pres_packet);
  *writer.mutable_payload<PairingResponseParams>() = PairingResponseParams();

  bool message_sent = false;
  fake_chan()->SetSendCallback([&message_sent](ByteBufferPtr sdu) { message_sent = true; },
                               dispatcher());

  fake_chan()->Receive(pres_packet);
  RunLoopUntilIdle();
  ASSERT_FALSE(last_pairing_req().has_value());
  ASSERT_FALSE(last_security_req().has_value());
  ASSERT_FALSE(message_sent);
}

TEST_F(SMP_IdlePhaseTest, DropsInvalidPacket) {
  auto bad_packet = CreateStaticByteBuffer(0xFF);  // 0xFF is not a valid SMP header code

  bool message_sent = false;
  fake_chan()->SetSendCallback([&message_sent](ByteBufferPtr sdu) { message_sent = true; },
                               dispatcher());

  fake_chan()->Receive(bad_packet);
  RunLoopUntilIdle();
  ASSERT_FALSE(last_pairing_req().has_value());
  ASSERT_FALSE(last_security_req().has_value());
  ASSERT_FALSE(message_sent);
}

}  // namespace
}  // namespace sm
}  // namespace bt
