// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_1.h"

#include <memory>

#include <fbl/macros.h>
#include <gtest/gtest.h>

#include "lib/fit/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {
namespace {

struct Phase1Args {
  PairingRequestParams preq = PairingRequestParams();
  IOCapability io_capability = IOCapability::kNoInputNoOutput;
  BondableMode bondable_mode = BondableMode::Bondable;
  bool mitm_required = false;
  bool sc_supported = false;
};

class SMP_Phase1Test : public l2cap::testing::FakeChannelTest {
 public:
  SMP_Phase1Test() = default;
  ~SMP_Phase1Test() override = default;

 protected:
  void SetUp() override { NewPhase1(); }

  void TearDown() override { phase_1_ = nullptr; }

  void NewPhase1(Role role = Role::kInitiator, Phase1Args phase_args = Phase1Args(),
                 hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == hci::Connection::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    uint16_t mtu = phase_args.sc_supported ? l2cap::kMaxMTU : kNoSecureConnectionsMtu;
    ChannelOptions options(cid, mtu);
    options.link_type = ll_type;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_);
    if (role == Role::kInitiator) {
      phase_1_ = Phase1::CreatePhase1Initiator(
          sm_chan_->GetWeakPtr(), listener_->as_weak_ptr(), phase_args.io_capability,
          phase_args.bondable_mode, phase_args.mitm_required,
          [this](const PairingFeatures& features, const ByteBuffer& preq, const ByteBuffer& pres) {
            feature_exchange_count_++;
            features_ = features;
            last_pairing_req_ = std::make_unique<DynamicByteBuffer>(preq);
            last_pairing_res_ = std::make_unique<DynamicByteBuffer>(pres);
          });
    } else {
      phase_1_ = Phase1::CreatePhase1Responder(
          sm_chan_->GetWeakPtr(), listener_->as_weak_ptr(), phase_args.preq,
          phase_args.io_capability, phase_args.bondable_mode, phase_args.mitm_required,
          [this](const PairingFeatures& features, const ByteBuffer& preq, const ByteBuffer& pres) {
            feature_exchange_count_++;
            features_ = features;
            last_pairing_req_ = std::make_unique<DynamicByteBuffer>(preq);
            last_pairing_res_ = std::make_unique<DynamicByteBuffer>(pres);
          });
    }
  }

  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  Phase1* phase_1() { return phase_1_.get(); }
  FakeListener* listener() { return listener_.get(); }

  int feature_exchange_count() { return feature_exchange_count_; }
  PairingFeatures features() { return features_; }
  ByteBuffer* last_preq() { return last_pairing_req_.get(); }
  ByteBuffer* last_pres() { return last_pairing_res_.get(); }

 private:
  std::unique_ptr<FakeListener> listener_;
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<Phase1> phase_1_;

  int feature_exchange_count_ = 0;
  PairingFeatures features_;
  ByteBufferPtr last_pairing_req_;
  ByteBufferPtr last_pairing_res_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_Phase1Test);
};

TEST_F(SMP_Phase1Test, FeatureExchangeStartDefaultParams) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: "Pairing Request"
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));
}

TEST_F(SMP_Phase1Test, FeatureExchangeStartCustomParams) {
  auto phase_args = Phase1Args{.io_capability = IOCapability::kDisplayYesNo,
                               .bondable_mode = BondableMode::NonBondable,
                               .mitm_required = true,
                               .sc_supported = true};
  NewPhase1(Role::kInitiator, phase_args);

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,        // code: "Pairing Request"
      0x01,        // IO cap.: DisplayYesNo
      0x00,        // OOB: not present
      0b00001100,  // AuthReq: no bonding, SC, MITM
      0x10,        // encr. key size: 16 (default max)
      0x00,        // initiator keys: none - non-bondable mode
      0x00         // responder keys: none - non-bondable mode
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));
}

