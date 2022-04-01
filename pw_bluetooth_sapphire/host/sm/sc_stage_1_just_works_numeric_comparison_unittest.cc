// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sc_stage_1_just_works_numeric_comparison.h"

#include <memory>

#include <gtest/gtest.h>

#include "lib/async/default.h"
#include "lib/fpromise/result.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::sm {
namespace {

struct ScStage1Args {
  Role role = Role::kInitiator;
  UInt256 local_pub_key_x = {1};
  UInt256 peer_pub_key_x = {2};
  PairingMethod method = PairingMethod::kJustWorks;
};

class ScStage1JustWorksNumericComparisonTest : public l2cap::testing::FakeChannelTest {
 public:
  ScStage1JustWorksNumericComparisonTest() = default;
  ~ScStage1JustWorksNumericComparisonTest() = default;

 protected:
  using ConfirmCallback = FakeListener::ConfirmCallback;
  void SetUp() override { NewScStage1JustWorksNumericComparison(); }
  void TearDown() override { stage_1_ = nullptr; }
  void NewScStage1JustWorksNumericComparison(ScStage1Args args = ScStage1Args()) {
    args_ = args;
    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(ChannelOptions(l2cap::kLESMPChannelId));
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    fake_chan_->SetSendCallback(
        [this](ByteBufferPtr sent_packet) {
          auto maybe_reader = ValidPacketReader::ParseSdu(sent_packet);
          ASSERT_TRUE(maybe_reader.is_ok())
              << "Sent invalid packet: "
              << ProtocolErrorTraits<sm::ErrorCode>::ToString(maybe_reader.error());
          last_packet_ = maybe_reader.value();
          last_packet_internal_ = std::move(sent_packet);
        },
        async_get_default_dispatcher());
    stage_1_ = std::make_unique<ScStage1JustWorksNumericComparison>(
        listener_->as_weak_ptr(), args.role, args.local_pub_key_x, args.peer_pub_key_x, args.method,
        sm_chan_->GetWeakPtr(),
        [this](fpromise::result<ScStage1::Output, ErrorCode> out) { last_results_ = out; });
  }

  UInt128 GenerateConfirmValue(const UInt128& random) const {
    UInt256 responder_key = args_.local_pub_key_x, initiator_key = args_.peer_pub_key_x;
    if (args_.role == Role::kInitiator) {
      std::swap(responder_key, initiator_key);
    }
    return util::F4(responder_key, initiator_key, random, 0).value();
  }

  struct MatchingPair {
    UInt128 confirm;
    UInt128 random;
  };
  MatchingPair GenerateMatchingConfirmAndRandom() const {
    MatchingPair pair;
    zx_cprng_draw(pair.random.data(), pair.random.size());
    pair.confirm = GenerateConfirmValue(pair.random);
    return pair;
  }

  void DestroyStage1() { stage_1_ = nullptr; }
  ScStage1JustWorksNumericComparison* stage_1() { return stage_1_.get(); }
  FakeListener* listener() { return listener_.get(); }
  std::optional<ValidPacketReader> last_packet() const { return last_packet_; }
  std::optional<fpromise::result<ScStage1::Output, ErrorCode>> last_results() const {
    return last_results_;
  }

 private:
  ScStage1Args args_;
  std::unique_ptr<FakeListener> listener_;
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<ScStage1JustWorksNumericComparison> stage_1_;
  std::optional<ValidPacketReader> last_packet_ = std::nullopt;
  // To store the last sent SDU so that the last_packet_ PacketReader points at valid data.
  ByteBufferPtr last_packet_internal_;
  std::optional<fpromise::result<ScStage1::Output, ErrorCode>> last_results_ = std::nullopt;
};

using ScStage1JustWorksNumericComparisonDeathTest = ScStage1JustWorksNumericComparisonTest;
TEST_F(ScStage1JustWorksNumericComparisonDeathTest, InvalidMethodsDie) {
  ScStage1Args args;
  args.method = PairingMethod::kOutOfBand;
  ASSERT_DEATH_IF_SUPPORTED(NewScStage1JustWorksNumericComparison(args), ".*method.*");
  args.method = PairingMethod::kPasskeyEntryDisplay;
  ASSERT_DEATH_IF_SUPPORTED(NewScStage1JustWorksNumericComparison(args), ".*method.*");
  args.method = PairingMethod::kPasskeyEntryInput;
  ASSERT_DEATH_IF_SUPPORTED(NewScStage1JustWorksNumericComparison(args), ".*method.*");
}

TEST_F(ScStage1JustWorksNumericComparisonTest, InitiatorJustWorks) {
  ScStage1Args args;
  args.role = Role::kInitiator;
  args.method = PairingMethod::kJustWorks;
  NewScStage1JustWorksNumericComparison(args);
  MatchingPair vals = GenerateMatchingConfirmAndRandom();
  ScStage1::Output expected_results{
      .initiator_r = {0}, .responder_r = {0}, .responder_rand = vals.random};

  stage_1()->Run();
  stage_1()->OnPairingConfirm(vals.confirm);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingRandom, last_packet()->code());
  expected_results.initiator_rand = last_packet()->payload<PairingRandomValue>();

