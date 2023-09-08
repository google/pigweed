// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_3.h"

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/fake_phase_listener.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"

namespace bt::sm {
namespace {

using util::PacketSize;

const PairingFeatures kDefaultFeatures = {
    .initiator = true,
    .secure_connections = false,
    .will_bond = true,
    .generate_ct_key = std::optional<CrossTransportKeyAlgo>{std::nullopt},
    .method = PairingMethod::kJustWorks,
    .encryption_key_size = kMaxEncryptionKeySize,
    .local_key_distribution = KeyDistGen::kEncKey,  // kEncKey because it lets Phase 3 "just work"
    .remote_key_distribution = 0u};

const SecurityProperties kDefaultProperties(SecurityLevel::kEncrypted, kMaxEncryptionKeySize,
                                            /*secure_connections=*/false);

struct Phase3Args {
  PairingFeatures features = kDefaultFeatures;
  SecurityProperties le_props = kDefaultProperties;
};

const hci_spec::LinkKey kSampleLinkKey(/*value=*/UInt128{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                                         14, 15, 16},
                                       /*rand=*/0x1234, /*ediv=*/0x5678);

const DeviceAddress kSampleDeviceAddress(DeviceAddress::Type::kLEPublic, {1});

class Phase3Test : public l2cap::testing::FakeChannelTest {
 public:
  Phase3Test() = default;
  ~Phase3Test() override = default;

 protected:
  void SetUp() override { NewPhase3(); }

  void TearDown() override { phase_3_ = nullptr; }

  void NewPhase3(Phase3Args phase_args = Phase3Args(), bt::LinkType ll_type = bt::LinkType::kLE) {
    l2cap::ChannelId cid =
        ll_type == bt::LinkType::kLE ? l2cap::kLESMPChannelId : l2cap::kSMPChannelId;
    ChannelOptions options(cid);
    options.link_type = ll_type;

    listener_ = std::make_unique<FakeListener>();
    fake_chan_ = CreateFakeChannel(options);
    sm_chan_ = std::make_unique<PairingChannel>(fake_chan_->GetWeakPtr());
    auto role = phase_args.features.initiator ? Role::kInitiator : Role::kResponder;
    phase_3_ = std::make_unique<Phase3>(sm_chan_->GetWeakPtr(), listener_->as_weak_ptr(), role,
                                        phase_args.features, phase_args.le_props,
                                        [this](PairingData pairing_results) {
                                          phase_3_complete_count_++;
                                          pairing_data_ = pairing_results;
                                        });
  }

  auto Make128BitCmd(Code cmd_code, const UInt128& value) {
    StaticByteBuffer<PacketSize<UInt128>()> buffer;
    PacketWriter writer(cmd_code, &buffer);
    *writer.mutable_payload<UInt128>() = value;
    return buffer;
  }

  void Receive128BitCmd(Code cmd_code, const UInt128& value) {
    fake_chan()->Receive(Make128BitCmd(cmd_code, value));
  }

  auto MakeCentralIdentification(uint64_t random, uint16_t ediv) {
    StaticByteBuffer<PacketSize<CentralIdentificationParams>()> buffer;
    PacketWriter writer(kCentralIdentification, &buffer);
    auto* params = writer.mutable_payload<CentralIdentificationParams>();
    params->ediv = htole16(ediv);
    params->rand = htole64(random);
    return buffer;
  }

  void ReceiveCentralIdentification(uint64_t random, uint16_t ediv) {
    fake_chan()->Receive(MakeCentralIdentification(random, ediv));
  }

  auto MakeIdentityAddress(const DeviceAddress& address) {
    StaticByteBuffer<PacketSize<IdentityAddressInformationParams>()> buffer;
    PacketWriter writer(kIdentityAddressInformation, &buffer);
    auto* params = writer.mutable_payload<IdentityAddressInformationParams>();
    params->type = address.type() == DeviceAddress::Type::kLEPublic ? AddressType::kPublic
                                                                    : AddressType::kStaticRandom;
    params->bd_addr = address.value();
    return buffer;
  }

  void ReceiveIdentityAddress(const DeviceAddress& address) {
    fake_chan()->Receive(MakeIdentityAddress(address));
  }

