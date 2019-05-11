// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bearer.h"

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"

namespace bt {
namespace sm {
namespace {

class SMP_BearerTest : public l2cap::testing::FakeChannelTest,
                       public Bearer::Listener {
 public:
  SMP_BearerTest() : weak_ptr_factory_(this) {}
  ~SMP_BearerTest() override = default;

 protected:
  void SetUp() override { NewBearer(); }

  void TearDown() override { bearer_ = nullptr; }

  void NewBearer(
      hci::Connection::Role role = hci::Connection::Role::kMaster,
      hci::Connection::LinkType ll_type = hci::Connection::LinkType::kLE,
      bool sc_supported = false,
      IOCapability io_capability = IOCapability::kNoInputNoOutput) {
    l2cap::ChannelId cid = ll_type == hci::Connection::LinkType::kLE
                               ? l2cap::kLESMPChannelId
                               : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    fake_chan_ = CreateFakeChannel(options);
    bearer_ =
        std::make_unique<Bearer>(fake_chan_, role, sc_supported, io_capability,
                                 weak_ptr_factory_.GetWeakPtr());
  }

  // Bearer::Listener override:
  void OnPairingFailed(Status error) override {
    pairing_error_count_++;
    last_error_ = error;
  }

  // Bearer::Listener override:
  void OnFeatureExchange(const PairingFeatures& features,
                         const ByteBuffer& preq,
                         const ByteBuffer& pres) override {
    feature_exchange_count_++;
    features_ = features;
    preq_ = DynamicByteBuffer(preq);
    pres_ = DynamicByteBuffer(pres);
  }

  // Bearer::Listener override:
  void OnPairingConfirm(const UInt128& confirm) override {
    confirm_value_count_++;
    confirm_value_ = confirm;
  }

  // Bearer::Listener override:
  void OnPairingRandom(const UInt128& random) override {
    random_value_count_++;
    random_value_ = random;
  }

  // Bearer::Listener override:
  void OnLongTermKey(const UInt128& ltk) override {
    ltk_count_++;
    ltk_ = ltk;
  }

  // Bearer::Listener override:
  void OnMasterIdentification(uint16_t ediv, uint64_t random) override {
    master_id_count_++;
    ediv_ = ediv;
    rand_ = random;
  }

  // Bearer::Listener override:
  void OnIdentityResolvingKey(const UInt128& irk) override {
    irk_count_++;
    irk_ = irk;
  }

  // Bearer::Listener override:
  void OnIdentityAddress(const DeviceAddress& address) override {
    identity_addr_count_++;
    identity_addr_ = address;
  }

  // Bearer::Listener override:
  void OnSecurityRequest(AuthReqField auth_req) override {
    security_request_count_++;
    security_request_auth_req_ = auth_req;
  }

  // Bearer::Listener override:
  bool HasIdentityInformation() override { return has_identity_info_; }

  Bearer* bearer() const { return bearer_.get(); }
  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }

  int pairing_error_count() const { return pairing_error_count_; }
  Status last_error() const { return last_error_; }

  int feature_exchange_count() const { return feature_exchange_count_; }
  const PairingFeatures& features() const { return features_; }
  const ByteBuffer& preq() const { return preq_; }
  const ByteBuffer& pres() const { return pres_; }

  int confirm_value_count() const { return confirm_value_count_; }
  int random_value_count() const { return random_value_count_; }
  int ltk_count() const { return ltk_count_; }
  int master_id_count() const { return master_id_count_; }
  int irk_count() const { return irk_count_; }
  int identity_addr_count() const { return identity_addr_count_; }
  int security_request_count() const { return security_request_count_; }

  const UInt128& confirm_value() const { return confirm_value_; }
  const UInt128& random_value() const { return random_value_; }
  const UInt128& ltk() const { return ltk_; }
  uint16_t ediv() const { return ediv_; }
  uint64_t rand() const { return rand_; }
  const UInt128& irk() const { return irk_; }
  const DeviceAddress& identity_addr() const { return identity_addr_; }
  AuthReqField security_request_auth_req() const {
    return security_request_auth_req_;
  }

  void set_has_identity_info(bool value) { has_identity_info_ = value; }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<Bearer> bearer_;

  int pairing_error_count_ = 0;
  Status last_error_;

