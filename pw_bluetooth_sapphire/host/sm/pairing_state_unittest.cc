// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_state.h"

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "util.h"

namespace bt {

namespace sm {
namespace {

const DeviceAddress kLocalAddr(DeviceAddress::Type::kLEPublic,
                               {0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1});
const DeviceAddress kPeerAddr(DeviceAddress::Type::kLERandom, {0xB6, 0xB5, 0xB4, 0xB3, 0xB2, 0xB1});

class SMP_PairingStateTest : public l2cap::testing::FakeChannelTest,
                             public sm::PairingState::Delegate {
 public:
  SMP_PairingStateTest() : weak_ptr_factory_(this) {}
  ~SMP_PairingStateTest() override = default;

 protected:
  void TearDown() override {
    RunLoopUntilIdle();
    DestroyPairingState();
  }

  void NewPairingState(hci::Connection::Role role, IOCapability ioc) {
    // Setup fake SMP channel.
    ChannelOptions options(l2cap::kLESMPChannelId);
    fake_chan_ = CreateFakeChannel(options);
    fake_chan_->SetSendCallback(fit::bind_member(this, &SMP_PairingStateTest::OnDataReceived),
                                dispatcher());

    // Setup a fake logical link.
    fake_link_ = std::make_unique<hci::testing::FakeConnection>(1, hci::Connection::LinkType::kLE,
                                                                role, kLocalAddr, kPeerAddr);

    pairing_ = std::make_unique<PairingState>(fake_link_->WeakPtr(), fake_chan_, ioc,
                                              weak_ptr_factory_.GetWeakPtr());
  }

  void DestroyPairingState() { pairing_ = nullptr; }

  // Called by |pairing_| to obtain a Temporary Key during legacy pairing.
  void OnTemporaryKeyRequest(PairingMethod method,
                             PairingState::Delegate::TkResponse responder) override {
    if (tk_delegate_) {
      tk_delegate_(method, std::move(responder));
    } else {
      responder(true /* success */, 0);
    }
  }

  std::optional<IdentityInfo> OnIdentityInformationRequest() override {
    local_id_info_callback_count_++;
    return local_id_info_;
  }

  // Called by |pairing_| when the pairing procedure ends.
  void OnPairingComplete(Status status) override {
    pairing_complete_count_++;
    pairing_complete_status_ = status;
  }

  // Called by |pairing_| when a new LTK is obtained.
  void OnNewPairingData(const PairingData& pairing_data) override {
    pairing_data_callback_count_++;
    pairing_data_ = pairing_data;
  }

  // Called by |pairing_| when any encryption procedure fails.
  void OnAuthenticationFailure(hci::Status status) override {
    auth_failure_callback_count_++;
    auth_failure_status_ = status;
  }

  // Called by |pairing_| when the link security properties change.
  void OnNewSecurityProperties(const SecurityProperties& sec) override {
    new_sec_props_count_++;
    new_sec_props_ = sec;
  }

  void UpgradeSecurity(SecurityLevel level) {
    ZX_DEBUG_ASSERT(pairing_);
    pairing_->UpgradeSecurity(level, [this](auto status, const auto& props) {
      pairing_callback_count_++;
      pairing_status_ = status;
      sec_props_ = props;
    });
  }

  // Called when SMP sends a packet over the fake channel.
  void OnDataReceived(std::unique_ptr<const ByteBuffer> packet) {
    ZX_DEBUG_ASSERT(packet);

    PacketReader reader(packet.get());
    switch (reader.code()) {
      case kPairingFailed:
        pairing_failed_count_++;
        received_error_code_ = reader.payload<PairingFailedParams>();
        break;
      case kPairingRequest:
        pairing_request_count_++;
        packet->Copy(&local_pairing_cmd_);
        break;
      case kPairingResponse:
        pairing_response_count_++;
        packet->Copy(&local_pairing_cmd_);
        break;
      case kPairingConfirm:
        pairing_confirm_count_++;
        pairing_confirm_ = reader.payload<PairingConfirmValue>();
        break;
      case kPairingRandom:
        pairing_random_count_++;
        pairing_random_ = reader.payload<PairingRandomValue>();
        break;
      case kEncryptionInformation:
        enc_info_count_++;
        enc_info_ = reader.payload<EncryptionInformationParams>();
        break;
      case kMasterIdentification: {
        const auto& params = reader.payload<MasterIdentificationParams>();
        master_ident_count_++;
        ediv_ = le16toh(params.ediv);
        rand_ = le64toh(params.rand);
        break;
      }
      case kIdentityInformation:
        id_info_count_++;
        id_info_ = reader.payload<UInt128>();
        break;
      case kIdentityAddressInformation: {
        const auto& params = reader.payload<IdentityAddressInformationParams>();
        id_addr_info_count_++;
        id_addr_info_ = DeviceAddress(params.type == AddressType::kStaticRandom
                                          ? DeviceAddress::Type::kLERandom
                                          : DeviceAddress::Type::kLEPublic,
                                      params.bd_addr);
        break;
      }
      default:
        FAIL() << "Sent unsupported SMP command";
    }
  }

  // Emulates the receipt of pairing features (both as initiator and responder).
  void ReceivePairingFeatures(const PairingRequestParams& params, bool peer_initiator = false) {
    PacketWriter writer(peer_initiator ? kPairingRequest : kPairingResponse, &peer_pairing_cmd_);
    *writer.mutable_payload<PairingRequestParams>() = params;
    fake_chan()->Receive(peer_pairing_cmd_);
  }

  void ReceivePairingFeatures(IOCapability ioc = IOCapability::kNoInputNoOutput,
                              AuthReqField auth_req = 0,
                              uint8_t max_enc_key_size = kMaxEncryptionKeySize,
                              bool peer_initiator = false) {
    PairingRequestParams pairing_params;
    std::memset(&pairing_params, 0, sizeof(pairing_params));
    pairing_params.io_capability = ioc;
    pairing_params.auth_req = auth_req;
    pairing_params.max_encryption_key_size = max_enc_key_size;

    ReceivePairingFeatures(pairing_params, peer_initiator);
  }

  void ReceivePairingFailed(ErrorCode error_code) {
    StaticByteBuffer<sizeof(Header) + sizeof(ErrorCode)> buffer;
    PacketWriter writer(kPairingFailed, &buffer);
    *writer.mutable_payload<PairingFailedParams>() = error_code;
    fake_chan()->Receive(buffer);
  }

  void ReceivePairingConfirm(const UInt128& confirm) { Receive128BitCmd(kPairingConfirm, confirm); }

  void ReceivePairingRandom(const UInt128& random) { Receive128BitCmd(kPairingRandom, random); }

  void ReceiveEncryptionInformation(const UInt128& ltk) {
    Receive128BitCmd(kEncryptionInformation, ltk);
  }

  void ReceiveMasterIdentification(uint64_t random, uint16_t ediv) {
    StaticByteBuffer<sizeof(Header) + sizeof(MasterIdentificationParams)> buffer;
    PacketWriter writer(kMasterIdentification, &buffer);
    auto* params = writer.mutable_payload<MasterIdentificationParams>();
    params->ediv = htole16(ediv);
    params->rand = htole64(random);
    fake_chan()->Receive(buffer);
  }

  void ReceiveIdentityResolvingKey(const UInt128& irk) {
    Receive128BitCmd(kIdentityInformation, irk);
  }

  void ReceiveIdentityAddress(const DeviceAddress& address) {
    StaticByteBuffer<sizeof(Header) + sizeof(IdentityAddressInformationParams)> buffer;
    PacketWriter writer(kIdentityAddressInformation, &buffer);
    auto* params = writer.mutable_payload<IdentityAddressInformationParams>();
    params->type = address.type() == DeviceAddress::Type::kLEPublic ? AddressType::kPublic
                                                                    : AddressType::kStaticRandom;
    params->bd_addr = address.value();
    fake_chan()->Receive(buffer);
  }

  void ReceiveSecurityRequest(AuthReqField auth_req = 0u) {
    StaticByteBuffer<sizeof(Header) + sizeof(AuthReqField)> buffer;
    buffer[0] = kSecurityRequest;
    buffer[1] = auth_req;
    fake_chan()->Receive(buffer);
    RunLoopUntilIdle();
  }

  void GenerateConfirmValue(const UInt128& random, UInt128* out_value, bool peer_initiator = false,
                            uint32_t tk = 0) {
    ZX_DEBUG_ASSERT(out_value);

    tk = htole32(tk);
    UInt128 tk128;
    tk128.fill(0);
    std::memcpy(tk128.data(), &tk, sizeof(tk));

    const ByteBuffer *preq, *pres;
    const DeviceAddress *init_addr, *rsp_addr;
    if (peer_initiator) {
      preq = &peer_pairing_cmd();
      pres = &local_pairing_cmd();
      init_addr = &kPeerAddr;
      rsp_addr = &kLocalAddr;
    } else {
      preq = &local_pairing_cmd();
      pres = &peer_pairing_cmd();
      init_addr = &kLocalAddr;
      rsp_addr = &kPeerAddr;
    }

    util::C1(tk128, random, *preq, *pres, *init_addr, *rsp_addr, out_value);
  }

  PairingState* pairing() const { return pairing_.get(); }
  l2cap::testing::FakeChannel* fake_chan() const { return fake_chan_.get(); }
  hci::testing::FakeConnection* fake_link() const { return fake_link_.get(); }

  int pairing_callback_count() const { return pairing_callback_count_; }
  ErrorCode received_error_code() const { return received_error_code_; }
  const Status& pairing_status() const { return pairing_status_; }
  const SecurityProperties& sec_props() const { return sec_props_; }

  int pairing_complete_count() const { return pairing_complete_count_; }
  const Status& pairing_complete_status() const { return pairing_complete_status_; }

  int pairing_data_callback_count() const { return pairing_data_callback_count_; }

  int auth_failure_callback_count() const { return auth_failure_callback_count_; }
  const hci::Status& auth_failure_status() const { return auth_failure_status_; }

  int local_id_info_callback_count() const { return local_id_info_callback_count_; }

  int new_sec_props_count() const { return new_sec_props_count_; }
  const SecurityProperties& new_sec_props() const { return new_sec_props_; }

  const std::optional<LTK>& ltk() const { return pairing_data_.ltk; }
  const std::optional<Key>& irk() const { return pairing_data_.irk; }
  const std::optional<DeviceAddress>& identity() const { return pairing_data_.identity_address; }
  const std::optional<Key>& csrk() const { return pairing_data_.csrk; }

  using TkDelegate = fit::function<void(PairingMethod, PairingState::Delegate::TkResponse)>;
  void set_tk_delegate(TkDelegate delegate) { tk_delegate_ = std::move(delegate); }

  void set_local_id_info(std::optional<IdentityInfo> info) { local_id_info_ = info; }

  int pairing_failed_count() const { return pairing_failed_count_; }
  int pairing_request_count() const { return pairing_request_count_; }
  int pairing_response_count() const { return pairing_response_count_; }
  int pairing_confirm_count() const { return pairing_confirm_count_; }
  int pairing_random_count() const { return pairing_random_count_; }
  int enc_info_count() const { return enc_info_count_; }
  int id_info_count() const { return id_info_count_; }
  int id_addr_info_count() const { return id_addr_info_count_; }
  int master_ident_count() const { return master_ident_count_; }

  const UInt128& pairing_confirm() const { return pairing_confirm_; }
  const UInt128& pairing_random() const { return pairing_random_; }
  const UInt128& enc_info() const { return enc_info_; }
  const UInt128& id_info() const { return id_info_; }
  const DeviceAddress& id_addr_info() const { return id_addr_info_; }
  uint16_t ediv() const { return ediv_; }
  uint64_t rand() const { return rand_; }
  const PairingData& pairing_data() const { return pairing_data_; }

  const ByteBuffer& local_pairing_cmd() const { return local_pairing_cmd_; }
  const ByteBuffer& peer_pairing_cmd() const { return peer_pairing_cmd_; }

 private:
  void Receive128BitCmd(Code cmd_code, const UInt128& value) {
    StaticByteBuffer<sizeof(Header) + sizeof(UInt128)> buffer;
    PacketWriter writer(cmd_code, &buffer);
    *writer.mutable_payload<UInt128>() = value;
    fake_chan()->Receive(buffer);
  }

  // We store the preq/pres values here to generate a valid confirm value for
  // the fake side.
  StaticByteBuffer<sizeof(Header) + sizeof(PairingRequestParams)> local_pairing_cmd_,
      peer_pairing_cmd_;

  // Number of times the security callback given to UpgradeSecurity has been
  // called and the most recent parameters that it was called with.
  int pairing_callback_count_ = 0;
  Status pairing_status_;
  SecurityProperties sec_props_;

  // Number of times the pairing data callback has been called and the most
  // recent value that it was called with.
  int pairing_data_callback_count_ = 0;
  PairingData pairing_data_;

  // Number of times the link security properties have been notified via
  // OnNewSecurityProperties().
  int new_sec_props_count_ = 0;
  SecurityProperties new_sec_props_;

  // State tracking the OnPairingComplete event.
  int pairing_complete_count_ = 0;
  Status pairing_complete_status_;

  // State tracking the OnAuthenticationFailure event.
  int auth_failure_callback_count_ = 0;
  hci::Status auth_failure_status_;

  // State tracking the OnIdentityInformationRequest event.
  int local_id_info_callback_count_ = 0;
  std::optional<IdentityInfo> local_id_info_;

  // Callback used to notify when a call to OnTKRequest() is received.
  // OnTKRequest() will reply with 0 if a callback is not set.
  TkDelegate tk_delegate_;

  // Counts of commands that we have sent out to the peer.
  int pairing_failed_count_ = 0;
  int pairing_request_count_ = 0;
  int pairing_response_count_ = 0;
  int pairing_confirm_count_ = 0;
  int pairing_random_count_ = 0;
  int enc_info_count_ = 0;
  int id_info_count_ = 0;
  int id_addr_info_count_ = 0;
  int master_ident_count_ = 0;

  // Values that have we have sent to the peer.
  UInt128 pairing_confirm_;
  UInt128 pairing_random_;
  UInt128 enc_info_;
  UInt128 id_info_;
  DeviceAddress id_addr_info_;
  uint16_t ediv_;
  uint64_t rand_;

  ErrorCode received_error_code_ = ErrorCode::kNoError;

  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan_;
  std::unique_ptr<hci::testing::FakeConnection> fake_link_;
  std::unique_ptr<PairingState> pairing_;

  fxl::WeakPtrFactory<SMP_PairingStateTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_PairingStateTest);
};

class SMP_InitiatorPairingTest : public SMP_PairingStateTest {
 public:
  SMP_InitiatorPairingTest() = default;
  ~SMP_InitiatorPairingTest() override = default;