  static std::pair<Code, UInt128> ExtractCodeAnd128BitCmd(ByteBufferPtr sdu) {
    BT_ASSERT_MSG(sdu, "Tried to ExtractCodeAnd128BitCmd from nullptr in test");
    auto maybe_reader = ValidPacketReader::ParseSdu(sdu);
    BT_ASSERT_MSG(maybe_reader.is_ok(), "Tried to ExtractCodeAnd128BitCmd from invalid SMP packet");
    return {maybe_reader.value().code(), maybe_reader.value().payload<UInt128>()};
  }

  static void ExpectEncryptionInfo(ByteBufferPtr sdu,
                                   std::optional<EncryptionInformationParams>* out_ltk_bytes,
                                   std::optional<CentralIdentificationParams>* out_central_id) {
    fit::result<ErrorCode, ValidPacketReader> reader = ValidPacketReader::ParseSdu(sdu);
    ASSERT_TRUE(reader.is_ok());
    if (reader.value().code() == kEncryptionInformation) {
      *out_ltk_bytes = reader.value().payload<EncryptionInformationParams>();
    } else if (reader.value().code() == kCentralIdentification) {
      *out_central_id = reader.value().payload<CentralIdentificationParams>();
    } else {
      ADD_FAILURE() << "Only expected LTK packets";
    }
  }

  static void ExpectIdentity(ByteBufferPtr sdu, std::optional<IRK>* out_irk,
                             std::optional<IdentityAddressInformationParams>* out_id_address) {
    fit::result<ErrorCode, ValidPacketReader> reader = ValidPacketReader::ParseSdu(sdu);
    ASSERT_TRUE(reader.is_ok());
    if (reader.value().code() == kIdentityInformation) {
      *out_irk = reader.value().payload<IRK>();
    } else if (reader.value().code() == kIdentityAddressInformation) {
      *out_id_address = reader.value().payload<IdentityAddressInformationParams>();
    } else {
      ADD_FAILURE() << "Only expected identity information packets";
    }
  }

  void DestroyPhase3() { phase_3_.reset(nullptr); }
  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  Phase3* phase_3() { return phase_3_.get(); }
  FakeListener* listener() { return listener_.get(); }

  int phase_3_complete_count() const { return phase_3_complete_count_; }
  const PairingData& pairing_data() const { return pairing_data_; }

 private:
  std::unique_ptr<FakeListener> listener_;
  std::unique_ptr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<PairingChannel> sm_chan_;
  std::unique_ptr<Phase3> phase_3_;
  int phase_3_complete_count_ = 0;
  PairingData pairing_data_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Phase3Test);
};

using Phase3DeathTest = Phase3Test;

TEST_F(Phase3DeathTest, NoLocalLtkDistributionDuringSecureConnections) {
  Phase3Args args;
  args.features.secure_connections = true;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  ASSERT_DEATH_IF_SUPPORTED(NewPhase3(args), ".*Secure Connections.*");
}

TEST_F(Phase3DeathTest, NoRemoteLtkDistributionDuringSecureConnections) {
  Phase3Args args;
  args.features.secure_connections = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  ASSERT_DEATH_IF_SUPPORTED(NewPhase3(args), ".*Secure Connections.*");
}

TEST_F(Phase3DeathTest, CannotDistributeKeysOnUnencryptedChannel) {
  Phase3Args args;
  args.le_props = SecurityProperties(SecurityLevel::kNoSecurity, kMaxEncryptionKeySize,
                                     /*secure_connections=*/false);
  ASSERT_DEATH_IF_SUPPORTED(NewPhase3(args), ".*NoSecurity.*");
}

TEST_F(Phase3DeathTest, Phase3MustDistributeKeys) {
  Phase3Args args;
  args.features.remote_key_distribution = args.features.local_key_distribution = 0;
  // Phase 3 should only be instantiated if there are keys to distribute
  ASSERT_DEATH_IF_SUPPORTED(NewPhase3(args), ".*HasKeysToDistribute.*");
}

// The peer sends EDIV and Rand before LTK.
TEST_F(Phase3Test, EncryptionInformationReceivedTwice) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  ByteBufferPtr sent = nullptr;
  fake_chan()->SetSendCallback([&](ByteBufferPtr sdu) { sent = std::move(sdu); }, pw_dispatcher());
  phase_3()->Start();
  Receive128BitCmd(kEncryptionInformation, UInt128());
  RunLoopUntilIdle();
  ASSERT_FALSE(sent);
  // When we receive the second Encryption Info packet, we should respond with pairing failed, as a
  // device should only ever send one Encryption Info packet in Phase 3.
  const auto kEncryptionInformationCmd = Make128BitCmd(kEncryptionInformation, UInt128());
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kEncryptionInformationCmd, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

// The peer sends EDIV and Rand before LTK.
TEST_F(Phase3Test, CentralIdentificationReceivedInWrongOrder) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  phase_3()->Start();