  int feature_exchange_count_ = 0;
  PairingFeatures features_;
  DynamicByteBuffer pres_, preq_;

  int confirm_value_count_ = 0;
  int random_value_count_ = 0;
  int ltk_count_ = 0;
  int master_id_count_ = 0;
  int irk_count_ = 0;
  int identity_addr_count_ = 0;
  int security_request_count_ = 0;
  UInt128 confirm_value_;
  UInt128 random_value_;
  UInt128 ltk_;
  uint16_t ediv_ = 0;
  uint64_t rand_ = 0;
  UInt128 irk_;
  DeviceAddress identity_addr_;
  AuthReqField security_request_auth_req_ = 0u;
  bool has_identity_info_ = false;

  fxl::WeakPtrFactory<SMP_BearerTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_BearerTest);
};

TEST_F(SMP_BearerTest, PacketsWhileIdle) {
  int tx_count = 0;
  fake_chan()->SetSendCallback([&](auto) { tx_count++; }, dispatcher());

  // Packets received while idle should have no side effect.
  fake_chan()->Receive(BufferView());                    // empty invalid buffer
  fake_chan()->Receive(StaticByteBuffer<kLEMTU + 1>());  // exceeds MTU
  fake_chan()->Receive(CreateStaticByteBuffer(kPairingFailed));
  fake_chan()->Receive(CreateStaticByteBuffer(kPairingResponse));

  RunLoopFor(kPairingTimeout);

  EXPECT_EQ(0, tx_count);
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());

  // Abort should have no effect either.
  bearer()->Abort(ErrorCode::kPairingNotSupported);

  // Unrecognized packets should result in a PairingFailed packet.
  fake_chan()->Receive(CreateStaticByteBuffer(0xFF));
  RunLoopUntilIdle();

  EXPECT_EQ(1, tx_count);
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_BearerTest, FeatureExchangeErrorSlave) {
  NewBearer(hci::Connection::Role::kSlave);
  EXPECT_FALSE(bearer()->InitiateFeatureExchange());
}

TEST_F(SMP_BearerTest, FeatureExchangeStartDefaultParams) {
  // clang-format off
  const auto kExpected = CreateStaticByteBuffer(
      0x01,  // code: "Pairing Request"
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  // clang-format on

  int tx_count = 0;
  fake_chan()->SetSendCallback(
      [&](auto pdu) {
        tx_count++;
        EXPECT_TRUE(ContainersEqual(kExpected, *pdu));
      },
      dispatcher());
  EXPECT_TRUE(bearer()->InitiateFeatureExchange());

  RunLoopUntilIdle();

  EXPECT_EQ(1, tx_count);
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_FALSE(bearer()->InitiateFeatureExchange());
}

TEST_F(SMP_BearerTest, FeatureExchangeStartCustomParams) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kLE,
            true /* sc_supported */, IOCapability::kDisplayYesNo);
  bearer()->set_oob_available(true);
  bearer()->set_mitm_required(true);

  // clang-format off
  const auto kExpected = CreateStaticByteBuffer(
      0x01,        // code: "Pairing Request"
      0x01,        // IO cap.: DisplayYesNo
      0x01,        // OOB: present
      0b00001101,  // AuthReq: Bonding, SC, MITM
      0x10,        // encr. key size: 16 (default max)
      0x00,        // initiator keys: none
      0x03         // responder keys: enc key and identity info
  );
  // clang-format on

  int tx_count = 0;
  fake_chan()->SetSendCallback(
      [&](auto pdu) {
        tx_count++;
        EXPECT_TRUE(ContainersEqual(kExpected, *pdu));
      },
      dispatcher());
  EXPECT_TRUE(bearer()->InitiateFeatureExchange());

  RunLoopUntilIdle();

  EXPECT_EQ(1, tx_count);
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_FALSE(bearer()->InitiateFeatureExchange());
}