  void SetUp() override { SetUpPairingState(); }

  void SetUpPairingState(IOCapability ioc = IOCapability::kDisplayOnly) {
    NewPairingState(hci::Connection::Role::kMaster, ioc);
  }

  void GenerateMatchingConfirmAndRandom(UInt128* out_confirm, UInt128* out_random,
                                        uint32_t tk = 0) {
    ZX_DEBUG_ASSERT(out_confirm);
    ZX_DEBUG_ASSERT(out_random);
    *out_random = RandomUInt128();
    GenerateConfirmValue(*out_random, out_confirm, false /* peer_initiator */, tk);
  }

  // Emulate legacy pairing up until before encryption with STK. Returns the STK
  // that the master is expected to encrypt the link with in |out_stk|.
  //
  // This will not resolve the encryption request that is made by using the STK
  // before this function returns (this is to unit test encryption failure). Use
  // FastForwardToSTKEncrypted() to also emulate successful encryption.
  void FastForwardToSTK(UInt128* out_stk, SecurityLevel level = SecurityLevel::kEncrypted,
                        KeyDistGenField remote_keys = 0, KeyDistGenField local_keys = 0,
                        uint8_t max_key_size = kMaxEncryptionKeySize) {
    UpgradeSecurity(level);

    PairingRequestParams pairing_params;
    pairing_params.io_capability = IOCapability::kNoInputNoOutput;
    pairing_params.auth_req = 0;
    pairing_params.max_encryption_key_size = max_key_size;
    pairing_params.initiator_key_dist_gen = local_keys;
    pairing_params.responder_key_dist_gen = remote_keys;
    ReceivePairingFeatures(pairing_params);

    // Run the loop until the harness caches the feature exchange PDUs (preq &
    // pres) so that we can generate a valid confirm value.
    RunLoopUntilIdle();

    UInt128 sconfirm, srand;
    GenerateMatchingConfirmAndRandom(&sconfirm, &srand);
    ReceivePairingConfirm(sconfirm);
    ReceivePairingRandom(srand);
    RunLoopUntilIdle();

    EXPECT_EQ(1, pairing_confirm_count());
    EXPECT_EQ(1, pairing_random_count());
    EXPECT_EQ(0, pairing_failed_count());
    EXPECT_EQ(0, pairing_callback_count());

    ZX_DEBUG_ASSERT(out_stk);

    UInt128 tk;
    tk.fill(0);
    util::S1(tk, srand, pairing_random(), out_stk);
  }