  // When we receive the second Encryption Info packet, we should respond with pairing failed, as a
  // device should only ever send one Encryption Info packet in Phase 3.
  StaticByteBuffer<PacketSize<CentralIdentificationParams>()> central_id_packet;
  PacketWriter p(kCentralIdentification, &central_id_packet);
  *p.mutable_payload<CentralIdentificationParams>() = Random<CentralIdentificationParams>();
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(central_id_packet, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

// The peer sends the sample Rand from the specification doc
TEST_F(Phase3Test, ReceiveExampleLtkAborts) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  // Sample data from spec V5.0 Vol. 6 Part C Section 1
  const UInt128 kLtkSample = {0xBF, 0x01, 0xFB, 0x9D, 0x4E, 0xF3, 0xBC, 0x36,
                              0xD8, 0x74, 0xF5, 0x39, 0x41, 0x38, 0x68, 0x4C};

  phase_3()->Start();

  // Pairing should abort when receiving sample LTK data
  const auto kEncryptionInformationCmd = Make128BitCmd(kEncryptionInformation, kLtkSample);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kEncryptionInformationCmd, kExpectedFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

// The peer sends the sample LTK from the specification doc
TEST_F(Phase3Test, ReceiveExampleRandAborts) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  // Sample data from spec V5.0 Vol. 6 Part C Section 1
  const uint64_t kRandSample = 0xABCDEF1234567890;
  const uint16_t kEDiv = 20;

  phase_3()->Start();

  // Pairing should still proceed after accepting the LTK
  Receive128BitCmd(kEncryptionInformation, UInt128());
  RunLoopUntilIdle();
  ASSERT_EQ(0, listener()->pairing_error_count());
  ASSERT_EQ(0, phase_3_complete_count());
  // We disallow pairing with spec sample values as using known values for encryption parameters
  // is inherently unsafe.
  const auto kCentralIdentificationCmd = MakeCentralIdentification(kRandSample, kEDiv);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kCentralIdentificationCmd, kExpectedFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

// The peer sends us an LTK that is longer than the negotiated maximum key size
TEST_F(Phase3Test, ReceiveTooLongLTK) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  args.features.encryption_key_size = 8;
  NewPhase3(args);
  // Ltk with 9 bytes set is longer than the negotiated max of 8 bytes.
  const UInt128 kTooLongLtk{1, 2, 3, 4, 5, 6, 7, 8, 9};
  // Pairing should abort when receiving sample LTK data
  const auto kEncryptionInformationCmd = Make128BitCmd(kEncryptionInformation, kTooLongLtk);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kInvalidParameters};
  ASSERT_TRUE(ReceiveAndExpect(kEncryptionInformationCmd, kExpectedFailure));
  EXPECT_EQ(Error(ErrorCode::kInvalidParameters), listener()->last_error());
}

TEST_F(Phase3Test, CentralIdentificationReceivedTwice) {
  Phase3Args args;
  args.features.initiator = true;
  // The local device must expect both an Encryption and ID key from the peer, or it would start
  // sending messages after the first Central ID, which is not the behavior checked in this test.
  args.features.remote_key_distribution = KeyDistGen::kEncKey | KeyDistGen::kIdKey;
  NewPhase3(args);
  constexpr uint16_t kEdiv = 1;
  constexpr uint64_t kRand = 2;

  phase_3()->Start();
  // Send duplicate central identification. If Phase 3 receives multiple Central Identification
  // commands before completing, it should abort the pairing.
  Receive128BitCmd(kEncryptionInformation, UInt128());
  const auto kCentralIdentificationCmd = MakeCentralIdentification(kRand, kEdiv);
  fake_chan()->Receive(kCentralIdentificationCmd);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kCentralIdentificationCmd, kExpectedFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
}