TEST_F(SMP_Phase1Test, FeatureExchangeInitiatorWithIdentityInfo) {
  listener()->set_identity_info(IdentityInfo());

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,        // code: "Pairing Request"
      0x03,        // IO cap.: NoInputNoOutput
      0x00,        // OOB: not present
      0x01,        // AuthReq: Bonding, no MITM
      0x10,        // encr. key size: 16 (default max)
      0x02,        // initiator keys: identity info
      0x03         // responder keys: enc key and identity info
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  EXPECT_EQ(1, listener()->identity_info_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangePairingFailed) {
  fake_chan()->Receive(CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                                              0x05   // reason: Pairing Not Supported
                                              ));
  RunLoopUntilIdle();

  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  ASSERT_EQ(ErrorCode::kPairingNotSupported, listener()->last_error().protocol_error());
  ASSERT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangeLocalRejectsUnsupportedInitiatorKeys) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x00,  // AuthReq: no MITM
      0x07,  // encr. key size: 7 (default min)
      0x01,  // initiator keys: enc key (not listed in kRequest)
      0x03   // responder keys: enc key and identity info
  );
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, listener()->last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangeLocalRejectsUnsupportedResponderKeys) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x00,  // AuthReq: no MITM
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x04   // responder keys: sign key (not in kRequest)
  );
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, listener()->last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

// Pairing should fail if MITM is required but the I/O capabilities cannot provide it
TEST_F(SMP_Phase1Test, FeatureExchangeFailureAuthenticationRequirements) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x04,  // AuthReq: MITM required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only (it's OK to agree to fewer keys)
  );
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x03   // reason: Authentication requirements
  );
  // clang-format on
  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kAuthenticationRequirements, listener()->last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangeFailureMalformedRequest) {
  // clang-format off
  const auto kMalformedResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: no auth. request by default
      0x10,  // encr. key size: 16 (default max)
      0x01   // initiator key dist.: encr. key only
             // Missing last byte
  );
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kMalformedResponse, kFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, listener()->last_error().protocol_error());
}

TEST_F(SMP_Phase1Test, FeatureExchangeBothSupportSCFeaturesHaveSC) {
  Phase1Args args;
  args.sc_supported = true;
  NewPhase1(Role::kInitiator, args);
  // clang-format off
  const auto kRequest = StaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      AuthReq::kSC | AuthReq::kBondingFlag,
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = StaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      AuthReq::kSC | AuthReq::kBondingFlag,
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_TRUE(features().secure_connections);
  ASSERT_TRUE(last_preq());
  ASSERT_TRUE(last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));
}

TEST_F(SMP_Phase1Test, FeatureExchangeLocalSCRemoteNoSCFeaturesNoSc) {
  Phase1Args args;
  args.sc_supported = true;
  NewPhase1(Role::kInitiator, args);
  // clang-format off
  const auto kRequest = StaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      AuthReq::kSC | AuthReq::kBondingFlag,
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = StaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      AuthReq::kBondingFlag,
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  ASSERT_TRUE(last_preq());
  ASSERT_TRUE(last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));
}

TEST_F(SMP_Phase1Test, FeatureExchangePairingResponseLegacyJustWorks) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_EQ(PairingMethod::kJustWorks, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_TRUE(KeyDistGen::kEncKey & features().remote_key_distribution);
  ASSERT_TRUE(last_preq());
  ASSERT_TRUE(last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));
}

TEST_F(SMP_Phase1Test, FeatureExchangePairingResponseLegacyMITM) {
  auto phase_args = Phase1Args{.io_capability = IOCapability::kDisplayYesNo};
  NewPhase1(Role::kInitiator, phase_args);

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x01,  // IO cap.: DisplayYesNo
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x02,  // IO cap.: KeyboardOnly
      0x00,  // OOB: not present
      0x05,  // AuthReq: MITM required, bondable mode
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_TRUE(KeyDistGen::kEncKey & features().remote_key_distribution);
  ASSERT_TRUE(last_preq() && last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));
}