  void FastForwardToSTKEncrypted(UInt128* out_stk, SecurityLevel level = SecurityLevel::kEncrypted,
                                 KeyDistGenField remote_keys = 0, KeyDistGenField local_keys = 0,
                                 uint8_t max_key_size = kMaxEncryptionKeySize) {
    FastForwardToSTK(out_stk, level, remote_keys, local_keys, max_key_size);

    ASSERT_TRUE(fake_link()->ltk());
    EXPECT_EQ(1, fake_link()->start_encryption_count());

    // Resolve the encryption request.
    fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
    RunLoopUntilIdle();
    EXPECT_EQ(1, new_sec_props_count());
    EXPECT_EQ(level, new_sec_props().level());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_InitiatorPairingTest);
};

class SMP_ResponderPairingTest : public SMP_PairingStateTest {
 public:
  SMP_ResponderPairingTest() = default;
  ~SMP_ResponderPairingTest() override = default;

  void SetUp() override { SetUpPairingState(); }

  void SetUpPairingState(IOCapability ioc = IOCapability::kDisplayOnly) {
    NewPairingState(hci::Connection::Role::kSlave, ioc);
  }

  void GenerateMatchingConfirmAndRandom(UInt128* out_confirm, UInt128* out_random,
                                        uint32_t tk = 0) {
    ZX_DEBUG_ASSERT(out_confirm);
    ZX_DEBUG_ASSERT(out_random);
    zx_cprng_draw(out_random->data(), out_random->size());
    GenerateConfirmValue(*out_random, out_confirm, true /* peer_initiator */, tk);
  }

  void ReceivePairingRequest(IOCapability ioc = IOCapability::kNoInputNoOutput,
                             AuthReqField auth_req = 0,
                             uint8_t max_enc_key_size = kMaxEncryptionKeySize) {
    ReceivePairingFeatures(ioc, auth_req, max_enc_key_size, true /* peer_initiator */);
  }

  void FastForwardToSTK(UInt128* out_stk, SecurityLevel level = SecurityLevel::kEncrypted,
                        KeyDistGenField remote_keys = 0, KeyDistGenField local_keys = 0,
                        uint8_t max_key_size = kMaxEncryptionKeySize) {
    PairingRequestParams pairing_params;
    pairing_params.io_capability = IOCapability::kNoInputNoOutput;
    pairing_params.auth_req = 0;
    pairing_params.max_encryption_key_size = max_key_size;
    pairing_params.initiator_key_dist_gen = remote_keys;
    pairing_params.responder_key_dist_gen = local_keys;
    ReceivePairingFeatures(pairing_params, true /* peer_initiator */);

    // Run the loop until the harness caches the feature exchange PDUs (preq &
    // pres) so that we can generate a valid confirm value.
    RunLoopUntilIdle();
    EXPECT_EQ(0, pairing_request_count());
    EXPECT_EQ(1, pairing_response_count());

    // The master initiates the Phase 2 keys.
    EXPECT_EQ(0, pairing_confirm_count());
    EXPECT_EQ(0, pairing_random_count());

    UInt128 mconfirm, mrand;
    GenerateMatchingConfirmAndRandom(&mconfirm, &mrand);
    ReceivePairingConfirm(mconfirm);
    RunLoopUntilIdle();
    EXPECT_EQ(1, pairing_confirm_count());
    EXPECT_EQ(0, pairing_random_count());

    ReceivePairingRandom(mrand);
    RunLoopUntilIdle();
    EXPECT_EQ(1, pairing_confirm_count());
    EXPECT_EQ(1, pairing_random_count());
    EXPECT_EQ(0, pairing_failed_count());
    EXPECT_EQ(0, pairing_callback_count());

    ZX_DEBUG_ASSERT(out_stk);

    UInt128 tk;
    tk.fill(0);
    util::S1(tk, pairing_random(), mrand, out_stk);
  }

  void FastForwardToSTKEncrypted(UInt128* out_stk, SecurityLevel level = SecurityLevel::kEncrypted,
                                 KeyDistGenField remote_keys = 0, KeyDistGenField local_keys = 0,
                                 uint8_t max_key_size = kMaxEncryptionKeySize) {
    FastForwardToSTK(out_stk, level, remote_keys, local_keys, max_key_size);

    ASSERT_TRUE(fake_link()->ltk());

    // No local start encryption request should be made.
    EXPECT_EQ(0, fake_link()->start_encryption_count());

    // Pretend that the master succeeded in encrypting the connection.
    fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
    RunLoopUntilIdle();
    EXPECT_EQ(1, new_sec_props_count());
    EXPECT_EQ(level, new_sec_props().level());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SMP_ResponderPairingTest);
};

// Requesting pairing at the current security level should succeed immediately.
TEST_F(SMP_InitiatorPairingTest, UpgradeSecurityCurrentLevel) {
  UpgradeSecurity(SecurityLevel::kNoSecurity);
  RunLoopUntilIdle();

  // No pairing requests should have been made.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Pairing should succeed.
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(SecurityLevel::kNoSecurity, sec_props().level());
  EXPECT_EQ(0u, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());
}

// Peer aborts during Phase 1.
TEST_F(SMP_InitiatorPairingTest, PairingFailedInPhase1) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pairing not complete yet but we should be in Phase 1.
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());

  ReceivePairingFailed(ErrorCode::kPairingNotSupported);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());
  EXPECT_EQ(ErrorCode::kPairingNotSupported, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// Local aborts during Phase 1.
TEST_F(SMP_InitiatorPairingTest, PairingAbortedInPhase1) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pairing not complete yet but we should be in Phase 1.
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());

  pairing()->Abort();
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// Local resets I/O capabilities while pairing. This should abort any ongoing
// pairing and the new I/O capabilities should be used in following pairing
// requests.
TEST_F(SMP_InitiatorPairingTest, PairingStateResetDuringPairing) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pairing not complete yet but we should be in Phase 1.
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());

  pairing()->Reset(IOCapability::kNoInputNoOutput);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_request_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Should have sent a new pairing request.
  EXPECT_EQ(2, pairing_request_count());

  // Make sure that the new request has the new I/O capabilities.
  const auto& params = local_pairing_cmd().view(1).As<PairingRequestParams>();
  EXPECT_EQ(IOCapability::kNoInputNoOutput, params.io_capability);
}

TEST_F(SMP_InitiatorPairingTest, ReceiveConfirmValueWhileNotPairing) {
  UInt128 confirm;
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Nothing should happen.
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
}