// Pairing completes after obtaining encryption information only.
TEST_F(Phase3Test, InitiatorReceivesEncKey) {
  const LTK kExpectedLtk = LTK(kDefaultProperties, kSampleLinkKey);
  Phase3Args args;
  args.features.initiator = true;
  args.features.secure_connections = false;
  args.features.local_key_distribution = 0u;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  args.le_props = kExpectedLtk.security();
  NewPhase3(args);
  size_t sent_msg_count = 0;
  fake_chan()->SetSendCallback([&](ByteBufferPtr /*ignore*/) { sent_msg_count++; },
                               pw_dispatcher());

  phase_3()->Start();
  Receive128BitCmd(kEncryptionInformation, kExpectedLtk.key().value());
  ReceiveCentralIdentification(kExpectedLtk.key().rand(), kExpectedLtk.key().ediv());
  RunLoopUntilIdle();

  // We were not supposed to distribute any keys in Phase 3
  EXPECT_EQ(0u, sent_msg_count);

  // Pairing should have succeeded
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(pairing_data().peer_ltk.has_value());
  EXPECT_EQ(kExpectedLtk, *pairing_data().peer_ltk);
}

TEST_F(Phase3Test, InitiatorSendsLocalIdKey) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.local_key_distribution = KeyDistGen::kIdKey;
  args.features.remote_key_distribution = 0u;
  args.le_props = kDefaultProperties;
  NewPhase3(args);
  std::optional<IRK> irk = std::nullopt;
  std::optional<IdentityAddressInformationParams> identity_addr = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectIdentity(std::move(sdu), &irk, &identity_addr); },
      pw_dispatcher());
  IdentityInfo kLocalIdentity{.irk = Random<IRK>(), .address = kSampleDeviceAddress};
  listener()->set_identity_info(kLocalIdentity);
  phase_3()->Start();
  RunLoopUntilIdle();

  // Local ID Info should be sent to the peer & pairing should be complete
  ASSERT_TRUE(irk.has_value());
  ASSERT_EQ(kLocalIdentity.irk, *irk);
  ASSERT_TRUE(identity_addr.has_value());
  ASSERT_EQ(kLocalIdentity.address.value(), identity_addr->bd_addr);
  EXPECT_EQ(1, phase_3_complete_count());
  // We should have stored no PairingData, as the peer did not send us any
  ASSERT_FALSE(pairing_data().identity_address.has_value());
  ASSERT_FALSE(pairing_data().irk.has_value());
  ASSERT_FALSE(pairing_data().peer_ltk.has_value());
  ASSERT_FALSE(pairing_data().local_ltk.has_value());
}

TEST_F(Phase3Test, InitiatorSendsEncKey) {
  Phase3Args args;
  args.features.initiator = true;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  args.features.remote_key_distribution = 0u;
  args.le_props = kDefaultProperties;
  NewPhase3(args);
  std::optional<EncryptionInformationParams> ltk_bytes = std::nullopt;
  std::optional<CentralIdentificationParams> central_id = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectEncryptionInfo(std::move(sdu), &ltk_bytes, &central_id); },
      pw_dispatcher());
  phase_3()->Start();
  RunLoopUntilIdle();

  // Local LTK should be sent to the peer & pairing should be complete
  EXPECT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(ltk_bytes.has_value());
  ASSERT_TRUE(central_id.has_value());
  ASSERT_TRUE(pairing_data().local_ltk.has_value());
  EXPECT_EQ(pairing_data().local_ltk->key(),
            hci_spec::LinkKey(*ltk_bytes, central_id->rand, central_id->ediv));
}