TEST_F(SMP_BearerTest, FeatureExchangeInitiatorWithIdentityInfo) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kLE,
            true /* sc_supported */, IOCapability::kDisplayYesNo);
  set_has_identity_info(true);
  bearer()->set_oob_available(true);
  bearer()->set_mitm_required(true);

  // clang-format off
  const auto kExpected = CreateStaticByteBuffer(
      0x01,        // code: "Pairing Request"
      0x01,        // IO cap.: DisplayYesNo
      0x01,        // OOB: present
      0b00001101,  // AuthReq: Bonding, SC, MITM
      0x10,        // encr. key size: 16 (default max)
      0x02,        // initiator keys: identity info
      0x03         // responder keys: enc key and identity info
  );
  // clang-format on

  int tx_count = 0;
  fake_chan()->SetSendCallback(
      [&](auto pdu) {
        tx_count++;
        EXPECT_TRUE(ContainersEqual(kExpected, *pdu));
      },
      dispatcher());
  EXPECT_TRUE(bearer()->InitiateFeatureExchange());

  RunLoopUntilIdle();

  EXPECT_EQ(1, tx_count);
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_FALSE(bearer()->InitiateFeatureExchange());
}

TEST_F(SMP_BearerTest, FeatureExchangeTimeout) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  RunLoopFor(kPairingTimeout);

  EXPECT_EQ(HostError::kTimedOut, last_error().error());
  EXPECT_TRUE(fake_chan()->link_error());
  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_BearerTest, Abort) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  bearer()->Abort(ErrorCode::kPairingNotSupported);
  EXPECT_EQ(ErrorCode::kPairingNotSupported, last_error().protocol_error());
  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_FALSE(fake_chan()->link_error());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());

  // Timer should have stopped.
  RunLoopFor(kPairingTimeout);

  EXPECT_EQ(1, pairing_error_count());
}

TEST_F(SMP_BearerTest, FeatureExchangePairingFailed) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  fake_chan()->Receive(CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x05   // reason: Pairing Not Supported
      ));
  RunLoopUntilIdle();

  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());
  EXPECT_EQ(ErrorCode::kPairingNotSupported, last_error().protocol_error());
}

TEST_F(SMP_BearerTest, FeatureExchangeLocalRejectsUnsupportedInitiatorKeys) {
  // We should reject a pairing response that requests keys that we don't
  // support.
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
  async::PostTask(dispatcher(),
                  [this] { bearer()->InitiateFeatureExchange(); });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_TRUE(last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_BearerTest, FeatureExchangeLocalRejectsUnsupportedResponderKeys) {
  // We should reject a pairing response that requests keys that we don't
  // support.
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
  async::PostTask(dispatcher(),
                  [this] { bearer()->InitiateFeatureExchange(); });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_TRUE(last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kInvalidParameters, last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

// Pairing should fail if MITM is required but the pairing method cannot provide
// it (due to I/O capabilities).
TEST_F(SMP_BearerTest, FeatureExchangeFailureAuthenticationRequirements) {
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
  async::PostTask(dispatcher(),
                  [this] { bearer()->InitiateFeatureExchange(); });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_TRUE(last_error().is_protocol_error());
  EXPECT_EQ(ErrorCode::kAuthenticationRequirements,
            last_error().protocol_error());
  EXPECT_EQ(0, feature_exchange_count());
}

TEST_F(SMP_BearerTest, FeatureExchangePairingResponseJustWorks) {
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
      0x00,  // AuthReq: MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(),
                  [this] { bearer()->InitiateFeatureExchange(); });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  // Pairing should continue until explicitly stopped.
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kJustWorks, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_TRUE(KeyDistGen::kEncKey & features().remote_key_distribution);
  EXPECT_TRUE(ContainersEqual(kRequest, preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, pres()));
}

// One of the devices requires MITM protection and the I/O capabilities can
// provide it.
TEST_F(SMP_BearerTest, FeatureExchangePairingResponseMITM) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kLE,
            false /* sc_supported */, IOCapability::kDisplayYesNo);

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
      0x04,  // AuthReq: MITM required
      0x07,  // encr. key size: 7 (default min)
      0x00,  // initiator keys: none
      0x01   // responder keys: enc key only
  );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(),
                  [this] { bearer()->InitiateFeatureExchange(); });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  // Pairing should continue until explicitly stopped.
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());

  EXPECT_TRUE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_TRUE(KeyDistGen::kEncKey & features().remote_key_distribution);
  EXPECT_TRUE(ContainersEqual(kRequest, preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, pres()));
}