TEST_F(SMP_InitiatorPairingTest, ReceiveConfirmValueInPhase1) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  UInt128 confirm;
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// In Phase 2 but still waiting to receive TK.
TEST_F(SMP_InitiatorPairingTest, ReceiveConfirmValueWhileWaitingForTK) {
  bool tk_requested = false;
  set_tk_delegate([&](auto, auto) { tk_requested = true; });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_TRUE(tk_requested);

  UInt128 confirm;
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// PairingState destroyed when TKResponse runs.
TEST_F(SMP_InitiatorPairingTest, PairingStateDestroyedStateWhileWaitingForTK) {
  PairingState::Delegate::TkResponse respond;
  set_tk_delegate([&](auto, auto rsp) { respond = std::move(rsp); });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_TRUE(respond);

  DestroyPairingState();

  // This should proceed safely.
  respond(true, 0);
  RunLoopUntilIdle();
}

// Pairing no longer in progress when TKResponse runs.
TEST_F(SMP_InitiatorPairingTest, PairingAbortedWhileWaitingForTK) {
  PairingState::Delegate::TkResponse respond;
  set_tk_delegate([&](auto, auto rsp) { respond = std::move(rsp); });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_TRUE(respond);

  ReceivePairingFailed(ErrorCode::kPairingNotSupported);
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kPairingNotSupported, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // This should have no effect.
  respond(true, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_request_count());
  EXPECT_EQ(0, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(0, pairing_data_callback_count());
}

// Pairing procedure stopped and restarted when TKResponse runs. The TKResponse
// does not belong to the current pairing.
TEST_F(SMP_InitiatorPairingTest, PairingRestartedWhileWaitingForTK) {
  PairingState::Delegate::TkResponse respond;
  set_tk_delegate([&](auto, auto rsp) { respond = std::move(rsp); });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_TRUE(respond);

  // Stop pairing.
  ReceivePairingFailed(ErrorCode::kPairingNotSupported);
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kPairingNotSupported, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // Reset the delegate so that |respond| doesn't get overwritten by the second
  // pairing.
  set_tk_delegate(nullptr);

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_EQ(2, pairing_request_count());
  EXPECT_EQ(0, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  // This should have no effect.
  respond(true, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(2, pairing_request_count());
  EXPECT_EQ(0, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(0, pairing_data_callback_count());
}

TEST_F(SMP_InitiatorPairingTest, ReceiveRandomValueWhileNotPairing) {
  UInt128 random;
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  // Nothing should happen.
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
}

TEST_F(SMP_InitiatorPairingTest, ReceiveRandomValueInPhase1) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  UInt128 random;
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// In Phase 2 but still waiting to receive TK.
TEST_F(SMP_InitiatorPairingTest, ReceiveRandomValueWhileWaitingForTK) {
  bool tk_requested = false;
  set_tk_delegate([&](auto, auto) { tk_requested = true; });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();
  EXPECT_TRUE(tk_requested);

  UInt128 random;
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, LegacyPhase2SconfirmValueReceivedTwice) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  ReceivePairingFeatures();
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  UInt128 confirm;
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Should have received Mrand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Send Mconfirm again
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, LegacyPhase2ReceiveRandomValueInWrongOrder) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  ReceivePairingFeatures();
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  UInt128 random;
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  // Should have aborted pairing if Srand arrives before Srand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, LegacyPhase2SconfirmValueInvalid) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pick I/O capabilities and MITM flags that will result in Just Works
  // pairing.
  ReceivePairingFeatures();
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Receive Sconfirm and Srand values that don't match.
  UInt128 confirm, random;
  confirm.fill(0);
  random.fill(1);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Should have received Mrand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Our Mconfirm/Mrand should be correct.
  UInt128 expected_confirm;
  GenerateConfirmValue(pairing_random(), &expected_confirm);
  EXPECT_EQ(expected_confirm, pairing_confirm());

  // Send the non-matching Srandom.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kConfirmValueFailed, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, LegacyPhase2RandomValueReceivedTwice) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pick I/O capabilities and MITM flags that will result in Just Works
  // pairing.
  ReceivePairingFeatures();
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Receive Sconfirm and Srand values that match.
  UInt128 confirm, random;
  GenerateMatchingConfirmAndRandom(&confirm, &random);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Should have received Mrand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Our Mconfirm/Mrand should be correct.
  UInt128 expected_confirm;
  GenerateConfirmValue(pairing_random(), &expected_confirm);
  EXPECT_EQ(expected_confirm, pairing_confirm());

  // Send Srandom.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Send Srandom again.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());

  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, LegacyPhase2ConfirmValuesExchanged) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pick I/O capabilities and MITM flags that will result in Just Works
  // pairing.
  ReceivePairingFeatures();
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Receive Sconfirm and Srand values that match.
  UInt128 confirm, random;
  GenerateMatchingConfirmAndRandom(&confirm, &random);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Should have received Mrand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Our Mconfirm/Mrand should be correct.
  UInt128 expected_confirm;
  GenerateConfirmValue(pairing_random(), &expected_confirm);
  EXPECT_EQ(expected_confirm, pairing_confirm());

  // Send Srandom.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
}

// TK delegate rejects pairing. When pairing method is "PasskeyEntryInput", this
// should result in a "Passkey Entry Failed" error.
TEST_F(SMP_InitiatorPairingTest, LegacyPhase2TKDelegateRejectsPasskeyInput) {
  SetUpPairingState(IOCapability::kKeyboardOnly);

  bool tk_requested = false;
  PairingState::Delegate::TkResponse respond;
  PairingMethod method = PairingMethod::kJustWorks;
  set_tk_delegate([&](auto cb_method, auto cb_rsp) {
    tk_requested = true;
    method = cb_method;
    respond = std::move(cb_rsp);
  });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pick I/O capabilities and MITM flags that will result in Passkey Entry
  // pairing.
  ReceivePairingFeatures(IOCapability::kDisplayOnly, AuthReq::kMITM);
  RunLoopUntilIdle();
  ASSERT_TRUE(tk_requested);
  EXPECT_EQ(PairingMethod::kPasskeyEntryInput, method);

  // Reject pairing.
  respond(false, 0);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kPasskeyEntryFailed, pairing_status().protocol_error());
}

// TK delegate rejects pairing.
TEST_F(SMP_InitiatorPairingTest, LegacyPhase2TKDelegateRejectsPairing) {
  bool tk_requested = false;
  PairingState::Delegate::TkResponse respond;
  PairingMethod method = PairingMethod::kPasskeyEntryDisplay;
  set_tk_delegate([&](auto cb_method, auto cb_rsp) {
    tk_requested = true;
    method = cb_method;
    respond = std::move(cb_rsp);
  });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  ReceivePairingFeatures();
  RunLoopUntilIdle();
  ASSERT_TRUE(tk_requested);
  EXPECT_EQ(PairingMethod::kJustWorks, method);

  // Reject pairing.
  respond(false, 0);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
}

// The TK delegate is called with the correct pairing method and the TK is
// factored into the confirm value generation.
TEST_F(SMP_InitiatorPairingTest, LegacyPhase2ConfirmValuesExchangedWithUserTK) {
  constexpr uint32_t kTK = 123456;

  bool tk_requested = false;
  PairingState::Delegate::TkResponse respond;
  PairingMethod method = PairingMethod::kJustWorks;
  set_tk_delegate([&](auto cb_method, auto cb_rsp) {
    tk_requested = true;
    method = cb_method;
    respond = std::move(cb_rsp);
  });

  UpgradeSecurity(SecurityLevel::kEncrypted);
  RunLoopUntilIdle();

  // Pick I/O capabilities and MITM flags that will result in Passkey Entry
  // pairing.
  ReceivePairingFeatures(IOCapability::kKeyboardOnly, AuthReq::kMITM);
  RunLoopUntilIdle();
  ASSERT_TRUE(tk_requested);

  // Local is DisplayOnly and peer is KeyboardOnly. Local displays passkey.
  EXPECT_EQ(PairingMethod::kPasskeyEntryDisplay, method);

  // Send TK.
  respond(true, kTK);
  RunLoopUntilIdle();

  // Should have received Mconfirm.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Receive Sconfirm and Srand values that match.
  UInt128 confirm, random;
  GenerateMatchingConfirmAndRandom(&confirm, &random, kTK);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Should have received Mrand.
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Our Mconfirm/Mrand should be correct.
  UInt128 expected_confirm;
  GenerateConfirmValue(pairing_random(), &expected_confirm, false /* peer_initiator */, kTK);
  EXPECT_EQ(expected_confirm, pairing_confirm());

  // Send Srandom.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
}

// Peer aborts during Phase 2.
TEST_F(SMP_InitiatorPairingTest, PairingFailedInPhase2) {
  UpgradeSecurity(SecurityLevel::kEncrypted);
  ReceivePairingFeatures();
  RunLoopUntilIdle();

  UInt128 confirm, random;
  GenerateMatchingConfirmAndRandom(&confirm, &random);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  ReceivePairingFailed(ErrorCode::kConfirmValueFailed);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(ErrorCode::kConfirmValueFailed, pairing_status().protocol_error());
}

// Encryption with STK fails.
TEST_F(SMP_InitiatorPairingTest, EncryptionWithSTKFails) {
  UInt128 stk;
  FastForwardToSTK(&stk);

  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(stk, fake_link()->ltk()->value());
  EXPECT_EQ(0u, fake_link()->ltk()->ediv());
  EXPECT_EQ(0u, fake_link()->ltk()->rand());

  // The host should have requested encryption.
  EXPECT_EQ(SecurityProperties(), pairing()->security());
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  fake_link()->TriggerEncryptionChangeCallback(hci::Status(hci::StatusCode::kPinOrKeyMissing),
                                               false /* enabled */);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, auth_failure_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(hci::StatusCode::kPinOrKeyMissing, auth_failure_status().protocol_error());

  // No security property update should have been sent since the security
  // properties have not changed.
  EXPECT_EQ(0, new_sec_props_count());
  EXPECT_EQ(SecurityProperties(), pairing()->security());
}

TEST_F(SMP_InitiatorPairingTest, EncryptionDisabledInPhase2) {
  UInt128 stk;
  FastForwardToSTK(&stk);

  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(stk, fake_link()->ltk()->value());
  EXPECT_EQ(0u, fake_link()->ltk()->ediv());
  EXPECT_EQ(0u, fake_link()->ltk()->rand());

  // The host should have requested encryption.
  EXPECT_EQ(1, fake_link()->start_encryption_count());
  EXPECT_EQ(SecurityProperties(), pairing()->security());

  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), false /* enabled */);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(0, auth_failure_callback_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());

  // No security property update should have been sent since the security
  // properties have not changed.
  EXPECT_EQ(0, new_sec_props_count());
  EXPECT_EQ(SecurityProperties(), pairing()->security());
}