TEST_F(Phase3Test, InitiatorReceivesThenSendsEncKey) {
  const LTK kExpectedLtk = LTK(kDefaultProperties, kSampleLinkKey);
  Phase3Args args;
  args.features.initiator = true;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  args.le_props = kExpectedLtk.security();
  NewPhase3(args);
  std::optional<EncryptionInformationParams> ltk_bytes = std::nullopt;
  std::optional<CentralIdentificationParams> central_id = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectEncryptionInfo(std::move(sdu), &ltk_bytes, &central_id); },
      pw_dispatcher());
  phase_3()->Start();
  Receive128BitCmd(kEncryptionInformation, kExpectedLtk.key().value());
  ReceiveCentralIdentification(kExpectedLtk.key().rand(), kExpectedLtk.key().ediv());
  RunLoopUntilIdle();

  // Local LTK should be sent to the peer & pairing should be complete
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, phase_3_complete_count());

  ASSERT_TRUE(ltk_bytes.has_value());
  ASSERT_TRUE(central_id.has_value());
  ASSERT_TRUE(pairing_data().local_ltk.has_value());
  EXPECT_EQ(pairing_data().local_ltk->key(),
            hci_spec::LinkKey(*ltk_bytes, central_id->rand, central_id->ediv));

  ASSERT_TRUE(pairing_data().peer_ltk.has_value());
  EXPECT_EQ(kExpectedLtk, *pairing_data().peer_ltk);
}

