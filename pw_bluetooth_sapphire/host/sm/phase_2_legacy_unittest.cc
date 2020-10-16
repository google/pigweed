// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_2_legacy.h"

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
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

// clang-format off
const PairingFeatures kDefaultFeatures(
    true,                                     // initiator
    false,                                    // secure_connections
    true,                                     // will_bond
    std::optional<CrossTransportKeyAlgo>{std::nullopt},
    PairingMethod::kJustWorks,
    kMaxEncryptionKeySize,                    // encryption_key_size
    KeyDistGen::kIdKey,                       // local_key_distribution
    KeyDistGen::kIdKey | KeyDistGen::kEncKey  // remote_key_distribution
);

const PairingRequestParams kDefaultPreq{
    .io_capability = IOCapability::kNoInputNoOutput,
    .oob_data_flag = OOBDataFlag::kNotPresent,
    .auth_req = AuthReq::kBondingFlag,
    .max_encryption_key_size = kMaxEncryptionKeySize,
    .initiator_key_dist_gen = KeyDistGen::kIdKey,
    .responder_key_dist_gen = KeyDistGen::kIdKey | KeyDistGen::kEncKey
};

const PairingResponseParams kDefaultPres{
    .io_capability = IOCapability::kNoInputNoOutput,
    .oob_data_flag = OOBDataFlag::kNotPresent,
    .auth_req = AuthReq::kBondingFlag,
    .max_encryption_key_size = kMaxEncryptionKeySize,
    .initiator_key_dist_gen = KeyDistGen::kIdKey,
    .responder_key_dist_gen = KeyDistGen::kIdKey | KeyDistGen::kEncKey
};
// clang-format on
const DeviceAddress kAddr1(DeviceAddress::Type::kLEPublic, {0x00, 0x00, 0x00, 0x00, 0x00, 0x01});
const DeviceAddress kAddr2(DeviceAddress::Type::kLEPublic, {0x00, 0x00, 0x00, 0x00, 0x00, 0x02});
struct Phase2LegacyArgs {
  PairingFeatures features = kDefaultFeatures;
  PairingRequestParams preq = kDefaultPreq;
  PairingResponseParams pres = kDefaultPres;
  const DeviceAddress* initiator_addr = &kAddr1;
  const DeviceAddress* responder_addr = &kAddr2;
};

using util::PacketSize;

class SMP_Phase2LegacyTest : public l2cap::testing::FakeChannelTest {
 public:
  SMP_Phase2LegacyTest() = default;
  ~SMP_Phase2LegacyTest() override = default;

 protected:
  void SetUp() override { NewPhase2Legacy(); }

  void TearDown() override { phase_2_legacy_ = nullptr; }

  void NewPhase2Legacy(Phase2LegacyArgs phase_args = Phase2LegacyArgs(),
                       hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    phase_args_ = phase_args;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    auto role = phase_args.features.initiator ? Role::kInitiator : Role::kResponder;
    StaticByteBuffer<PacketSize<PairingRequestParams>()> preq, pres;
    preq.WriteObj(phase_args.preq);
    pres.WriteObj(phase_args.pres);
    phase_2_legacy_ = std::make_unique<Phase2Legacy>(
        sm_chan_->GetWeakPtr(), listener_->as_weak_ptr(), role, phase_args.features, preq, pres,
        *phase_args.initiator_addr, *phase_args.responder_addr, [this](const UInt128& stk) {
          phase_2_complete_count_++;
          stk_ = stk;
        });
  }

  void Receive128BitCmd(Code cmd_code, const UInt128& value) {
    fake_chan()->Receive(Make128BitCmd(cmd_code, value));
  }

  DynamicByteBuffer Make128BitCmd(Code cmd_code, const UInt128& value) {
    StaticByteBuffer<PacketSize<UInt128>()> buffer;
    PacketWriter writer(cmd_code, &buffer);
    *writer.mutable_payload<UInt128>() = value;
    return DynamicByteBuffer(buffer);
  }

  UInt128 GenerateConfirmValue(const UInt128& random, uint32_t tk = 0) const {
    tk = htole32(tk);
    UInt128 tk128;
    tk128.fill(0);
    std::memcpy(tk128.data(), &tk, sizeof(tk));
    StaticByteBuffer<PacketSize<PairingRequestParams>()> preq, pres;
    preq.WriteObj(phase_args_.preq);
    pres.WriteObj(phase_args_.pres);

    UInt128 out_value;
    util::C1(tk128, random, preq, pres, *phase_args_.initiator_addr, *phase_args_.responder_addr,
             &out_value);
    return out_value;
  }