  stage_1()->OnPairingRandom(vals.random);
  ASSERT_TRUE(last_results()->is_ok());
  EXPECT_EQ(expected_results, last_results()->value());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, InitiatorNumericComparison) {
  ScStage1Args args;
  args.role = Role::kInitiator;
  args.method = PairingMethod::kNumericComparison;
  NewScStage1JustWorksNumericComparison(args);
  std::optional<uint32_t> compare = std::nullopt;
  ConfirmCallback user_confirm = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t cmp, Delegate::DisplayMethod method, ConfirmCallback cb) {
        ASSERT_EQ(Delegate::DisplayMethod::kComparison, method);
        compare = cmp;
        user_confirm = std::move(cb);
      });
  MatchingPair vals = GenerateMatchingConfirmAndRandom();
  ScStage1::Output expected_results{
      .initiator_r = {0}, .responder_r = {0}, .responder_rand = vals.random};

  stage_1()->Run();
  stage_1()->OnPairingConfirm(vals.confirm);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingRandom, last_packet()->code());
  expected_results.initiator_rand = last_packet()->payload<PairingRandomValue>();

  stage_1()->OnPairingRandom(vals.random);
  ASSERT_TRUE(user_confirm);
  // Results should not be ready until user input is received through user_confirm
  ASSERT_FALSE(last_results().has_value());
  uint32_t kExpectedCompare =
      *util::G2(args.local_pub_key_x, args.peer_pub_key_x, expected_results.initiator_rand,
                expected_results.responder_rand);
  kExpectedCompare %= 1000000;
  EXPECT_EQ(kExpectedCompare, compare);

  user_confirm(true);
  ASSERT_TRUE(last_results()->is_ok());
  EXPECT_EQ(expected_results, last_results()->value());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, InitiatorReceivesConfirmTwiceFails) {
  ScStage1Args args;
  args.role = Role::kInitiator;
  NewScStage1JustWorksNumericComparison(args);
  stage_1()->Run();
  ASSERT_FALSE(last_results().has_value());
  stage_1()->OnPairingConfirm(Random<PairingConfirmValue>());
  stage_1()->OnPairingConfirm(Random<PairingConfirmValue>());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, InitiatorReceiveRandomOutOfOrder) {
  stage_1()->Run();
  ASSERT_FALSE(last_results().has_value());
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

// Test to demonstrate receiving random twice for responder causes pairing to fail.
TEST_F(ScStage1JustWorksNumericComparisonTest, ResponderReceiveRandomTwiceFails) {
  ScStage1Args args;
  args.role = Role::kResponder;
  NewScStage1JustWorksNumericComparison(args);

  stage_1()->Run();
  ASSERT_FALSE(last_results().has_value());
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, InitiatorMismatchedConfirmAndRand) {
  ScStage1Args args;
  args.role = Role::kInitiator;
  NewScStage1JustWorksNumericComparison(args);
  MatchingPair vals = GenerateMatchingConfirmAndRandom();

  stage_1()->Run();
  stage_1()->OnPairingConfirm(vals.confirm);
  RunLoopUntilIdle();
  vals.random[0] -= 1;
  ASSERT_FALSE(last_results().has_value());
  stage_1()->OnPairingRandom(vals.random);
  EXPECT_EQ(ErrorCode::kConfirmValueFailed, last_results()->error());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, ResponderJustWorks) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kJustWorks;
  NewScStage1JustWorksNumericComparison(args);
  UInt128 kPeerRand = Random<PairingRandomValue>();
  ScStage1::Output expected_results{
      .initiator_r = {0}, .responder_r = {0}, .initiator_rand = kPeerRand};

  stage_1()->Run();
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingConfirm, last_packet()->code());
  UInt128 sent_confirm = last_packet()->payload<PairingConfirmValue>();

  stage_1()->OnPairingRandom(kPeerRand);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingRandom, last_packet()->code());
  expected_results.responder_rand = last_packet()->payload<PairingRandomValue>();
  EXPECT_EQ(GenerateConfirmValue(expected_results.responder_rand), sent_confirm);
  ASSERT_TRUE(last_results()->is_ok());
  EXPECT_EQ(expected_results, last_results()->value());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, ResponderNumericComparison) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kNumericComparison;
  NewScStage1JustWorksNumericComparison(args);
  std::optional<uint32_t> compare = std::nullopt;
  ConfirmCallback user_confirm = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t cmp, Delegate::DisplayMethod method, ConfirmCallback cb) {
        ASSERT_EQ(Delegate::DisplayMethod::kComparison, method);
        compare = cmp;
        user_confirm = std::move(cb);
      });
  UInt128 kPeerRand = Random<PairingRandomValue>();
  ScStage1::Output expected_results{
      .initiator_r = {0}, .responder_r = {0}, .initiator_rand = kPeerRand};

  stage_1()->Run();
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingConfirm, last_packet()->code());
  UInt128 sent_confirm = last_packet()->payload<PairingConfirmValue>();

  stage_1()->OnPairingRandom(kPeerRand);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_packet().has_value());
  EXPECT_EQ(kPairingRandom, last_packet()->code());
  expected_results.responder_rand = last_packet()->payload<PairingRandomValue>();
  EXPECT_EQ(GenerateConfirmValue(expected_results.responder_rand), sent_confirm);

  ASSERT_TRUE(user_confirm);
  // Results should not be ready until user input is received through user_confirm
  ASSERT_FALSE(last_results().has_value());
  uint32_t kExpectedCompare =
      *util::G2(args.peer_pub_key_x, args.local_pub_key_x, expected_results.initiator_rand,
                expected_results.responder_rand);
  kExpectedCompare %= 1000000;
  EXPECT_EQ(kExpectedCompare, compare);

  user_confirm(true);
  RunLoopUntilIdle();
  ASSERT_TRUE(last_results()->is_ok());
  EXPECT_EQ(expected_results, last_results()->value());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, ResponderReceivesConfirmFails) {
  ScStage1Args args;
  args.role = Role::kResponder;
  NewScStage1JustWorksNumericComparison(args);
  stage_1()->Run();
  ASSERT_FALSE(last_results().has_value());
  stage_1()->OnPairingConfirm(Random<PairingConfirmValue>());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, ResponderReceiveRandomOutOfOrder) {
  NewScStage1JustWorksNumericComparison(ScStage1Args{.role = Role::kResponder});
  // `stage_1_` was not `Run`, so the Pairing Confirm hasn't been sent and the peer should not have
  // sent the Pairing Random.
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

// This test uses responder flow, but the behavior under test is the same for initiator.
TEST_F(ScStage1JustWorksNumericComparisonTest, ListenerRejectsJustWorks) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kJustWorks;
  NewScStage1JustWorksNumericComparison(args);
  ConfirmCallback user_confirm = nullptr;
  listener()->set_confirm_delegate([&](ConfirmCallback cb) { user_confirm = std::move(cb); });

  stage_1()->Run();
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  ASSERT_TRUE(user_confirm);
  // No results should be reported until the confirmation is rejected.
  ASSERT_FALSE(last_results().has_value());
  user_confirm(false);
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, last_results()->error());
}