// Tests that pairing aborts if the local ID key doesn't exist but we'd already agreed to send it.
TEST_F(Phase3Test, AbortsIfLocalIdKeyIsRemoved) {
  Phase3Args args;
  args.features.local_key_distribution = KeyDistGen::kIdKey;
  NewPhase3(args);
  listener()->set_identity_info(std::nullopt);

  async::PostTask(dispatcher(), [this] { phase_3()->Start(); });
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(Expect(kExpectedFailure));

  EXPECT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

TEST_F(Phase3Test, IRKReceivedTwice) {
  Phase3Args args;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  NewPhase3(args);

  Receive128BitCmd(kIdentityInformation, UInt128());
  RunLoopUntilIdle();

  // Should be waiting for identity address.
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(0, phase_3_complete_count());

  // Send an IRK again. This should cause pairing to fail.
  const auto kIdentityInformationCmd = Make128BitCmd(kIdentityInformation, UInt128());
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kIdentityInformationCmd, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

// The responder sends its identity address before sending its IRK.
TEST_F(Phase3Test, IdentityAddressReceivedInWrongOrder) {
  Phase3Args args;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  NewPhase3(args);

  const auto kIdentityAddressCmd = MakeIdentityAddress(kSampleDeviceAddress);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kIdentityAddressCmd, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

TEST_F(Phase3Test, IdentityAddressReceivedTwice) {
  Phase3Args args;
  // We tell Phase 3 to expect the sign key even though we don't yet support it so that pairing
  // does not complete after receiving the first IdentityAddress
  args.features.remote_key_distribution = KeyDistGen::kIdKey | KeyDistGen::kSignKey;
  NewPhase3(args);
  phase_3()->Start();

  Receive128BitCmd(kIdentityInformation, UInt128());
  ReceiveIdentityAddress(kSampleDeviceAddress);
  RunLoopUntilIdle();

  // Should not complete as we still have not obtained all the requested keys.
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(0, phase_3_complete_count());

  // Send the IdentityAddress again. This should cause pairing to fail.
  const auto kIdentityAddressCmd = MakeIdentityAddress(kSampleDeviceAddress);
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ASSERT_TRUE(ReceiveAndExpect(kIdentityAddressCmd, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

TEST_F(Phase3Test, BadIdentityAddressType) {
  Phase3Args args;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  NewPhase3(args);
  phase_3()->Start();
  // Receive a generic IdentityInfo packet so we are prepared for the Identity Addr pacekt
  Receive128BitCmd(kIdentityInformation, UInt128());

  StaticByteBuffer<PacketSize<IdentityAddressInformationParams>()> addr;
  PacketWriter writer(kIdentityAddressInformation, &addr);
  auto* params = writer.mutable_payload<IdentityAddressInformationParams>();
  // The only valid address type values are 0 or 1 (V5.0 Vol. 3 Part H 3.6.5)
  const uint8_t kInvalidAddrType = 0xFF;
  params->type = *reinterpret_cast<const AddressType*>(&kInvalidAddrType);
  params->bd_addr = DeviceAddressBytes({1, 2, 3, 4, 5, 6});

  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kInvalidParameters};
  ASSERT_TRUE(ReceiveAndExpect(addr, kExpectedFailure));
  EXPECT_EQ(1, listener()->pairing_error_count());
}

// Pairing completes after obtaining identity information only.
TEST_F(Phase3Test, InitiatorCompleteWithIdKey) {
  const Key kIrk = Key(kDefaultProperties, {1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0});
  Phase3Args args;
  args.features.local_key_distribution = 0u;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  args.le_props = kIrk.security();
  NewPhase3(args);
  size_t sent_msg_count = 0;
  fake_chan()->SetSendCallback([&](ByteBufferPtr /*ignore*/) { sent_msg_count++; },
                               pw_dispatcher());

  phase_3()->Start();
  Receive128BitCmd(kIdentityInformation, kIrk.value());
  ReceiveIdentityAddress(kSampleDeviceAddress);
  RunLoopUntilIdle();

  // We were not supposed to distribute any keys in Phase 3
  EXPECT_EQ(0u, sent_msg_count);

  // Pairing should have succeeded
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(pairing_data().irk.has_value());
  EXPECT_EQ(kIrk, *pairing_data().irk);
}

// Pairing completes after obtaining identity information only.
TEST_F(Phase3Test, InitiatorCompleteWithEncAndIdKey) {
  const LTK kExpectedLtk = LTK(kDefaultProperties, kSampleLinkKey);
  const Key kIrk = Key(kDefaultProperties, {1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0});
  Phase3Args args;
  args.features.initiator = true;
  args.features.secure_connections = false;
  args.features.local_key_distribution = 0u;
  args.features.remote_key_distribution = KeyDistGen::kEncKey | KeyDistGen::kIdKey;
  args.le_props = kExpectedLtk.security();
  NewPhase3(args);
  size_t sent_msg_count = 0;
  fake_chan()->SetSendCallback([&](ByteBufferPtr /*ignore*/) { sent_msg_count++; },
                               pw_dispatcher());

  phase_3()->Start();
  Receive128BitCmd(kEncryptionInformation, kExpectedLtk.key().value());
  ReceiveCentralIdentification(kExpectedLtk.key().rand(), kExpectedLtk.key().ediv());
  Receive128BitCmd(kIdentityInformation, kIrk.value());
  ReceiveIdentityAddress(kSampleDeviceAddress);
  RunLoopUntilIdle();

  // We were not supposed to distribute any keys in Phase 3
  EXPECT_EQ(0u, sent_msg_count);

  // Pairing should have succeeded
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(pairing_data().peer_ltk.has_value());
  EXPECT_EQ(kExpectedLtk, *pairing_data().peer_ltk);
  ASSERT_TRUE(pairing_data().irk.has_value());
  EXPECT_EQ(kIrk, *pairing_data().irk);
}

TEST_F(Phase3Test, ResponderLTKDistributionNoRemoteKeys) {
  Phase3Args args;
  args.features.initiator = false;
  args.features.secure_connections = false;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  args.features.remote_key_distribution = 0u;
  args.le_props = kDefaultProperties;
  NewPhase3(args);
  std::optional<EncryptionInformationParams> ltk_bytes = std::nullopt;
  std::optional<CentralIdentificationParams> central_id = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectEncryptionInfo(std::move(sdu), &ltk_bytes, &central_id); },
      pw_dispatcher());
  phase_3()->Start();
  RunLoopUntilIdle();
  // We should have sent both the Encryption Info and Central ID to the peer
  ASSERT_TRUE(ltk_bytes.has_value());
  ASSERT_TRUE(central_id.has_value());

  // We should have notified the callback with the LTK
  ASSERT_TRUE(pairing_data().local_ltk.has_value());
  // The LTK we sent to the peer should match the one we notified callbacks with
  ASSERT_EQ(hci_spec::LinkKey(*ltk_bytes, central_id->rand, central_id->ediv),
            pairing_data().local_ltk->key());
}

TEST_F(Phase3Test, ResponderAcceptsInitiatorEncKey) {
  const LTK kExpectedLtk = LTK(kDefaultProperties, kSampleLinkKey);
  Phase3Args args;
  args.features.initiator = false;
  args.features.secure_connections = false;
  args.features.local_key_distribution = 0u;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  args.le_props = kExpectedLtk.security();
  NewPhase3(args);

  size_t sent_msg_count = 0;
  fake_chan()->SetSendCallback([&](ByteBufferPtr /*ignore*/) { sent_msg_count++; },
                               pw_dispatcher());

  phase_3()->Start();
  Receive128BitCmd(kEncryptionInformation, kExpectedLtk.key().value());
  ReceiveCentralIdentification(kExpectedLtk.key().rand(), kExpectedLtk.key().ediv());
  RunLoopUntilIdle();

  // We were not supposed to distribute any keys in Phase 3
  EXPECT_EQ(0u, sent_msg_count);

  // Pairing should have succeeded
  EXPECT_EQ(0, listener()->pairing_error_count());
  EXPECT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(pairing_data().peer_ltk.has_value());
  EXPECT_EQ(kExpectedLtk, *pairing_data().peer_ltk);
}

TEST_F(Phase3Test, ResponderLTKDistributionWithRemoteKeys) {
  const Key kIrk = Key(kDefaultProperties, {1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0});
  Phase3Args args;
  args.features.initiator = false;
  args.features.secure_connections = false;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  args.le_props = kDefaultProperties;
  NewPhase3(args);
  std::optional<EncryptionInformationParams> ltk_bytes = std::nullopt;
  std::optional<CentralIdentificationParams> central_id = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectEncryptionInfo(std::move(sdu), &ltk_bytes, &central_id); },
      pw_dispatcher());
  phase_3()->Start();
  RunLoopUntilIdle();
  // We should have sent the Encryption Info and Central ID & be waiting for the peer's IRK
  ASSERT_TRUE(ltk_bytes.has_value());
  ASSERT_TRUE(central_id.has_value());
  ASSERT_EQ(0, phase_3_complete_count());

  const hci_spec::LinkKey kExpectedKey =
      hci_spec::LinkKey(*ltk_bytes, central_id->rand, central_id->ediv);
  // Reset ltk_bytes & central_id to verify that we don't send any further messages
  ltk_bytes.reset();
  central_id.reset();
  Receive128BitCmd(kIdentityInformation, kIrk.value());
  ReceiveIdentityAddress(kSampleDeviceAddress);
  RunLoopUntilIdle();

  ASSERT_FALSE(ltk_bytes.has_value());
  ASSERT_FALSE(central_id.has_value());
  // We should have notified the callback with the LTK & IRK
  ASSERT_TRUE(pairing_data().local_ltk.has_value());
  ASSERT_TRUE(pairing_data().irk.has_value());
  // The LTK we sent to the peer should be equal to the one provided to the callback.
  ASSERT_EQ(kExpectedKey, pairing_data().local_ltk->key());
  // The IRK we notified the callback with should match the one we sent.
  ASSERT_EQ(kIrk, *pairing_data().irk);
}

// Locally generated ltk length should match max key length specified
TEST_F(Phase3Test, ResponderLocalLTKMaxLength) {
  const uint16_t kNegotiatedMaxKeySize = 7;
  Phase3Args args;
  args.features.initiator = false;
  args.features.encryption_key_size = kNegotiatedMaxKeySize;
  args.features.local_key_distribution = KeyDistGen::kEncKey;
  args.features.remote_key_distribution = 0u;
  NewPhase3(args);
  std::optional<EncryptionInformationParams> ltk_bytes = std::nullopt;
  std::optional<CentralIdentificationParams> central_id = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectEncryptionInfo(std::move(sdu), &ltk_bytes, &central_id); },
      pw_dispatcher());
  phase_3()->Start();
  RunLoopUntilIdle();

  // Local LTK, EDiv, and Rand should be sent to the peer & listener should be notified.
  ASSERT_TRUE(ltk_bytes.has_value());
  ASSERT_TRUE(central_id.has_value());
  EXPECT_EQ(1, phase_3_complete_count());
  EXPECT_EQ(hci_spec::LinkKey(*ltk_bytes, central_id->rand, central_id->ediv),
            pairing_data().local_ltk->key());

  // Ensure that most significant (16 - kNegotiatedMaxKeySize) bytes are zero.
  auto ltk = pairing_data().local_ltk->key().value();
  for (auto i = kNegotiatedMaxKeySize; i < ltk.size(); i++) {
    EXPECT_TRUE(ltk[i] == 0);
  }
}