  struct MatchingPair {
    UInt128 confirm;
    UInt128 random;
  };
  MatchingPair GenerateMatchingConfirmAndRandom(uint32_t tk) const {
    MatchingPair pair;
    zx_cprng_draw(pair.random.data(), pair.random.size());
    pair.confirm = GenerateConfirmValue(pair.random, tk);
    return pair;
  }

  static std::pair<Code, UInt128> ExtractCodeAnd128BitCmd(ByteBufferPtr sdu) {
    ZX_ASSERT_MSG(sdu, "Tried to ExtractCodeAnd128BitCmd from nullptr in test");
    auto maybe_reader = ValidPacketReader::ParseSdu(sdu);
    ZX_ASSERT_MSG(maybe_reader.is_ok(), "Tried to ExtractCodeAnd128BitCmd from invalid SMP packet");
    return {maybe_reader.value().code(), maybe_reader.value().payload<UInt128>()};
  }

  void DestroyPhase2() { phase_2_legacy_.reset(nullptr); }
  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  Phase2Legacy* phase_2_legacy() { return phase_2_legacy_.get(); }
  FakeListener* listener() { return listener_.get(); }

  int phase_2_complete_count() const { return phase_2_complete_count_; }
  UInt128 stk() const { return stk_; }

 private:
  std::unique_ptr<FakeListener> listener_;
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<Phase2Legacy> phase_2_legacy_;
  Phase2LegacyArgs phase_args_;
  int phase_2_complete_count_ = 0;
  UInt128 stk_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_Phase2LegacyTest);
};

using SMP_Phase2LegacyDeathTest = SMP_Phase2LegacyTest;

TEST_F(SMP_Phase2LegacyDeathTest, InvalidPairingMethodDies) {
  Phase2LegacyArgs args;
  // Legacy Pairing does not permit Numeric Comparison (V5.0, Vol. 3, Part H, Section 2.3.5.1)
  args.features.method = PairingMethod::kNumericComparison;
  ASSERT_DEATH_IF_SUPPORTED(NewPhase2Legacy(args), "method");
}