TEST_F(SMP_BearerTest, FeatureExchangeEncryptionKeySize) {
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
  const auto kFailure =
      CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                             0x06   // reason: Encryption Key Size
      );
  // clang-format on

  // Initiate the request in a loop task for Expect to detect it.
  async::PostTask(dispatcher(), [this] {
    bearer()->InitiateFeatureExchange();
    EXPECT_TRUE(bearer()->pairing_started());
  });
  ASSERT_TRUE(Expect(kRequest));
  ASSERT_TRUE(bearer()->pairing_started());

  // We should receive a pairing response and reply back with Pairing Failed.
  EXPECT_TRUE(ReceiveAndExpect(kResponse, kFailure));

  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_EQ(0, feature_exchange_count());
  EXPECT_EQ(ErrorCode::kEncryptionKeySize, last_error().protocol_error());
}

TEST_F(SMP_BearerTest, FeatureExchangeResponderErrorMaster) {
  const auto kRequest =
      CreateStaticByteBuffer(0x01,  // code: Pairing Request
                             0x03,  // IO cap.: NoInputNoOutput
                             0x00,  // OOB: not present
                             0x00,  // AuthReq: no auth. request by default
                             0x10,  // encr. key size: 16 (default max)
                             0x01,  // initiator key dist.: encr. key only
                             0x01   // responder key dist.: encr. key only
      );
  const auto kFailure =
      CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                             0x07   // reason: Command Not Supported
      );

  NewBearer(hci::Connection::Role::kMaster);
  EXPECT_TRUE(ReceiveAndExpect(kRequest, kFailure));
  EXPECT_FALSE(bearer()->pairing_started());
}

TEST_F(SMP_BearerTest, FeatureExchangeResponderMalformedRequest) {
  const auto kMalformedRequest =
      CreateStaticByteBuffer(0x01,  // code: Pairing Request
                             0x03,  // IO cap.: NoInputNoOutput
                             0x00,  // OOB: not present
                             0x00,  // AuthReq: no auth. request by default
                             0x10,  // encr. key size: 16 (default max)
                             0x01   // initiator key dist.: encr. key only
                                    // Missing last byte
      );
  const auto kFailure =
      CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                             0x0A   // reason: Invalid Parameters
      );

  NewBearer(hci::Connection::Role::kMaster);
  EXPECT_TRUE(ReceiveAndExpect(kMalformedRequest, kFailure));
  EXPECT_FALSE(bearer()->pairing_started());
}

// Tests that the local responder does not request keys that the initiator
// cannot distribute.
TEST_F(SMP_BearerTest, FeatureExchangeLocalResponderRespectsInitiator) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: no auth. request by default
      0x10,  // encr. key size: 16 (default max)
      0x01,  // initiator key dist.: encr. key only
      0x00   // responder key dist.: none
  );
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

  NewBearer(hci::Connection::Role::kSlave);
  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_FALSE(features().remote_key_distribution);
}

// Tests that we (as the responder) request to distribute identity information
// if available.
TEST_F(SMP_BearerTest, FeatureExchangeResponderDistributesIdKey) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: no auth. request by default
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x02   // responder keys: identity info
  );
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

  NewBearer(hci::Connection::Role::kSlave);
  set_has_identity_info(true);

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_TRUE(features().local_key_distribution & KeyDistGen::kIdKey);
  EXPECT_FALSE(features().remote_key_distribution);
}

// Tests that we (as the responder) do not request to distribute identity
// information if the data is available but the initiator did not request it.
TEST_F(SMP_BearerTest, FeatureExchangeResponderRespectsInitiatorForIdKey) {
  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x00,  // AuthReq: no auth. request by default
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x00   // responder keys: none
  );
  const auto kResponse = CreateStaticByteBuffer(
      0x02,  // code: Pairing Response
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x00   // responder keys: none  // we shouldn't distribute the IdKey
  );
  // clang-format on

  NewBearer(hci::Connection::Role::kSlave);
  set_has_identity_info(true);

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().local_key_distribution);
  EXPECT_FALSE(features().remote_key_distribution);
}