// Tests that the STK is generated according to the max length provided
TEST_F(SMP_InitiatorPairingTest, StkLengthGeneration) {
  UInt128 stk;
  uint8_t max_key_size = 10;
  FastForwardToSTK(&stk, SecurityLevel::kEncrypted, 0, 0, max_key_size);

  // At this stage, the stk is stored here
  ASSERT_TRUE(fake_link()->ltk());

  // Ensure that most significant (16 - max_key_size) bytes are zero. The key
  // should be generated up to the max_key_size.
  for (auto i = max_key_size; i < fake_link()->ltk()->value().size(); i++) {
    EXPECT_TRUE(fake_link()->ltk()->value()[i] == 0);
  }
}

// Tests that the pairing procedure ends after encryption with the STK if there
// are no keys to distribute.
TEST_F(SMP_InitiatorPairingTest, Phase3CompleteWithoutKeyExchange) {
  UInt128 stk;
  FastForwardToSTK(&stk);

  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(stk, fake_link()->ltk()->value());
  EXPECT_EQ(0u, fake_link()->ltk()->ediv());
  EXPECT_EQ(0u, fake_link()->ltk()->rand());

  // The host should have requested encryption.
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();

  // Pairing should succeed without any pairing data.
  EXPECT_EQ(1, pairing_data_callback_count());
  EXPECT_FALSE(ltk());
  EXPECT_FALSE(irk());
  EXPECT_FALSE(identity());
  EXPECT_FALSE(csrk());

  // Should have been called at least once to determine local identity
  // availability.
  EXPECT_NE(0, local_id_info_callback_count());

  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  EXPECT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  EXPECT_EQ(16u, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());

  // The security properties should have been updated to match the STK.
  EXPECT_EQ(1, new_sec_props_count());
  EXPECT_EQ(sec_props(), pairing()->security());

  ASSERT_TRUE(fake_link()->ltk());
}

TEST_F(SMP_InitiatorPairingTest, Phase3EncryptionInformationReceivedTwice) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  ReceiveEncryptionInformation(UInt128());
  RunLoopUntilIdle();

  // Waiting for EDIV and Rand
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Send the LTK twice. This should cause pairing to fail.
  ReceiveEncryptionInformation(UInt128());
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// The responder sends EDIV and Rand before LTK.
TEST_F(SMP_InitiatorPairingTest, Phase3MasterIdentificationReceivedInWrongOrder) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  // Send master identification before encryption information. This should cause
  // pairing to fail.
  ReceiveMasterIdentification(1, 2);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// The responder sends the sample LTK from the specification doc
TEST_F(SMP_InitiatorPairingTest, Phase3MasterIdentificationReceiveSampleLTK) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  const UInt128 kLtkSample{{0xBF, 0x01, 0xFB, 0x9D, 0x4E, 0xF3, 0xBC, 0x36, 0xD8, 0x74, 0xF5, 0x39,
                            0x41, 0x38, 0x68, 0x4C}};

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  // Send a bad LTK, this should cause pairing to fail.
  ReceiveEncryptionInformation(kLtkSample);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// The responder sends the sample Rand from the specification doc
TEST_F(SMP_InitiatorPairingTest, Phase3MasterIdentificationReceiveExampleRand) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  uint64_t kRandSample = 0xABCDEF1234567890;
  uint16_t kEDiv = 20;

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  // Send a bad Rand, this should cause pairing to fail.
  ReceiveEncryptionInformation(UInt128());
  ReceiveMasterIdentification(kRandSample, kEDiv);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// The responder sends an LTK that is longer than the max key size
TEST_F(SMP_InitiatorPairingTest, Phase3MasterIdentificationReceiveLongLTK) {
  UInt128 stk;
  auto max_key_size = 8;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey, 0, max_key_size);

  const UInt128 kLtk{{1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8}};

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  // Send a long LTK, this should cause pairing to fail.
  ReceiveEncryptionInformation(kLtk);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, Phase3MasterIdentificationReceivedTwice) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  constexpr uint16_t kEdiv = 1;
  constexpr uint64_t kRand = 2;
  constexpr uint16_t kDupEdiv = 3;
  constexpr uint64_t kDupRand = 4;

  // Send duplicate master identification. Pairing should complete with the
  // first set of information. The second set should get ignored.
  ReceiveEncryptionInformation(UInt128());
  ReceiveMasterIdentification(kRand, kEdiv);
  ReceiveMasterIdentification(kDupRand, kDupEdiv);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(1, pairing_data_callback_count());
  EXPECT_TRUE(pairing_status().is_success());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
  ASSERT_TRUE(pairing_data().ltk);
  EXPECT_EQ(kEdiv, pairing_data().ltk->key().ediv());
  EXPECT_EQ(kRand, pairing_data().ltk->key().rand());
}

// Pairing completes after obtaining encryption information only.
TEST_F(SMP_InitiatorPairingTest, Phase3CompleteWithEncKey) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);

  const UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  uint64_t kRand = 5;
  uint16_t kEDiv = 20;

  ReceiveEncryptionInformation(kLTK);
  ReceiveMasterIdentification(kRand, kEDiv);
  RunLoopUntilIdle();

  // Pairing should have succeeded.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // LTK should have been assigned to the link.
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(kLTK, fake_link()->ltk()->value());
  EXPECT_EQ(kRand, fake_link()->ltk()->rand());
  EXPECT_EQ(kEDiv, fake_link()->ltk()->ediv());

  // We don't re-encrypt with the LTK while the link is already authenticated
  // with the STK.
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  EXPECT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  EXPECT_EQ(16u, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());

  // Should have been called at least once to determine local identity
  // availability.
  EXPECT_NE(0, local_id_info_callback_count());

  // Local identity information should not have been distributed by us since it
  // isn't available.
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());

  // Should have notified the LTK.
  EXPECT_EQ(1, pairing_data_callback_count());
  ASSERT_TRUE(ltk());
  ASSERT_FALSE(irk());
  ASSERT_FALSE(identity());
  ASSERT_FALSE(csrk());
  EXPECT_EQ(sec_props(), ltk()->security());
  EXPECT_EQ(kLTK, ltk()->key().value());
  EXPECT_EQ(kRand, ltk()->key().rand());
  EXPECT_EQ(kEDiv, ltk()->key().ediv());

  // No security property update should have been sent for the LTK. This is
  // because the LTK and the STK are expected to have the same properties.
  EXPECT_EQ(1, new_sec_props_count());
}