TEST_F(SMP_Phase2LegacyTest, InitiatorJustWorksStkSucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = true;
  args.features.method = PairingMethod::kJustWorks;
  NewPhase2Legacy(args);
  // Using Just Works, pairing should request user confirmation
  FakeListener::ConfirmCallback confirm_cb = nullptr;
  listener()->set_confirm_delegate(
      [&](FakeListener::ConfirmCallback cb) { confirm_cb = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();
  // We should request user confirmation, but not send a message until we receive it.
  ASSERT_EQ(kInvalidCode, sent_code);
  ASSERT_TRUE(confirm_cb);
  confirm_cb(true);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);

  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  ASSERT_TRUE(sent_payload.has_value());

  // Receive the peer pairing random & verify pairing completes successfully
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  ASSERT_EQ(1, phase_2_complete_count());
  UInt128 generated_stk;
  util::S1({0}, values.random, *sent_payload, &generated_stk);
  ASSERT_EQ(generated_stk, stk());
}

TEST_F(SMP_Phase2LegacyTest, InitiatorPasskeyInputStkSucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = true;
  args.features.method = PairingMethod::kPasskeyEntryInput;
  // preq & pres are set for consistency w/ args.features - not necessary for the test to pass.
  args.preq.io_capability = IOCapability::kKeyboardOnly;
  args.preq.auth_req |= AuthReq::kMITM;
  args.pres.io_capability = IOCapability::kDisplayOnly;
  NewPhase2Legacy(args);
  FakeListener::PasskeyResponseCallback passkey_responder = nullptr;
  listener()->set_request_passkey_delegate(
      [&](FakeListener::PasskeyResponseCallback cb) { passkey_responder = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();

  // We should request user confirmation, but not send a message until we receive it.
  ASSERT_EQ(kInvalidCode, sent_code);
  ASSERT_TRUE(passkey_responder);
  const int32_t kTk = 0x1234;
  const UInt128 kTk128 = {0x34, 0x12};
  passkey_responder(kTk);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);

  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  MatchingPair values = GenerateMatchingConfirmAndRandom(kTk);
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  ASSERT_TRUE(sent_payload.has_value());

  // Receive the peer pairing random & verify pairing completes successfully
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  ASSERT_EQ(1, phase_2_complete_count());
  UInt128 generated_stk;
  util::S1(kTk128, values.random, *sent_payload, &generated_stk);
  ASSERT_EQ(generated_stk, stk());
}

// This test is shorter than InitiatorPasskeyInputStkSucceeds because it only tests the code paths
// that differ for PasskeyDisplay, which all take place before sending the Confirm value.
TEST_F(SMP_Phase2LegacyTest, InitiatorPasskeyDisplaySucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = true;
  args.features.method = PairingMethod::kPasskeyEntryDisplay;
  // preq & pres are set for consistency w/ args.features - not necessary for the test to pass.
  args.preq.io_capability = IOCapability::kDisplayOnly;
  args.preq.auth_req |= AuthReq::kMITM;
  args.pres.io_capability = IOCapability::kKeyboardOnly;
  NewPhase2Legacy(args);

  FakeListener::ConfirmCallback display_confirmer = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t, bool, FakeListener::ConfirmCallback cb) { display_confirmer = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();
  // We should request user confirmation, but not send a message until we receive it.
  ASSERT_EQ(kInvalidCode, sent_code);
  ASSERT_TRUE(display_confirmer);
  display_confirmer(true);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);
  ASSERT_TRUE(sent_payload.has_value());
  // After sending Pairing Confirm, the behavior is the same as InitiatorPasskeyInputStkSucceeds
}

TEST_F(SMP_Phase2LegacyTest, InitiatorReceivesConfirmBeforeTkFails) {
  Phase2LegacyArgs args;
  args.features.initiator = true;
  NewPhase2Legacy(args);
  FakeListener::ConfirmCallback confirm_cb = nullptr;
  listener()->set_confirm_delegate(
      [&](FakeListener::ConfirmCallback cb) { confirm_cb = std::move(cb); });

  ByteBufferPtr sent_sdu = nullptr;
  fake_chan()->SetSendCallback([&](ByteBufferPtr sdu) { sent_sdu = std::move(sdu); }, dispatcher());
  phase_2_legacy()->Start();
  ASSERT_TRUE(confirm_cb);
  ASSERT_FALSE(sent_sdu);

  // Receive peer confirm (generated from arbitrary peer rand {0}) before |confirm_cb| is notified
  const auto kPairingConfirmCmd = Make128BitCmd(kPairingConfirm, GenerateConfirmValue({0}));
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kPairingConfirmCmd, kExpectedFailure));
}

TEST_F(SMP_Phase2LegacyTest, InvalidConfirmValueFails) {
  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);
  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  // Change the peer random so that the confirm value we sent does not match the random value.
  UInt128 mismatched_peer_rand = values.random;
  mismatched_peer_rand[0] += 1;
  const auto kPairingRandomCmd = Make128BitCmd(kPairingRandom, mismatched_peer_rand);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kConfirmValueFailed};
  ASSERT_TRUE(ReceiveAndExpect(kPairingRandomCmd, kExpectedFailure));
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, JustWorksUserConfirmationRejectedPairingFails) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kJustWorks;
  NewPhase2Legacy(args);
  // Reject the TK request
  bool confirmation_requested = false;
  listener()->set_confirm_delegate([&](FakeListener::ConfirmCallback cb) {
    confirmation_requested = true;
    cb(false);
  });
  async::PostTask(dispatcher(), [this] { phase_2_legacy()->Start(); });
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(Expect(kExpectedFailure));
  ASSERT_TRUE(confirmation_requested);
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, PasskeyInputRejectedPairingFails) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kPasskeyEntryInput;
  NewPhase2Legacy(args);
  // Reject the TK request
  bool confirmation_requested = false;
  listener()->set_request_passkey_delegate([&](FakeListener::PasskeyResponseCallback cb) {
    confirmation_requested = true;
    const int64_t kGenericNegativeInt = -12;
    cb(kGenericNegativeInt);
  });
  async::PostTask(dispatcher(), [this] { phase_2_legacy()->Start(); });
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kPasskeyEntryFailed};
  ASSERT_TRUE(Expect(kExpectedFailure));
  ASSERT_TRUE(confirmation_requested);
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, PasskeyDisplayRejectedPairingFails) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kPasskeyEntryDisplay;
  NewPhase2Legacy(args);
  // Reject the TK request
  bool confirmation_requested = false;
  listener()->set_display_delegate(
      [&](uint32_t /*ignore*/, bool, FakeListener::ConfirmCallback cb) {
        confirmation_requested = true;
        cb(false);
      });
  async::PostTask(dispatcher(), [this] { phase_2_legacy()->Start(); });
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(Expect(kExpectedFailure));
  ASSERT_TRUE(confirmation_requested);
  ASSERT_EQ(1, listener()->pairing_error_count());
}