// Tests that the pairing timer gets reset when a second Pairing Request is
// received.
TEST_F(SMP_BearerTest, FeatureExchangeResponderTimerRestarted) {
  NewBearer(hci::Connection::Role::kSlave);

  constexpr auto kThresholdSeconds = zx::sec(1);
  const auto kRequest =
      CreateStaticByteBuffer(0x01,  // code: Pairing Request
                             0x03,  // IO cap.: NoInputNoOutput
                             0x00,  // OOB: not present
                             0x00,  // AuthReq: no auth. request by default
                             0x10,  // encr. key size: 16 (default max)
                             0x01,  // initiator key dist.: encr. key only
                             0x01   // responder key dist.: encr. key only
      );
  fake_chan()->Receive(kRequest);
  RunLoopUntilIdle();
  ASSERT_TRUE(bearer()->pairing_started());

  // Advance the time to 1 second behind the end of the pairing timeout.
  RunLoopFor(kPairingTimeout - kThresholdSeconds);

  // The timer should not have expired.
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_FALSE(fake_chan()->link_error());
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_TRUE(last_error().is_success());

  // Send a second request which should restart the timer.
  fake_chan()->Receive(kRequest);
  RunLoopUntilIdle();
  EXPECT_TRUE(bearer()->pairing_started());

  // The old timeout should not expire when advance to 1 second behind the new
  // timeout.
  RunLoopFor(kPairingTimeout - kThresholdSeconds);
  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(0, pairing_error_count());

  RunLoopFor(kThresholdSeconds);
  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_EQ(HostError::kTimedOut, last_error().error());
}

// Pairing should fail if MITM is required but the pairing method cannot provide
// it (due to I/O capabilities).
TEST_F(SMP_BearerTest,
       FeatureExchangeResponderFailedAuthenticationRequirements) {
  NewBearer(hci::Connection::Role::kSlave);

  const auto kRequest =
      CreateStaticByteBuffer(0x01,  // code: Pairing Response
                             0x00,  // IO cap.: DisplayOnly
                             0x00,  // OOB: not present
                             0x04,  // AuthReq: MITM required
                             0x07,  // encr. key size: 7 (default min)
                             0x01,  // initiator key dist.: encr. key only
                             0x01   // responder key dist.: encr. key only
      );
  const auto kFailure =
      CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                             0x03   // reason: Authentication requirements
      );

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kFailure));
  EXPECT_EQ(1, pairing_error_count());
  EXPECT_FALSE(bearer()->pairing_started());
  EXPECT_EQ(ErrorCode::kAuthenticationRequirements,
            last_error().protocol_error());
}

TEST_F(SMP_BearerTest, FeatureExchangeResponderJustWorks) {
  NewBearer(hci::Connection::Role::kSlave);

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x00,  // AuthReq: MITM not required
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

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));

  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kJustWorks, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_TRUE(ContainersEqual(kRequest, preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, pres()));

  // We send the LTK when we are the responder.
  EXPECT_TRUE(KeyDistGen::kEncKey & features().local_key_distribution);

  // The remote should send us identity information since we requested it and it
  // promised it.
  EXPECT_TRUE(KeyDistGen::kIdKey & features().remote_key_distribution);
}

TEST_F(SMP_BearerTest, FeatureExchangeResponderSendsOnlyRequestedKeys) {
  NewBearer(hci::Connection::Role::kSlave);

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Response
      0x00,  // IO cap.: DisplayOnly
      0x00,  // OOB: not present
      0x00,  // AuthReq: MITM not required
      0x07,  // encr. key size: 7 (default min)
      0x02,  // initiator keys: identity only
      0x02   // responder keys: identity only. Responder shouldn't send it.
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

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));
}

TEST_F(SMP_BearerTest, FeatureExchangeResponderMITM) {
  NewBearer(hci::Connection::Role::kSlave, hci::Connection::LinkType::kLE,
            false /* sc_supported */, IOCapability::kDisplayYesNo);

  // clang-format off
  const auto kRequest = CreateStaticByteBuffer(
      0x01,  // code: Pairing Request
      0x02,  // IO cap.: KeyboardOnly
      0x00,  // OOB: not present
      0x04,  // AuthReq: MITM required
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

  EXPECT_TRUE(ReceiveAndExpect(kRequest, kResponse));

  EXPECT_TRUE(bearer()->pairing_started());
  EXPECT_EQ(0, pairing_error_count());
  EXPECT_EQ(1, feature_exchange_count());
  EXPECT_FALSE(features().initiator);
  EXPECT_FALSE(features().secure_connections);
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay, features().method);
  EXPECT_EQ(7, features().encryption_key_size);
  EXPECT_TRUE(ContainersEqual(kRequest, preq()));
  EXPECT_TRUE(ContainersEqual(kResponse, pres()));

  // We send the LTK when we are the responder.
  EXPECT_TRUE(KeyDistGen::kEncKey & features().local_key_distribution);

  // The remote should send us identity information since we requested it and it
  // promised it.
  EXPECT_TRUE(KeyDistGen::kIdKey & features().remote_key_distribution);
}