// This test uses responder flow, but the behavior under test is the same for initiator.
TEST_F(ScStage1JustWorksNumericComparisonTest, ListenerRejectsNumericComparison) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kNumericComparison;
  NewScStage1JustWorksNumericComparison(args);
  ConfirmCallback user_confirm = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t, Delegate::DisplayMethod method, ConfirmCallback cb) {
        ASSERT_EQ(Delegate::DisplayMethod::kComparison, method);
        user_confirm = std::move(cb);
      });

  stage_1()->Run();
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  ASSERT_TRUE(user_confirm);
  // No results should be reported until the numeric comparison is rejected.
  ASSERT_FALSE(last_results().has_value());
  user_confirm(false);
  EXPECT_EQ(ErrorCode::kNumericComparisonFailed, last_results()->error());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, StageDestroyedWhileWaitingForJustWorksConfirm) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kJustWorks;
  NewScStage1JustWorksNumericComparison(args);
  ConfirmCallback user_confirm = nullptr;
  listener()->set_confirm_delegate([&](ConfirmCallback cb) { user_confirm = std::move(cb); });

  stage_1()->Run();
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  ASSERT_TRUE(user_confirm);

  DestroyStage1();
  // No results should be reported after Stage 1 is destroyed.
  user_confirm(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(last_results().has_value());
}

TEST_F(ScStage1JustWorksNumericComparisonTest, StageDestroyedWhileWaitingForNumericComparison) {
  ScStage1Args args;
  args.role = Role::kResponder;
  args.method = PairingMethod::kNumericComparison;
  NewScStage1JustWorksNumericComparison(args);
  ConfirmCallback user_confirm = nullptr;
  listener()->set_display_delegate(
      [&](uint32_t, Delegate::DisplayMethod method, ConfirmCallback cb) {
        ASSERT_EQ(Delegate::DisplayMethod::kComparison, method);
        user_confirm = std::move(cb);
      });

  stage_1()->Run();
  stage_1()->OnPairingRandom(Random<PairingRandomValue>());
  ASSERT_TRUE(user_confirm);

  DestroyStage1();
  // No results should be reported after Stage 1 is destroyed.
  user_confirm(true);
  RunLoopUntilIdle();
  EXPECT_FALSE(last_results().has_value());
}

}  // namespace
}  // namespace bt::sm