// Each of the pairing methods has its own user input callback, and thus correct behavior under
// destruction of the Phase needs to be checked for each method.
TEST_F(SMP_Phase2LegacyTest, PhaseDestroyedWhileWaitingForJustWorksTk) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kJustWorks;
  NewPhase2Legacy(args);
  FakeListener::ConfirmCallback respond = nullptr;
  listener()->set_confirm_delegate([&](auto rsp) { respond = std::move(rsp); });
  phase_2_legacy()->Start();

  ASSERT_TRUE(respond);

  DestroyPhase2();
  respond(true);
  RunLoopUntilIdle();
  SUCCEED();
}

TEST_F(SMP_Phase2LegacyTest, PhaseDestroyedWhileWaitingForPasskeyInputTk) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kPasskeyEntryInput;
  NewPhase2Legacy(args);
  FakeListener::PasskeyResponseCallback respond = nullptr;
  listener()->set_request_passkey_delegate([&](auto rsp) { respond = std::move(rsp); });
  phase_2_legacy()->Start();

  ASSERT_TRUE(respond);

  DestroyPhase2();
  respond(1234);
  RunLoopUntilIdle();
  SUCCEED();
}

TEST_F(SMP_Phase2LegacyTest, PhaseDestroyedWaitingForPasskeyDisplayTk) {
  Phase2LegacyArgs args;
  args.features.method = PairingMethod::kPasskeyEntryDisplay;
  NewPhase2Legacy(args);
  FakeListener::ConfirmCallback respond = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t /*unused*/, bool, FakeListener::ConfirmCallback rsp) {
        respond = std::move(rsp);
      });
  phase_2_legacy()->Start();

  ASSERT_TRUE(respond);

  DestroyPhase2();
  respond(true);
  RunLoopUntilIdle();
  SUCCEED();
}

TEST_F(SMP_Phase2LegacyTest, ReceiveRandomBeforeTkFails) {
  // This test assumes initiator flow, but the behavior verified is the same for responder flow.
  FakeListener::ConfirmCallback confirm_cb = nullptr;
  listener()->set_confirm_delegate(
      [&](FakeListener::ConfirmCallback cb) { confirm_cb = std::move(cb); });

  phase_2_legacy()->Start();
  // We should have made the pairing delegate request, which will not be responded to.
  ASSERT_TRUE(confirm_cb);

  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  const auto kPairingRandomCmd = Make128BitCmd(kPairingRandom, values.random);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kPairingRandomCmd, kExpectedFailure));
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, ReceiveRandomBeforeConfirmFails) {
  // This test assumes initiator flow, but the behavior verified is the same for responder flow.
  bool requested_confirmation = false;
  // We automatically confirm the TK to check the case where we have a TK, but no peer confirm.
  listener()->set_confirm_delegate([&](FakeListener::ConfirmCallback cb) {
    requested_confirmation = true;
    cb(true);
  });
  phase_2_legacy()->Start();
  ASSERT_TRUE(requested_confirmation);
  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  const auto kPairingRandomCmd = Make128BitCmd(kPairingRandom, values.random);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kPairingRandomCmd, kExpectedFailure));
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, ReceivePairingFailed) {
  phase_2_legacy()->Start();
  fake_chan()->Receive(
      StaticByteBuffer<PacketSize<ErrorCode>()>{kPairingFailed, ErrorCode::kPairingNotSupported});
  RunLoopUntilIdle();

  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kPairingNotSupported, listener()->last_error().protocol_error());
  EXPECT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase2LegacyTest, UnsupportedCommandDuringPairing) {
  // Don't confirm the TK so that the confirm value is not sent;
  listener()->set_confirm_delegate([](auto) {});
  phase_2_legacy()->Start();

  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpected{kPairingFailed,
                                                            ErrorCode::kCommandNotSupported};
  ASSERT_TRUE(ReceiveAndExpect(StaticByteBuffer<1>(0xFF), kExpected));  // 0xFF is not an SMP code.
  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(ErrorCode::kCommandNotSupported, listener()->last_error().protocol_error());
}