TEST_F(SMP_Phase1Test, FeatureExchangeEncryptionKeySize) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x04,  // AuthReq: MITM required
      0x02,  // encr. key size: 2 (too small)
      0x01,  // initiator keys: enc key only
      0x01   // responder keys: enc key only
  );
  const auto kFailure =  CreateStaticByteBuffer(
    0x05,  // code: Pairing Failed
    0x06   // reason: Encryption Key Size
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_EQ(0, feature_exchange_count());
  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_EQ(ErrorCode::kEncryptionKeySize, listener()->last_error().protocol_error());
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderErrorMaster) {
  const auto kRequest = CreateStaticByteBuffer(0x01,  // code: Pairing Request
                                               0x03,  // IO cap.: NoInputNoOutput
                                               0x00,  // OOB: not present
                                               0x00,  // AuthReq: no auth. request by default
                                               0x10,  // encr. key size: 16 (default max)
                                               0x01,  // initiator key dist.: encr. key only
                                               0x01   // responder key dist.: encr. key only
  );
  const auto kFailure = CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                                               ErrorCode::kUnspecifiedReason);

  NewPhase1(Role::kInitiator);
  EXPECT_TRUE(ReceiveAndExpect(kRequest, kFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

// Verify that Pairing Requests can only be received in IdlePhase, not in Phase1.
TEST_F(SMP_Phase1Test, Phase1ResponderRejectsPairingRequest) {
  const auto kRequest = CreateStaticByteBuffer(0x01,  // code: Pairing Request
                                               0x03,  // IO cap.: NoInputNoOutput
                                               0x00,  // OOB: not present
                                               0x00,  // AuthReq: no auth. request by default
                                               0x10,  // encr. key size: 16 (default max)
                                               0x01,  // initiator key dist.: encr. key only
                                               0x01   // responder key dist.: encr. key only
  );
  const auto kFailure = CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                                               ErrorCode::kUnspecifiedReason);

  NewPhase1(Role::kResponder);
  EXPECT_TRUE(ReceiveAndExpect(kRequest, kFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderBothSupportSCFeaturesHaveSC) {
  // clang-format off
  const auto kResponse = StaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      AuthReq::kSC | AuthReq::kBondingFlag,
      0x10,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on
  Phase1Args args{.preq =
                      PairingRequestParams{
                          .io_capability = IOCapability::kNoInputNoOutput,
                          .auth_req = AuthReq::kSC | AuthReq::kBondingFlag,
                          .max_encryption_key_size = 0x10,  // 16, default max
                          .initiator_key_dist_gen = 0x00,
                          .responder_key_dist_gen = 0x01  // enc key
                      },
                  .sc_supported = true};
  NewPhase1(Role::kResponder, args);
  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_FALSE(features().initiator);
  EXPECT_TRUE(features().secure_connections);
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderLocalSCRemoteNoSCFeaturesNoSC) {
  // clang-format off
  const auto kResponse = StaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      AuthReq::kSC | AuthReq::kBondingFlag,
      0x10,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on
  Phase1Args args{.preq =
                      PairingRequestParams{
                          .io_capability = IOCapability::kNoInputNoOutput,
                          .auth_req = AuthReq::kBondingFlag,
                          .max_encryption_key_size = 0x10,  // 16, default max
                          .initiator_key_dist_gen = 0x00,
                          .responder_key_dist_gen = 0x01  // enc key
                      },
                  .sc_supported = true};
  NewPhase1(Role::kResponder, args);
  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  RunLoopUntilIdle();

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
}

// Tests that the local responder does not request keys that the initiator cannot distribute.
TEST_F(SMP_Phase1Test, FeatureExchangeLocalResponderRespectsInitiator) {
  auto phase_args = Phase1Args{.preq = PairingRequestParams{
                                   .io_capability = IOCapability::kNoInputNoOutput,
                                   .auth_req = AuthReq::kBondingFlag,
                                   .max_encryption_key_size = 16,
                                   .initiator_key_dist_gen = 0x01,  // encryption key only
                                   .responder_key_dist_gen = 0x00   // none
                               }};
  // clang-format off
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none  // We shouldn't request the IdKey
      0x00   // responder keys: none
  );
  // clang-format on

  NewPhase1(Role::kResponder, phase_args);
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_FALSE(features().remote_key_distribution);
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));
}

// Tests that we (as the responder) request to distribute identity information if available.
TEST_F(SMP_Phase1Test, FeatureExchangeResponderDistributesIdKey) {
  auto phase_args =
      Phase1Args{.preq = PairingRequestParams{.io_capability = IOCapability::kNoInputNoOutput,
                                              .auth_req = 0x01,                 // bondable mode
                                              .max_encryption_key_size = 0x10,  // 16, default max
                                              .responder_key_dist_gen = KeyDistGen::kIdKey}};
  // clang-format off
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x02   // responder keys: identity info
  );
  // clang-format on

  NewPhase1(Role::kResponder, phase_args);
  listener()->set_identity_info(IdentityInfo());
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_TRUE(features().local_key_distribution & KeyDistGen::kIdKey);
  EXPECT_FALSE(features().remote_key_distribution);
}