TEST_F(Phase3Test, ResponderLocalIdKeyDistributionWithRemoteKeys) {
  Phase3Args args;
  args.features.initiator = false;
  args.features.local_key_distribution = KeyDistGen::kIdKey;
  args.features.remote_key_distribution = KeyDistGen::kIdKey;
  args.le_props = kDefaultProperties;
  NewPhase3(args);
  std::optional<IRK> irk = std::nullopt;
  std::optional<IdentityAddressInformationParams> identity_addr = std::nullopt;
  fake_chan()->SetSendCallback(
      [&](ByteBufferPtr sdu) { ExpectIdentity(std::move(sdu), &irk, &identity_addr); },
      pw_dispatcher());
  IdentityInfo kLocalIdentity{.irk = Random<IRK>(), .address = kSampleDeviceAddress};
  listener()->set_identity_info(kLocalIdentity);
  phase_3()->Start();
  RunLoopUntilIdle();

  // Local ID Info should be sent to the peer & we should be waiting for the peer's ID info
  ASSERT_TRUE(irk.has_value());
  ASSERT_TRUE(identity_addr.has_value());
  EXPECT_EQ(0, phase_3_complete_count());
  EXPECT_EQ(kLocalIdentity.irk, *irk);
  EXPECT_EQ(kLocalIdentity.address.value(), identity_addr->bd_addr);

  // Ensure that most significant (16 - kNegotiatedMaxKeySize) bytes are zero.
  const Key kIrk(kDefaultProperties, Random<UInt128>());
  const DeviceAddress kPeerAddr(DeviceAddress::Type::kLEPublic, {2});
  Receive128BitCmd(kIdentityInformation, kIrk.value());
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  // Pairing should be complete with the peer's identity information.
  ASSERT_EQ(1, phase_3_complete_count());
  ASSERT_TRUE(pairing_data().irk.has_value());
  EXPECT_EQ(kIrk, *pairing_data().irk);
  ASSERT_TRUE(pairing_data().identity_address.has_value());
  EXPECT_EQ(kPeerAddr, *pairing_data().identity_address);
}