TEST_F(SMP_Phase2LegacyTest, ReceiveMalformedPacket) {
  phase_2_legacy()->Start();
  // clang-format off
  const StaticByteBuffer<PacketSize<PairingRandomValue>() - 1> kMalformedPairingRandom {
    kPairingRandom,
    // Random value (1 octet too short)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  };
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure {
    kPairingFailed, ErrorCode::kInvalidParameters
  };
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kMalformedPairingRandom, kExpectedFailure));
}

TEST_F(SMP_Phase2LegacyTest, ResponderJustWorksStkSucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = false;
  args.features.method = PairingMethod::kJustWorks;
  NewPhase2Legacy(args);
  // Using Just Works, pairing should request user confirmation
  FakeListener::ConfirmCallback confirm_cb = nullptr;
  listener()->set_confirm_delegate(
      [&](FakeListener::ConfirmCallback cb) { confirm_cb = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();
  // We should not send a message until we receive the requested user input AND the peer confirm.
  ASSERT_TRUE(confirm_cb);
  ASSERT_EQ(kInvalidCode, sent_code);
  confirm_cb(true);
  RunLoopUntilIdle();
  ASSERT_EQ(kInvalidCode, sent_code);

  // Now we receive the peer confirm & should send ours.
  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);

  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  // Receive the peer pairing random & verify we send our random & pairing completes.
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  ASSERT_TRUE(sent_payload.has_value());
  ASSERT_EQ(1, phase_2_complete_count());
  UInt128 generated_stk;
  util::S1({0}, *sent_payload, values.random, &generated_stk);
  ASSERT_EQ(generated_stk, stk());
}

TEST_F(SMP_Phase2LegacyTest, ResponderPasskeyInputStkSucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = false;
  args.features.method = PairingMethod::kPasskeyEntryInput;
  // preq & pres are set for consistency w/ args.features - not necessary for the test to pass.
  args.preq.io_capability = IOCapability::kDisplayOnly;
  args.preq.auth_req |= AuthReq::kMITM;
  args.pres.io_capability = IOCapability::kKeyboardOnly;
  NewPhase2Legacy(args);
  FakeListener::PasskeyResponseCallback passkey_responder = nullptr;
  listener()->set_request_passkey_delegate(
      [&](FakeListener::PasskeyResponseCallback cb) { passkey_responder = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());

  phase_2_legacy()->Start();

  // We should not send a message until we receive the requested user input AND the peer confirm.
  ASSERT_TRUE(passkey_responder);
  ASSERT_EQ(kInvalidCode, sent_code);
  const int32_t kTk = 0x1234;
  const UInt128 kTk128 = {0x34, 0x12};
  passkey_responder(kTk);
  RunLoopUntilIdle();
  ASSERT_EQ(kInvalidCode, sent_code);
  // Now we receive the peer confirm & should send ours.
  MatchingPair values = GenerateMatchingConfirmAndRandom(kTk);
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);

  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  // Receive the peer pairing random & verify we send our random & pairing completes.
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  ASSERT_TRUE(sent_payload.has_value());
  ASSERT_EQ(1, phase_2_complete_count());
  UInt128 generated_stk;
  util::S1(kTk128, *sent_payload, values.random, &generated_stk);
  ASSERT_EQ(generated_stk, stk());
}

// This test is shorter than ResponderPasskeyInputStkSucceeds because it only tests the code paths
// that differ for PasskeyDisplay, which all take place before sending the Confirm value.
TEST_F(SMP_Phase2LegacyTest, ResponderPasskeyDisplaySucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = false;
  args.features.method = PairingMethod::kPasskeyEntryDisplay;
  // preq & pres are set for consistency w/ args.features - not necessary for the test to pass.
  args.preq.io_capability = IOCapability::kKeyboardOnly;
  args.preq.auth_req |= AuthReq::kMITM;
  args.pres.io_capability = IOCapability::kDisplayOnly;
  NewPhase2Legacy(args);

  FakeListener::ConfirmCallback display_confirmer = nullptr;
  uint32_t passkey = 0;
  listener()->set_display_delegate([&](uint32_t key, bool, FakeListener::ConfirmCallback cb) {
    passkey = key;
    display_confirmer = std::move(cb);
  });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());

  phase_2_legacy()->Start();
  // We should not send a message until we receive the requested user input AND the peer confirm.
  ASSERT_TRUE(display_confirmer);
  ASSERT_EQ(kInvalidCode, sent_code);
  display_confirmer(true);
  RunLoopUntilIdle();
  ASSERT_EQ(kInvalidCode, sent_code);

  // Now we receive the peer confirm & should send ours.
  MatchingPair values = GenerateMatchingConfirmAndRandom(passkey);
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);
  ASSERT_TRUE(sent_payload.has_value());
}