// Tests that local responder doesn't respond with distribute ID info if available but not requested
// by the initiator.
TEST_F(SMP_Phase1Test, FeatureExchangeResponderRespectsInitiatorForIdKey) {
  auto phase_args =
      Phase1Args{.preq = PairingRequestParams{
                     .io_capability = IOCapability::kNoInputNoOutput,
                     .auth_req = 0x01,                 // bondable mode
                     .max_encryption_key_size = 0x10,  // 16, default max
                     .initiator_key_dist_gen = 0x00,
                     .responder_key_dist_gen = 0x00  // Initiator explicitly does not request ID key
                 }};
  // clang-format off
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x00   // responder keys: none - we shouldn't distribute IdKey even though we have it
  );
  // clang-format on

  NewPhase1(Role::kResponder, phase_args);
  listener()->set_identity_info(IdentityInfo());
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_FALSE(features().remote_key_distribution);
}

// Pairing should fail if MITM is required but the pairing method cannot provide
// it (due to I/O capabilities).
TEST_F(SMP_Phase1Test, FeatureExchangeResponderFailedAuthenticationRequirements) {
  const auto kRequest = CreateStaticByteBuffer(0x01,  // code: Pairing Response
                                               0x00,  // IO cap.: DisplayOnly
                                               0x00,  // OOB: not present
                                               0x04,  // AuthReq: MITM required
                                               0x07,  // encr. key size: 7 (default min)
                                               0x01,  // initiator key dist.: encr. key only
                                               0x01   // responder key dist.: encr. key only
  );
  const auto kFailure = CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                                               0x03   // reason: Authentication requirements
  );
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>(),
                               .io_capability = IOCapability::kNoInputNoOutput};
  NewPhase1(Role::kResponder, phase_args);

  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kAuthenticationRequirements, listener()->last_error().protocol_error());
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderJustWorks) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x01,  // AuthReq: bondable mode, ITM not required
      0x07,  // encr. key size: 7 (default min)
      0x02,  // initiator keys: identity only
      0x03   // responder keys: enc key and identity
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x02,  // initiator keys: identity only
      0x01   // responder keys: enc key
  );
  // clang-format on
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>(),
                               .io_capability = IOCapability::kNoInputNoOutput};

  NewPhase1(Role::kResponder, phase_args);
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kJustWorks, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  ASSERT_TRUE(last_preq());
  ASSERT_TRUE(last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));

  // We send the LTK when we are the responder.
  EXPECT_TRUE(KeyDistGen::kEncKey & features().local_key_distribution);

  // The remote should send us identity information since we requested it and it
  // promised it.
  EXPECT_TRUE(KeyDistGen::kIdKey & features().remote_key_distribution);
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderSendsOnlyRequestedKeys) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x01,  // AuthReq: bondable mode, MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x02,  // initiator keys: identity only
      0x02   // responder keys: identity only. Responder doesn't have it & shouldn't send it
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x02,  // initiator keys: identity only
      0x00   // responder keys: none
  );
  // clang-format on
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>(),
                               .io_capability = IOCapability::kNoInputNoOutput};
  NewPhase1(Role::kResponder, phase_args);
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });

  ASSERT_TRUE(Expect(kResponse));
  ASSERT_EQ(1, feature_exchange_count());
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderMITM) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x02,  // IO cap.: KeyboardOnly
      0x00,  // OOB: not present
      0b00000101,  // AuthReq: Bonding, MITM required
      0x07,  // encr. key size: 7 (default min)
      0x02,  // initiator keys: identity only
      0x03   // responder keys: enc key and identity
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x01,  // IO cap.: DisplayYesNo
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x02,  // initiator keys: identity only
      0x01   // responder keys: enc key
  );
  // clang-format on
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>(),
                               .io_capability = IOCapability::kDisplayYesNo};
  NewPhase1(Role::kResponder, phase_args);

  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  ASSERT_TRUE(last_preq());
  ASSERT_TRUE(last_pres());
  EXPECT_TRUE(ContainersEqual(kRequest, *last_preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, *last_pres()));

  // We send the LTK when we are the responder.
  EXPECT_TRUE(KeyDistGen::kEncKey & features().local_key_distribution);

  // The remote should send us identity information since we requested it and it
  // promised it.
  EXPECT_TRUE(KeyDistGen::kIdKey & features().remote_key_distribution);

  // We should have set bondable mode as both sides enabled it
  EXPECT_TRUE(features().will_bond);
}

TEST_F(SMP_Phase1Test, UnsupportedCommandDuringPairing) {
  phase_1()->Start();

  const auto kExpected = CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                                                0x07   // reason: Command Not Supported
  );
  ReceiveAndExpect(CreateStaticByteBuffer(0xFF), kExpected);
  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(ErrorCode::kCommandNotSupported, listener()->last_error().protocol_error());
}