TEST_F(Phase3Test, ReceivePairingFailed) {
  Phase3Args args;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  phase_3()->Start();
  fake_chan()->Receive(StaticByteBuffer{kPairingFailed, ErrorCode::kPairingNotSupported});
  RunLoopUntilIdle();

  ASSERT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kPairingNotSupported), listener()->last_error());
}

TEST_F(Phase3Test, MalformedCommand) {
  Phase3Args args;
  args.features.remote_key_distribution = KeyDistGen::kEncKey;
  NewPhase3(args);
  phase_3()->Start();
  // The kEncryptionInformation packet is expected to have a 16 byte payload, not 1 byte.
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kInvalidParameters};
  ReceiveAndExpect(StaticByteBuffer{kEncryptionInformation, 0x01}, kExpectedFailure);

  ASSERT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kInvalidParameters), listener()->last_error());
}

TEST_F(Phase3Test, UnexpectedOpCode) {
  phase_3()->Start();
  // The Security Request is not expected during Phase 3.
  const StaticByteBuffer<PacketSize<ErrorCode>()> kExpectedFailure{kPairingFailed,
                                                                   ErrorCode::kUnspecifiedReason};
  ReceiveAndExpect(StaticByteBuffer{kSecurityRequest, AuthReq::kBondingFlag}, kExpectedFailure);
  ASSERT_EQ(1, listener()->pairing_error_count());
  EXPECT_EQ(Error(ErrorCode::kUnspecifiedReason), listener()->last_error());
}

}  // namespace
}  // namespace bt::sm