// Pairing completes after obtaining short encryption information only.
TEST_F(SMP_InitiatorPairingTest, Phase3CompleteWithShortEncKey) {
  UInt128 stk;
  uint8_t max_key_size = 12;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey, 0u, max_key_size);

  // This LTK is within the max_key_size specified above.
  const UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 0, 0, 0, 0}};
  uint64_t kRand = 5;
  uint16_t kEDiv = 20;

  ReceiveEncryptionInformation(kLTK);
  ReceiveMasterIdentification(kRand, kEDiv);
  RunLoopUntilIdle();

  // Pairing should have succeeded.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // LTK should have been assigned to the link.
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(kLTK, fake_link()->ltk()->value());
  EXPECT_EQ(kRand, fake_link()->ltk()->rand());
  EXPECT_EQ(kEDiv, fake_link()->ltk()->ediv());

  // We don't re-encrypt with the LTK while the link is already authenticated
  // with the STK.
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  EXPECT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  EXPECT_EQ(max_key_size, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());

  // Should have been called at least once to determine local identity
  // availability.
  EXPECT_NE(0, local_id_info_callback_count());

  // Local identity information should not have been distributed by us since it
  // isn't available.
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());

  // Should have notified the LTK.
  EXPECT_EQ(1, pairing_data_callback_count());
  ASSERT_TRUE(ltk());
  ASSERT_FALSE(irk());
  ASSERT_FALSE(identity());
  ASSERT_FALSE(csrk());
  EXPECT_EQ(sec_props(), ltk()->security());
  EXPECT_EQ(kLTK, ltk()->key().value());
  EXPECT_EQ(kRand, ltk()->key().rand());
  EXPECT_EQ(kEDiv, ltk()->key().ediv());

  // No security property update should have been sent for the LTK. This is
  // because the LTK and the STK are expected to have the same properties.
  EXPECT_EQ(1, new_sec_props_count());
}

TEST_F(SMP_InitiatorPairingTest, Phase3WithLocalIdKey) {
  IdentityInfo local_id_info;
  local_id_info.irk = UInt128{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  local_id_info.address = kLocalAddr;
  set_local_id_info(local_id_info);

  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            0,                    // remote keys
                            KeyDistGen::kIdKey);  // local keys

  // Local identity information should have been sent.
  EXPECT_EQ(1, id_info_count());
  EXPECT_EQ(local_id_info.irk, id_info());
  EXPECT_EQ(1, id_addr_info_count());
  EXPECT_EQ(local_id_info.address, id_addr_info());

  // Pairing should succeed without notifying any keys.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// Tests that pairing results in an error if a local ID key was initially
// negotiated but gets removed before the distribution phase.
TEST_F(SMP_InitiatorPairingTest, Phase3IsAbortedIfLocalIdKeyIsRemoved) {
  IdentityInfo local_id_info;
  local_id_info.irk = UInt128{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  local_id_info.address = kLocalAddr;
  set_local_id_info(local_id_info);

  UInt128 stk;
  FastForwardToSTK(&stk, SecurityLevel::kEncrypted,
                   0,                    // remote keys
                   KeyDistGen::kIdKey);  // local keys

  // Local identity information should not have been sent yet.
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());

  // Pairing still in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Remove the local identity information.
  set_local_id_info(std::nullopt);

  // Encrypt with the STK to finish phase 2.
  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();

  // Pairing should have been aborted.
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_FALSE(pairing_status());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
}

TEST_F(SMP_InitiatorPairingTest, Phase3IRKReceivedTwice) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kIdKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());

  ReceiveIdentityResolvingKey(UInt128());
  RunLoopUntilIdle();

  // Waiting for identity address.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Send an IRK again. This should cause pairing to fail.
  ReceiveIdentityResolvingKey(UInt128());
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// The responder sends its identity address before sending its IRK.
TEST_F(SMP_InitiatorPairingTest, Phase3IdentityAddressReceivedInWrongOrder) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kIdKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Send identity address before the IRK. This should cause pairing to fail.
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

TEST_F(SMP_InitiatorPairingTest, Phase3IdentityAddressReceivedTwice) {
  UInt128 stk;
  // Request enc key to prevent pairing from completing after sending the first
  // identity address.
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            KeyDistGen::kEncKey | KeyDistGen::kIdKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  ReceiveIdentityResolvingKey(UInt128());
  ReceiveIdentityAddress(kPeerAddr);
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_status().protocol_error());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(pairing_status(), pairing_complete_status());
}

// Pairing completes after obtaining identity information only.
TEST_F(SMP_InitiatorPairingTest, Phase3CompleteWithIdKey) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kIdKey);

  // Pairing should still be in progress.
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_data_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  const UInt128 kIRK{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};

  ReceiveIdentityResolvingKey(kIRK);
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // The link remains encrypted with the STK.
  EXPECT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  EXPECT_EQ(16u, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());

  EXPECT_EQ(1, pairing_data_callback_count());
  ASSERT_FALSE(ltk());
  ASSERT_TRUE(irk());
  ASSERT_TRUE(identity());
  ASSERT_FALSE(csrk());

  EXPECT_EQ(sec_props(), irk()->security());
  EXPECT_EQ(kIRK, irk()->value());
  EXPECT_EQ(kPeerAddr, *identity());
}

TEST_F(SMP_InitiatorPairingTest, Phase3CompleteWithAllKeys) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            KeyDistGen::kEncKey | KeyDistGen::kIdKey);

  const UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  const UInt128 kIRK{{8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4, 5, 6, 7, 8}};
  uint64_t kRand = 5;
  uint16_t kEDiv = 20;

  // The link should be assigned the STK as its link key.
  EXPECT_EQ(stk, fake_link()->ltk()->value());

  // Receive EncKey
  ReceiveEncryptionInformation(kLTK);
  ReceiveMasterIdentification(kRand, kEDiv);
  RunLoopUntilIdle();

  // Pairing still pending. SMP should have assigned the LTK to the link without
  // requesting to re-encrypt with the LTK (i.e. the link should remain
  // encrypted with the STK until pairing is complete).
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());
  EXPECT_TRUE(fake_link()->ltk());
  EXPECT_EQ(kLTK, fake_link()->ltk()->value());
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  // Receive IdKey
  ReceiveIdentityResolvingKey(kIRK);
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  // Pairing should have succeeded
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(1, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_TRUE(pairing_status());
  EXPECT_EQ(pairing_status(), pairing_complete_status());

  // LTK should have been assigned to the link.
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(kLTK, fake_link()->ltk()->value());
  EXPECT_EQ(kRand, fake_link()->ltk()->rand());
  EXPECT_EQ(kEDiv, fake_link()->ltk()->ediv());

  // We don't re-encrypt with the LTK while the link is already authenticated
  // with the STK.
  EXPECT_EQ(1, fake_link()->start_encryption_count());

  EXPECT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  EXPECT_EQ(16u, sec_props().enc_key_size());
  EXPECT_FALSE(sec_props().secure_connections());

  // Should have notified the LTK.
  EXPECT_EQ(1, pairing_data_callback_count());
  ASSERT_TRUE(ltk());
  ASSERT_TRUE(irk());
  ASSERT_TRUE(identity());
  ASSERT_FALSE(csrk());
  EXPECT_EQ(sec_props(), ltk()->security());
  EXPECT_EQ(kLTK, ltk()->key().value());
  EXPECT_EQ(kRand, ltk()->key().rand());
  EXPECT_EQ(kEDiv, ltk()->key().ediv());
  EXPECT_EQ(sec_props(), irk()->security());
  EXPECT_EQ(kIRK, irk()->value());
  EXPECT_EQ(kPeerAddr, *identity());
}