TEST_F(SMP_BearerTest, UnsupportedCommandDuringPairing) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  const auto kExpected =
      CreateStaticByteBuffer(0x05,  // code: Pairing Failed
                             0x07   // reason: Command Not Supported
      );
  ReceiveAndExpect(CreateStaticByteBuffer(0xFF), kExpected);
  EXPECT_FALSE(bearer()->pairing_started());
}

TEST_F(SMP_BearerTest, StopTimer) {
  const auto kResponse =
      CreateStaticByteBuffer(0x02,  // code: Pairing Response
                             0x00,  // IO cap.: DisplayOnly
                             0x00,  // OOB: not present
                             0x00,  // AuthReq: MITM not required
                             0x07,  // encr. key size: 7 (default min)
                             0x00,  // initiator key dist.: none
                             0x01   // responder key dist.: encr. key only
      );

  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  fake_chan()->Receive(kResponse);
  RunLoopUntilIdle();

  // Pairing should continue until explicitly stopped.
  EXPECT_TRUE(bearer()->pairing_started());

  bearer()->StopTimer();
  EXPECT_FALSE(bearer()->pairing_started());

  RunLoopFor(kPairingTimeout);
  EXPECT_EQ(0, pairing_error_count());
}

TEST_F(SMP_BearerTest, SendConfirmValueNotPairing) {
  ASSERT_FALSE(bearer()->pairing_started());

  UInt128 confirm;
  EXPECT_FALSE(bearer()->SendConfirmValue(confirm));
}

TEST_F(SMP_BearerTest, SendConfirmValueNotLE) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kACL,
            false /* sc_supported */, IOCapability::kDisplayYesNo);
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 confirm;
  EXPECT_FALSE(bearer()->SendConfirmValue(confirm));
}

TEST_F(SMP_BearerTest, SendConfirmValue) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 confirm{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

  // Initiate the request in a loop task so it runs after the call to Expect
  // below.
  async::PostTask(dispatcher(),
                  [&] { EXPECT_TRUE(bearer()->SendConfirmValue(confirm)); });

  // clang-format off
  EXPECT_TRUE(Expect(CreateStaticByteBuffer(
      // code: Pairing Confirm
      0x03,

      // Confirm value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  )));
  // clang-format on
}

TEST_F(SMP_BearerTest, OnPairingConfirmNotPairing) {
  // clang-format off
  const auto kPairingConfirm = CreateStaticByteBuffer(
      // code: Pairing Confirm
      0x03,

      // Confirm value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kPairingConfirm);
  RunLoopUntilIdle();
  EXPECT_EQ(0, confirm_value_count());
}

TEST_F(SMP_BearerTest, OnPairingConfirmNotLE) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kACL,
            false /* sc_supported */, IOCapability::kDisplayYesNo);
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kPairingConfirm = CreateStaticByteBuffer(
      // code: Pairing Confirm
      0x03,

      // Confirm value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  const auto kPairingFailed = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x07   // reason: Command Not Supported
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kPairingConfirm, kPairingFailed));
  EXPECT_EQ(0, confirm_value_count());
}

TEST_F(SMP_BearerTest, OnPairingConfirmMalformed) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kMalformedPairingConfirm = CreateStaticByteBuffer(
      // code: Pairing Confirm
      0x03,

      // Confirm value (1 octet too short)
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  );
  const auto kPairingFailed = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kMalformedPairingConfirm, kPairingFailed));
  EXPECT_EQ(0, confirm_value_count());
}