TEST_F(SMP_Phase2LegacyTest, ResponderReceivesConfirmBeforeTkSucceeds) {
  Phase2LegacyArgs args;
  args.features.initiator = false;
  NewPhase2Legacy(args);
  // Using Just Works, pairing should request user confirmation
  FakeListener::ConfirmCallback confirm_cb = nullptr;
  listener()->set_confirm_delegate(
      [&](FakeListener::ConfirmCallback cb) { confirm_cb = std::move(cb); });

  Code sent_code = kInvalidCode;
  std::optional<UInt128> sent_payload = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(sent_code, sent_payload) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();

  // We should not send a message until we receive the requested user input AND the peer confirm.
  ASSERT_TRUE(confirm_cb);
  ASSERT_EQ(kInvalidCode, sent_code);
  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kInvalidCode, sent_code);

  // Now we received the user input & should send the peer our confirmation.
  confirm_cb(true);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, sent_code);
  // Reset |sent_payload| to be able to detect that the FakeChannel's |send_callback| is notified.
  sent_payload = std::nullopt;
  // Receive the peer pairing random & verify we send our random & pairing completes.
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingRandom, sent_code);
  ASSERT_TRUE(sent_payload.has_value());
  ASSERT_EQ(1, phase_2_complete_count());
  UInt128 generated_stk;
  util::S1({0}, *sent_payload, values.random, &generated_stk);
  ASSERT_EQ(generated_stk, stk());
}

TEST_F(SMP_Phase2LegacyTest, ReceiveConfirmValueTwiceFails) {
  // This test uses the responder flow, but the behavior verified is the same for initiator flow.
  Phase2LegacyArgs args;
  args.features.initiator = false;
  NewPhase2Legacy(args);

  Code code = kInvalidCode;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) {
        std::tie(code, std::ignore) = ExtractCodeAnd128BitCmd(std::move(sdu));
      },
      dispatcher());
  phase_2_legacy()->Start();

  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  ASSERT_EQ(kPairingConfirm, code);
  const auto kPairingConfirmCmd = Make128BitCmd(kPairingConfirm, values.confirm);
  // Pairing should fail after receiving 2 confirm values with kUnspecifiedReason
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kPairingConfirmCmd, kExpectedFailure));
  ASSERT_EQ(1, listener()->pairing_error_count());
}

// Phase 2 ends after receiving the second random value & subsequently sending its own, but if the
// Phase 2 object is kept around and receives a second random value, pairing will fail.
TEST_F(SMP_Phase2LegacyTest, ReceiveRandomValueTwiceFails) {
  // This test uses the responder flow, but the behavior verified is the same for initiator flow.
  Phase2LegacyArgs args;
  args.features.initiator = false;
  NewPhase2Legacy(args);

  phase_2_legacy()->Start();

  MatchingPair values = GenerateMatchingConfirmAndRandom(0);  // Just Works TK is 0
  Receive128BitCmd(kPairingConfirm, values.confirm);
  RunLoopUntilIdle();
  Receive128BitCmd(kPairingRandom, values.random);
  RunLoopUntilIdle();
  // We've completed Phase 2, and should've notified the callback
  ASSERT_EQ(1, phase_2_complete_count());
  const auto kPairingRandomCmd = Make128BitCmd(kPairingRandom, values.random);
  // Pairing should fail after receiving a second random value with kUnspecifiedReason
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kPairingRandomCmd, kExpectedFailure));
  ASSERT_EQ(1, listener()->pairing_error_count());
}
}  // namespace
}  // namespace sm
}  // namespace bt