TEST_F(SMP_Phase1Test, OnSecurityRequestWhilePairing) {
  phase_1()->Start();

  // clang-format off
  const auto kSecurityRequest = CreateStaticByteBuffer(
      0x0B,  // code: Security Request
      0x00   // auth_req
  );
  // clang-format on

  fake_chan()->Receive(kSecurityRequest);
  RunLoopUntilIdle();

  // The security request while pairing should cause pairing to fail.
  EXPECT_EQ(1, listener()->pairing_error_count());
}

// Tests whether a request from a device with bondable mode enabled to a peer with non-bondable
// mode enabled will return a PairingFeatures with non-bondable mode enabled, the desired result.
TEST_F(SMP_Phase1Test, FeatureExchangeInitiatorReqBondResNoBond) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x00,  // AuthReq: no bonding, MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x00   // responder keys: none due to non-bondable mode
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  // Should be in non-bondable mode even though the Initiator specifies bonding, as kResponse
  // indicated that the peer follower does not support bonding.
  EXPECT_FALSE(features().will_bond);
  EXPECT_EQ(features().local_key_distribution, 0u);
  EXPECT_EQ(features().remote_key_distribution, 0u);
}

TEST_F(SMP_Phase1Test, FeatureExchangeInitiatorReqNoBondResBond) {
  auto phase_args = Phase1Args{.bondable_mode = BondableMode::NonBondable};
  NewPhase1(Role::kInitiator, phase_args);
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: non-bondable, SC/MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x00   // responder keys: none
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none - should not change request field
      0x00   // responder keys: none - should not change request field
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kRequest));

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  // Should be in non-bondable mode even though kResponse specifies bonding as local Bearer
  // is in non-bondable mode.
  ASSERT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().will_bond);
  EXPECT_EQ(features().local_key_distribution, 0u);
  EXPECT_EQ(features().remote_key_distribution, 0u);
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderReqBondResNoBond) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bondable, SC/MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: non-bondable to match local mode, even though kRequest was bondable
             //          MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none - should not change request field
      0x00   // responder keys: none - should not change request field
  );
  // clang-format on
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>(),
                               .bondable_mode = BondableMode::NonBondable};
  NewPhase1(Role::kResponder, phase_args);
  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  // Should be in non-bondable mode even though the peer requested bondable, as the Bearer was
  // created in non-bondable mode.
  EXPECT_FALSE(features().will_bond);
  EXPECT_EQ(features().local_key_distribution, 0u);
  EXPECT_EQ(features().remote_key_distribution, 0u);
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderReqNoBondResNoBond) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: non-bondable, SC/MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x00   // responder keys: none
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: non-bondable to match peer mode, even though Phase1 is bondable, no MITM.
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none - should not change request field
      0x00   // responder keys: none - should not change request field
  );
  // clang-format on
  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{
      .preq = reader.payload<PairingRequestParams>(),
      .bondable_mode = BondableMode::Bondable,  // local mode is bondable, although peer is not
  };
  NewPhase1(Role::kResponder, phase_args);
  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] { phase_1()->Start(); });
  ASSERT_TRUE(Expect(kResponse));

  // Should be in non-bondable mode even though Bearer was created in bondable mode as
  // kRequest indicated that peer does not support bonding.
  EXPECT_FALSE(features().will_bond);
  EXPECT_EQ(features().local_key_distribution, 0u);
  EXPECT_EQ(features().remote_key_distribution, 0u);
}

TEST_F(SMP_Phase1Test, FeatureExchangeResponderReqNoBondWithKeys) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: non-bondable, SC/MITM not required
      0x10,  // encr. key size: 16 (default max)
      0x03,  // initiator keys: enc key and identity info
      0x03   // responder keys: enc key and identity info
  );
  // clang-format on

  auto reader = PacketReader(&kRequest);
  auto phase_args = Phase1Args{.preq = reader.payload<PairingRequestParams>()};
  NewPhase1(Role::kResponder, phase_args);
  // Initiate the request in a loop task for Expect to detect it.
  phase_1()->Start();
  RunLoopUntilIdle();

  // Check that we fail with invalid parameters when a peer requests nonbondable mode
  // with a non-zero KeyDistGen field
  EXPECT_EQ(1, listener()->pairing_error_count());
  ASSERT_TRUE(listener()->last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, listener()->last_error().protocol_error());
}

}  // namespace
}  // namespace sm
}  // namespace bt