TEST_F(SMP_InitiatorPairingTest, AssignLongTermKeyFailsDuringPairing) {
  UpgradeSecurity(SecurityLevel::kEncrypted);  // Initiate pairing.
  SecurityProperties sec_props(SecurityLevel::kAuthenticated, 16, false);
  EXPECT_FALSE(pairing()->AssignLongTermKey(LTK(sec_props, hci::LinkKey())));
  EXPECT_EQ(0, fake_link()->start_encryption_count());
  EXPECT_EQ(SecurityLevel::kNoSecurity, pairing()->security().level());
}

TEST_F(SMP_InitiatorPairingTest, AssignLongTermKey) {
  SecurityProperties sec_props(SecurityLevel::kAuthenticated, 16, false);
  LTK ltk(sec_props, hci::LinkKey());

  EXPECT_TRUE(pairing()->AssignLongTermKey(ltk));
  EXPECT_EQ(1, fake_link()->start_encryption_count());
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(ltk.key(), *fake_link()->ltk());

  // The link security level is not assigned until successful encryption.
  EXPECT_EQ(SecurityProperties(), pairing()->security());
  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();

  EXPECT_EQ(1, new_sec_props_count());
  EXPECT_EQ(sec_props, new_sec_props());
  EXPECT_EQ(sec_props, pairing()->security());
}

TEST_F(SMP_InitiatorPairingTest, ReceiveSecurityRequest) {
  ReceiveSecurityRequest(AuthReq::kMITM);
  RunLoopUntilIdle();

  // Should have requested pairing with MITM protection.
  EXPECT_EQ(1, pairing_request_count());
  const auto& params = local_pairing_cmd().view(1).As<PairingRequestParams>();
  EXPECT_TRUE(params.auth_req & AuthReq::kMITM);
}

TEST_F(SMP_InitiatorPairingTest, ReceiveSecurityRequestWhenPaired) {
  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted, KeyDistGen::kEncKey);
  EXPECT_EQ(stk, fake_link()->ltk()->value());
  EXPECT_EQ(1, pairing_request_count());

  // Receiving a security request now should have no effect since pairing is
  // still in progress.
  ReceiveSecurityRequest();
  EXPECT_EQ(1, pairing_request_count());

  // Receive EncKey and wait until the link is encrypted with the LTK.
  UInt128 kLTK{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  uint64_t kRand = 5;
  uint16_t kEDiv = 20;
  ReceiveEncryptionInformation(kLTK);
  ReceiveMasterIdentification(kRand, kEDiv);
  RunLoopUntilIdle();
  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();
  ASSERT_EQ(1, pairing_complete_count());
  ASSERT_TRUE(pairing_status());
  ASSERT_TRUE(ltk());
  ASSERT_EQ(SecurityLevel::kEncrypted, sec_props().level());
  ASSERT_EQ(1, fake_link()->start_encryption_count());  // Once for the STK
  ASSERT_EQ(1, pairing_request_count());

  // Receive a security request with the no MITM requirement. This should
  // trigger an encryption key refresh and no pairing request.
  ReceiveSecurityRequest();
  EXPECT_EQ(1, pairing_request_count());

  // Once for the STK and once again due to the locally initiated key refresh.
  ASSERT_EQ(2, fake_link()->start_encryption_count());

  // Receive a security request with a higher security requirement. This should
  // trigger a pairing request.
  ReceiveSecurityRequest(AuthReq::kMITM);
  EXPECT_EQ(2, pairing_request_count());
  const auto& params = local_pairing_cmd().view(1).As<PairingRequestParams>();
  EXPECT_TRUE(params.auth_req & AuthReq::kMITM);
}

TEST_F(SMP_ResponderPairingTest, ReceiveSecondPairingRequestWhilePairing) {
  ReceivePairingRequest();
  RunLoopUntilIdle();

  // We should have sent a pairing response and should now be in Phase 2,
  // waiting for the peer to send us Mconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // This should cause pairing to be aborted.
  ReceivePairingRequest();
  RunLoopUntilIdle();
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(2, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(received_error_code(), pairing_complete_status().protocol_error());
}

TEST_F(SMP_ResponderPairingTest, ReceiveConfirmValueWhileWaitingForTK) {
  bool tk_requested = false;
  PairingState::Delegate::TkResponse respond;
  set_tk_delegate([&](auto, auto cb) {
    tk_requested = true;
    respond = std::move(cb);
  });

  ReceivePairingRequest();
  RunLoopUntilIdle();
  ASSERT_TRUE(tk_requested);

  UInt128 confirm;
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // Pairing should still be in progress without sending out any packets.
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Respond with the TK. This should cause us to send Sconfirm.
  respond(true, 0);
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase2ReceivePairingRandomInWrongOrder) {
  ReceivePairingRequest();
  RunLoopUntilIdle();

  // We should have sent a pairing response and should now be in Phase 2,
  // waiting for the peer to send us Mconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Peer sends Mrand before Mconfirm.
  UInt128 random;
  ReceivePairingRandom(random);
  RunLoopUntilIdle();
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, received_error_code());
  EXPECT_EQ(ErrorCode::kUnspecifiedReason, pairing_complete_status().protocol_error());
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase2MconfirmValueInvalid) {
  ReceivePairingRequest();
  RunLoopUntilIdle();

  // We should have sent a pairing response and should now be in Phase 2,
  // waiting for the peer to send us Mconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Set up values that don't match.
  UInt128 confirm, random;
  confirm.fill(0);
  random.fill(1);

  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // We should have sent Sconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Peer sends Mrand that doesn't match. We should reject the pairing
  // without sending Srand.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(1, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(1, pairing_complete_count());
  EXPECT_EQ(ErrorCode::kConfirmValueFailed, received_error_code());
  EXPECT_EQ(ErrorCode::kConfirmValueFailed, pairing_complete_status().protocol_error());
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase2ConfirmValuesExchanged) {
  ReceivePairingRequest();
  RunLoopUntilIdle();

  // We should have sent a pairing response and should now be in Phase 2,
  // waiting for the peer to send us Mconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(0, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Set up Sconfirm and Srand values that match.
  UInt128 confirm, random;
  GenerateMatchingConfirmAndRandom(&confirm, &random);

  // Peer sends Mconfirm.
  ReceivePairingConfirm(confirm);
  RunLoopUntilIdle();

  // We should have sent Sconfirm.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(0, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Peer sends Mrand.
  ReceivePairingRandom(random);
  RunLoopUntilIdle();

  // We should have sent Srand.
  EXPECT_EQ(0, pairing_request_count());
  EXPECT_EQ(1, pairing_response_count());
  EXPECT_EQ(1, pairing_confirm_count());
  EXPECT_EQ(1, pairing_random_count());
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());
  EXPECT_EQ(0, pairing_complete_count());

  // Sconfirm/Srand values we sent should be correct.
  UInt128 expected_confirm;
  GenerateConfirmValue(pairing_random(), &expected_confirm, true /* peer_initiator */);
  EXPECT_EQ(expected_confirm, pairing_confirm());
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase3LocalLTKDistributionNoRemoteKeys) {
  EXPECT_EQ(0, enc_info_count());
  EXPECT_EQ(0, master_ident_count());

  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            0u,                    // remote keys
                            KeyDistGen::kEncKey);  // local keys

  // Local LTK, EDiv, and Rand should be sent to the peer.
  EXPECT_EQ(1, enc_info_count());
  EXPECT_EQ(1, master_ident_count());
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(enc_info(), fake_link()->ltk()->value());
  EXPECT_EQ(ediv(), fake_link()->ltk()->ediv());
  EXPECT_EQ(rand(), fake_link()->ltk()->rand());

  // This LTK should be stored with the pairing data but the pairing callback
  // shouldn't be called because pairing wasn't initiated by UpgradeSecurity().
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Pairing is considered complete when all keys have been distributed even if
  // we're still encrypted with the STK. This is because the initiator may not
  // always re-encrypt the link with the LTK until a reconnection.
  EXPECT_EQ(1, pairing_data_callback_count());

  // Nonetheless the link should have been assigned the LTK.
  ASSERT_TRUE(pairing_data().ltk);
  EXPECT_EQ(fake_link()->ltk(), pairing_data().ltk->key());

  // Make sure that an encryption change has no effect.
  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();
  EXPECT_EQ(1, pairing_data_callback_count());

  // No additional security property update should have been sent since the STK
  // and LTK have the same properties.
  EXPECT_EQ(1, new_sec_props_count());
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase3LocalLTKDistributionWithRemoteKeys) {
  EXPECT_EQ(0, enc_info_count());
  EXPECT_EQ(0, master_ident_count());

  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            KeyDistGen::kIdKey,    // remote keys
                            KeyDistGen::kEncKey);  // local keys

  // Local LTK, EDiv, and Rand should be sent to the peer.
  EXPECT_EQ(1, enc_info_count());
  EXPECT_EQ(1, master_ident_count());
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(enc_info(), fake_link()->ltk()->value());
  EXPECT_EQ(ediv(), fake_link()->ltk()->ediv());
  EXPECT_EQ(rand(), fake_link()->ltk()->rand());

  // No local identity information should have been sent.
  EXPECT_EQ(0, id_info_count());
  EXPECT_EQ(0, id_addr_info_count());

  // This LTK should be stored with the pairing data but the pairing callback
  // shouldn't be called because pairing wasn't initiated by UpgradeSecurity().
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Still waiting for initiator's keys.
  EXPECT_EQ(0, pairing_data_callback_count());

  const auto kIrk = RandomUInt128();
  ReceiveIdentityResolvingKey(kIrk);
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  // Pairing is considered complete when all keys have been distributed even if
  // we're still encrypted with the STK. This is because the initiator may not
  // always re-encrypt the link with the LTK until a reconnection.
  EXPECT_EQ(1, pairing_data_callback_count());

  // The peer should have sent us its identity information.
  ASSERT_TRUE(pairing_data().irk);
  EXPECT_EQ(kIrk, pairing_data().irk->value());
  ASSERT_TRUE(pairing_data().identity_address);
  EXPECT_EQ(kPeerAddr, *pairing_data().identity_address);

  // Nonetheless the link should have been assigned the LTK.
  ASSERT_TRUE(pairing_data().ltk);
  EXPECT_EQ(fake_link()->ltk(), pairing_data().ltk->key());
}