TEST_F(SMP_BearerTest, OnPairingConfirmCallback) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 kExpected{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

  // clang-format off
  const auto kPairingConfirm = CreateStaticByteBuffer(
      // code: Pairing Confirm
      0x03,

      // Confirm value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kPairingConfirm);
  RunLoopUntilIdle();
  EXPECT_EQ(1, confirm_value_count());
  EXPECT_EQ(kExpected, confirm_value());
}

TEST_F(SMP_BearerTest, SendRandomValueNotPairing) {
  ASSERT_FALSE(bearer()->pairing_started());

  UInt128 rand;
  EXPECT_FALSE(bearer()->SendRandomValue(rand));
}

TEST_F(SMP_BearerTest, SendRandomValueNotLE) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kACL,
            false /* sc_supported */, IOCapability::kDisplayYesNo);
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 rand;
  EXPECT_FALSE(bearer()->SendRandomValue(rand));
}

TEST_F(SMP_BearerTest, SendRandomValue) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 rand{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

  // Initiate the request in a loop task so it runs after the call to Expect
  // below.
  async::PostTask(dispatcher(),
                  [&] { EXPECT_TRUE(bearer()->SendRandomValue(rand)); });

  // clang-format off
  EXPECT_TRUE(Expect(CreateStaticByteBuffer(
      // code: Pairing Random
      0x04,

      // Confirm value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  )));
  // clang-format on
}

TEST_F(SMP_BearerTest, OnPairingRandomNotPairing) {
  // clang-format off
  const auto kPairingRandom = CreateStaticByteBuffer(
      // code: Pairing Random
      0x04,

      // Random value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kPairingRandom);
  RunLoopUntilIdle();
  EXPECT_EQ(0, random_value_count());
}

TEST_F(SMP_BearerTest, OnPairingRandomNotLE) {
  NewBearer(hci::Connection::Role::kMaster, hci::Connection::LinkType::kACL,
            false /* sc_supported */, IOCapability::kDisplayYesNo);
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kPairingRandom = CreateStaticByteBuffer(
      // code: Pairing Random
      0x04,

      // Random value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  const auto kPairingFailed = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x07   // reason: Command Not Supported
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kPairingRandom, kPairingFailed));
  EXPECT_EQ(0, random_value_count());
}

TEST_F(SMP_BearerTest, OnPairingRandomMalformed) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kMalformedPairingRandom = CreateStaticByteBuffer(
      // code: Pairing Random
      0x04,

      // Random value (1 octet too short)
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  );
  const auto kPairingFailed = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kMalformedPairingRandom, kPairingFailed));
  EXPECT_EQ(0, random_value_count());
}

TEST_F(SMP_BearerTest, OnPairingRandomCallback) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 kExpected{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

  // clang-format off
  const auto kPairingRandom = CreateStaticByteBuffer(
      // code: Pairing Random
      0x04,

      // Random value
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kPairingRandom);
  RunLoopUntilIdle();
  EXPECT_EQ(1, random_value_count());
  EXPECT_EQ(kExpected, random_value());
}

TEST_F(SMP_BearerTest, OnIdentityInformationNotPairing) {
  // clang-format off
  const auto kIdentityInfo = CreateStaticByteBuffer(
      // code: Identity Information
      0x08,

      // IRK
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kIdentityInfo);
  RunLoopUntilIdle();
  EXPECT_EQ(0, irk_count());
}

TEST_F(SMP_BearerTest, OnIdentityInformationMalformed) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kIdentityInfo = CreateStaticByteBuffer(
      // code: Identity Information
      0x08,

      // IRK (1 octet too short)
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  );
  // clang-format on

  fake_chan()->Receive(kIdentityInfo);
  RunLoopUntilIdle();
  EXPECT_EQ(0, irk_count());
}

TEST_F(SMP_BearerTest, OnIdentityInformationCallback) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  UInt128 kExpected{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};

  // clang-format off
  const auto kIdentityInfo = CreateStaticByteBuffer(
      // code: Identity Information
      0x08,

      // IRK
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  );
  // clang-format on

  fake_chan()->Receive(kIdentityInfo);
  RunLoopUntilIdle();
  EXPECT_EQ(kExpected, irk());
}

TEST_F(SMP_BearerTest, OnIdentityAddressInformationNotPairing) {
  // clang-format off
  const auto kIdentityAddr = CreateStaticByteBuffer(
      0x09,                               // code: Identity Address Information
      0x00,                               // type: public
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF  // BD_ADDR
  );
  // clang-format on

  fake_chan()->Receive(kIdentityAddr);
  RunLoopUntilIdle();
  EXPECT_EQ(0, identity_addr_count());
}