// Locally generated ltk length should match max key length specified
TEST_F(SMP_ResponderPairingTest, LegacyPhase3LocalLTKMaxLength) {
  EXPECT_EQ(0, enc_info_count());
  EXPECT_EQ(0, master_ident_count());

  UInt128 stk;
  uint16_t max_key_size = 7;

  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            0u,                   // remote keys
                            KeyDistGen::kEncKey,  // local keys
                            max_key_size);

  // Local LTK, EDiv, and Rand should be sent to the peer.
  EXPECT_EQ(1, enc_info_count());
  EXPECT_EQ(1, master_ident_count());
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(enc_info(), fake_link()->ltk()->value());
  EXPECT_EQ(ediv(), fake_link()->ltk()->ediv());
  EXPECT_EQ(rand(), fake_link()->ltk()->rand());

  // This LTK should be stored with the pairing data but the pairing callback
  // shouldn't be called because pairing wasn't initiated by UpgradeSecurity().
  EXPECT_EQ(0, pairing_failed_count());
  EXPECT_EQ(0, pairing_callback_count());

  // Pairing is considered complete when all keys have been distributed even if
  // we're still encrypted with the STK. This is because the initiator may not
  // always re-encrypt the link with the LTK until a reconnection.
  EXPECT_EQ(1, pairing_data_callback_count());

  // The link should have been assigned the LTK.
  ASSERT_TRUE(pairing_data().ltk);
  EXPECT_EQ(fake_link()->ltk(), pairing_data().ltk->key());

  // Ensure that most significant (16 - max_key_size) bytes are zero. The key
  // should be generated up to the max_key_size.
  auto ltk = pairing_data().ltk->key().value();
  for (auto i = max_key_size; i < ltk.size(); i++) {
    EXPECT_TRUE(ltk[i] == 0);
  }
}

TEST_F(SMP_ResponderPairingTest, LegacyPhase3LocalIdKeyDistributionWithRemoteKeys) {
  IdentityInfo local_id_info;
  local_id_info.irk = UInt128{{1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
  local_id_info.address = kLocalAddr;
  set_local_id_info(local_id_info);

  EXPECT_EQ(0, enc_info_count());
  EXPECT_EQ(0, master_ident_count());

  UInt128 stk;
  FastForwardToSTKEncrypted(&stk, SecurityLevel::kEncrypted,
                            KeyDistGen::kIdKey,   // remote keys
                            KeyDistGen::kIdKey);  // local keys

  // No local LTK, EDiv, and Rand should be sent to the peer.
  EXPECT_EQ(0, enc_info_count());
  EXPECT_EQ(0, master_ident_count());

  // Local identity information should have been sent.
  EXPECT_EQ(1, id_info_count());
  EXPECT_EQ(local_id_info.irk, id_info());
  EXPECT_EQ(1, id_addr_info_count());
  EXPECT_EQ(local_id_info.address, id_addr_info());

  // Still waiting for master's keys.
  EXPECT_EQ(0, pairing_data_callback_count());

  const auto kIrk = RandomUInt128();
  ReceiveIdentityResolvingKey(kIrk);
  ReceiveIdentityAddress(kPeerAddr);
  RunLoopUntilIdle();

  // Pairing is considered complete when all keys have been distributed even if
  // we're still encrypted with the STK. This is because the master may not
  // always re-encrypt the link with the LTK until a reconnection.
  EXPECT_EQ(1, pairing_data_callback_count());

  // The peer should have sent us its identity information.
  ASSERT_TRUE(pairing_data().irk);
  EXPECT_EQ(kIrk, pairing_data().irk->value());
  ASSERT_TRUE(pairing_data().identity_address);
  EXPECT_EQ(kPeerAddr, *pairing_data().identity_address);
}

TEST_F(SMP_ResponderPairingTest, AssignLongTermKeyFailsDuringPairing) {
  ReceivePairingRequest();
  RunLoopUntilIdle();
  SecurityProperties sec_props(SecurityLevel::kAuthenticated, 16, false);
  EXPECT_FALSE(pairing()->AssignLongTermKey(LTK(sec_props, hci::LinkKey())));
  EXPECT_EQ(0, fake_link()->start_encryption_count());
  EXPECT_EQ(SecurityLevel::kNoSecurity, pairing()->security().level());
}

TEST_F(SMP_ResponderPairingTest, AssignLongTermKey) {
  SecurityProperties sec_props(SecurityLevel::kAuthenticated, 16, false);
  LTK ltk(sec_props, hci::LinkKey());

  EXPECT_TRUE(pairing()->AssignLongTermKey(ltk));
  ASSERT_TRUE(fake_link()->ltk());
  EXPECT_EQ(ltk.key(), *fake_link()->ltk());

  // No encryption request should have been made as the initiator is expected to
  // do it.
  EXPECT_EQ(0, fake_link()->start_encryption_count());

  // The link security level is not assigned until successful encryption.
  EXPECT_EQ(SecurityProperties(), pairing()->security());
  fake_link()->TriggerEncryptionChangeCallback(hci::Status(), true /* enabled */);
  RunLoopUntilIdle();

  EXPECT_EQ(1, new_sec_props_count());
  EXPECT_EQ(sec_props, new_sec_props());
  EXPECT_EQ(sec_props, pairing()->security());
}

TEST_F(SMP_ResponderPairingTest, ReceiveSecurityRequest) {
  ReceiveSecurityRequest();
  EXPECT_EQ(ErrorCode::kCommandNotSupported, received_error_code());
}

}  // namespace
}  // namespace sm
}  // namespace bt