TEST_F(SMP_BearerTest, OnIdentityAddressInformationMalformed) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kIdentityAddr = CreateStaticByteBuffer(
      0x09,                               // code: Identity Address Information
      0x00,                               // type: public
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE        // BD_ADDR (1 byte too short)
  );
  // clang-format on

  fake_chan()->Receive(kIdentityAddr);
  RunLoopUntilIdle();
  EXPECT_EQ(0, identity_addr_count());
}

TEST_F(SMP_BearerTest, OnIdentityAddressInformationCallbackPublic) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  DeviceAddress kExpected(DeviceAddress::Type::kLEPublic, "FF:EE:DD:CC:BB:AA");

  // clang-format off
  const auto kIdentityAddr = CreateStaticByteBuffer(
      0x09,                               // code: Identity Address Information
      0x00,                               // type: public
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF  // BD_ADDR
  );
  // clang-format on

  fake_chan()->Receive(kIdentityAddr);
  RunLoopUntilIdle();
  EXPECT_EQ(1, identity_addr_count());
  EXPECT_EQ(kExpected, identity_addr());
}

TEST_F(SMP_BearerTest, OnIdentityAddressInformationCallbackRandom) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  DeviceAddress kExpected(DeviceAddress::Type::kLERandom, "FF:EE:DD:CC:BB:AA");

  // clang-format off
  const auto kIdentityAddr = CreateStaticByteBuffer(
      0x09,                               // code: Identity Address Information
      0x01,                               // type: random
      0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF  // BD_ADDR
  );
  // clang-format on

  fake_chan()->Receive(kIdentityAddr);
  RunLoopUntilIdle();
  EXPECT_EQ(1, identity_addr_count());
  EXPECT_EQ(kExpected, identity_addr());
}

TEST_F(SMP_BearerTest, OnSecurityRequestMalformed) {
  // clang-format off
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x0A   // reason: Invalid Parameters
  );
  const auto kSecurityRequest1 = CreateStaticByteBuffer(
      0x0B,        // code: Security Request
      0x00, 0x00   // malformed 2-byte payload
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kSecurityRequest1, kFailure));
  EXPECT_EQ(0, security_request_count());

  // clang-format off
  const auto kSecurityRequest2 = CreateStaticByteBuffer(
      0x0B  // code: Security Request
            // missing payload
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kSecurityRequest2, kFailure));
  EXPECT_EQ(0, security_request_count());
}

TEST_F(SMP_BearerTest, OnSecurityRequestWhilePairing) {
  bearer()->InitiateFeatureExchange();
  ASSERT_TRUE(bearer()->pairing_started());

  // clang-format off
  const auto kSecurityRequest = CreateStaticByteBuffer(
      0x0B,  // code: Security Request
      0x00   // auth_req
  );
  // clang-format on

  fake_chan()->Receive(kSecurityRequest);
  RunLoopUntilIdle();

  // The request should be ignored during pairing.
  EXPECT_EQ(0, security_request_count());
}

TEST_F(SMP_BearerTest, OnSecurityRequestFromMaster) {
  NewBearer(hci::Connection::Role::kSlave);

  // clang-format off
  const auto kFailure = CreateStaticByteBuffer(
      0x05,  // code: Pairing Failed
      0x07   // reason: Command Not Supported
  );
  const auto kSecurityRequest = CreateStaticByteBuffer(
      0x0B,  // code: Security Request
      0x00   // auth_req
  );
  // clang-format on

  EXPECT_TRUE(ReceiveAndExpect(kSecurityRequest, kFailure));
  EXPECT_EQ(0, security_request_count());
}

TEST_F(SMP_BearerTest, OnSecurityRequest) {
  constexpr AuthReqField kAuthReq = 5u;  // (value is unimportant)

  // clang-format off
  const auto kSecurityRequest = CreateStaticByteBuffer(
      0x0B,     // code: Security Request
      kAuthReq  // auth_req
  );
  // clang-format on

  fake_chan()->Receive(kSecurityRequest);
  RunLoopUntilIdle();

  // The request should be ignored during pairing.
  EXPECT_EQ(1, security_request_count());
  EXPECT_EQ(kAuthReq, security_request_auth_req());
}

}  // namespace
}  // namespace sm
}  // namespace bt
