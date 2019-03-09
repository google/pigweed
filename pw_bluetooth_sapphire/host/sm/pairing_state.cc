// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_state.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"

#include "util.h"

namespace btlib {

using common::ByteBuffer;
using common::DeviceAddress;
using common::HostError;
using common::MutableBufferView;
using common::UInt128;

namespace sm {

namespace {

SecurityProperties FeaturesToProperties(const PairingFeatures& features) {
  return SecurityProperties(features.method == PairingMethod::kJustWorks
                                ? SecurityLevel::kEncrypted
                                : SecurityLevel::kAuthenticated,
                            features.encryption_key_size,
                            features.secure_connections);
}

}  // namespace

PairingState::LegacyState::LegacyState(uint64_t id)
    : id(id),
      stk_encrypted(false),
      ltk_encrypted(false),
      obtained_remote_keys(0u),
      sent_local_keys(false),
      has_tk(false),
      has_peer_confirm(false),
      has_peer_rand(false),
      sent_local_confirm(false),
      sent_local_rand(false),
      has_ltk(false),
      has_irk(false) {}

bool PairingState::LegacyState::InPhase1() const {
  return !features && !stk_encrypted;
}

bool PairingState::LegacyState::InPhase2() const {
  return features && has_tk && !stk_encrypted;
}

bool PairingState::LegacyState::InPhase3() const {
  return features && stk_encrypted && !KeyExchangeComplete();
}

bool PairingState::LegacyState::IsComplete() const {
  return features && stk_encrypted && KeyExchangeComplete() &&
         !WaitingForEncryptionWithLTK();
}

bool PairingState::LegacyState::WaitingForTK() const {
  return features && !has_tk && !stk_encrypted;
}

bool PairingState::LegacyState::RequestedKeysObtained() const {
  ZX_DEBUG_ASSERT(features);

  // Return true if we expect no keys from the remote.
  return !features->remote_key_distribution ||
         (features->remote_key_distribution == obtained_remote_keys);
}

bool PairingState::LegacyState::LocalKeysSent() const {
  ZX_DEBUG_ASSERT(features);

  // Return true if we didn't agree to send any keys.
  return !features->local_key_distribution || sent_local_keys;
}

bool PairingState::LegacyState::ShouldReceiveLTK() const {
  ZX_DEBUG_ASSERT(features);
  return (features->remote_key_distribution & KeyDistGen::kEncKey);
}

bool PairingState::LegacyState::ShouldReceiveIdentity() const {
  ZX_DEBUG_ASSERT(features);
  return (features->remote_key_distribution & KeyDistGen::kIdKey);
}

bool PairingState::LegacyState::ShouldSendLTK() const {
  ZX_DEBUG_ASSERT(features);
  return (features->local_key_distribution & KeyDistGen::kEncKey);
}

bool PairingState::LegacyState::ShouldSendIdentity() const {
  ZX_DEBUG_ASSERT(features);
  return (features->local_key_distribution & KeyDistGen::kIdKey);
}

bool PairingState::LegacyState::WaitingForEncryptionWithLTK() const {
  // When we are the responder we consider the pairing to be done after all keys
  // have been exchanged and do not wait for the master to encrypt with the LTK.
  // This is because some LE central implementations leave the link encrypted
  // with the STK until a re-connection.
  return features->initiator && (ShouldReceiveLTK() || ShouldSendLTK()) &&
         !ltk_encrypted;
}

PairingState::PendingRequest::PendingRequest(SecurityLevel level,
                                             PairingCallback callback)
    : level(level), callback(std::move(callback)) {}

PairingState::PairingState(fxl::WeakPtr<hci::Connection> link,
                           fbl::RefPtr<l2cap::Channel> smp,
                           IOCapability io_capability,
                           fxl::WeakPtr<Delegate> delegate)
    : next_pairing_id_(0),
      delegate_(delegate),
      le_link_(link),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(delegate_);
  ZX_DEBUG_ASSERT(le_link_);
  ZX_DEBUG_ASSERT(smp);
  ZX_DEBUG_ASSERT(link->handle() == smp->link_handle());
  ZX_DEBUG_ASSERT(link->ll_type() == hci::Connection::LinkType::kLE);
  ZX_DEBUG_ASSERT(smp->id() == l2cap::kLESMPChannelId);

  // Set up SMP data bearer.
  // TODO(armansito): Enable SC when we support it.
  le_smp_ =
      std::make_unique<Bearer>(std::move(smp), link->role(), false /* sc */,
                               io_capability, weak_ptr_factory_.GetWeakPtr());

  // Set up HCI encryption event.
  le_link_->set_encryption_change_callback(
      fit::bind_member(this, &PairingState::OnEncryptionChange));
}

PairingState::~PairingState() {
  if (le_link_) {
    le_link_->set_encryption_change_callback({});
  }
}

void PairingState::Reset(IOCapability io_capability) {
  Abort();
  le_smp_->set_io_capability(io_capability);
}

bool PairingState::AssignLongTermKey(const LTK& ltk) {
  if (legacy_state_) {
    bt_log(TRACE, "sm",
           "Cannot directly assign LTK while pairing is in progress");
    return false;
  }

  AssignLongTermKeyInternal(ltk);

  // Try to initiate encryption if we are the master.
  if (le_link_->role() == hci::Connection::Role::kMaster &&
      !le_link_->StartEncryption()) {
    bt_log(ERROR, "sm", "Failed to initiate authentication procedure");
    return false;
  }

  return true;
}

void PairingState::UpgradeSecurity(SecurityLevel level,
                                   PairingCallback callback) {
  // If pairing is in progress then we queue the request.
  if (legacy_state_) {
    bt_log(SPEW, "sm", "LE legacy pairing in progress; request queued");
    ZX_DEBUG_ASSERT(le_smp_->pairing_started());
    request_queue_.emplace(level, std::move(callback));
    return;
  }

  if (level <= le_sec_.level()) {
    callback(Status(), le_sec_);
    return;
  }

  // TODO(armansito): Support initiating a security upgrade as slave (Bearer
  // needs to support the SMP Security Request).
  if (le_smp_->role() == hci::Connection::Role::kSlave) {
    callback(Status(HostError::kNotSupported), SecurityProperties());
    return;
  }

  request_queue_.emplace(level, std::move(callback));
  BeginLegacyPairingPhase1(level);
}

void PairingState::SetSecurityProperties(const SecurityProperties& sec) {
  if (sec != le_sec_) {
    bt_log(TRACE, "sm",
           "security properties changed - handle: %#.4x, new: %s, old: %s",
           le_link_->handle(), sec.ToString().c_str(),
           le_sec_.ToString().c_str());
    le_sec_ = sec;
    delegate_->OnNewSecurityProperties(le_sec_);
  }
}

void PairingState::AbortLegacyPairing(ErrorCode error_code) {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(le_smp_->pairing_started());

  le_smp_->Abort(error_code);

  // "Abort" should trigger OnPairingFailed.
}

void PairingState::BeginLegacyPairingPhase1(SecurityLevel level) {
  ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kMaster);
  ZX_DEBUG_ASSERT_MSG(!legacy_state_, "already pairing!");

  if (level == SecurityLevel::kAuthenticated) {
    le_smp_->set_mitm_required(true);
  }

  legacy_state_ = std::make_unique<LegacyState>(next_pairing_id_++);
  le_smp_->InitiateFeatureExchange();
}

void PairingState::Abort() {
  if (!le_smp_->pairing_started())
    return;

  bt_log(TRACE, "sm", "abort pairing");
  if (legacy_state_) {
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
  }
}

void PairingState::BeginLegacyPairingPhase2(const ByteBuffer& preq,
                                            const ByteBuffer& pres) {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->WaitingForTK());
  ZX_DEBUG_ASSERT(!legacy_state_->features->secure_connections);
  ZX_DEBUG_ASSERT(!legacy_state_->has_tk);
  ZX_DEBUG_ASSERT(!legacy_state_->has_peer_confirm);
  ZX_DEBUG_ASSERT(!legacy_state_->has_peer_rand);
  ZX_DEBUG_ASSERT(!legacy_state_->sent_local_confirm);
  ZX_DEBUG_ASSERT(!legacy_state_->sent_local_rand);
  // Cache |preq| and |pres|. These are used for confirm value generation.
  ZX_DEBUG_ASSERT(preq.size() == legacy_state_->preq.size());
  ZX_DEBUG_ASSERT(pres.size() == legacy_state_->pres.size());
  preq.Copy(&legacy_state_->preq);
  pres.Copy(&legacy_state_->pres);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto tk_callback = [self, id = legacy_state_->id](bool success, uint32_t tk) {
    if (!self) {
      return;
    }

    auto* state = self->legacy_state_.get();
    if (!state || id != state->id) {
      bt_log(TRACE, "sm", "ignoring TK callback for expired pairing: (id = %u)",
             id);
      return;
    }

    if (!success) {
      bt_log(TRACE, "sm", "TK delegate responded with error; aborting");
      if (state->features->method == PairingMethod::kPasskeyEntryInput) {
        self->AbortLegacyPairing(ErrorCode::kPasskeyEntryFailed);
      } else {
        self->AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
      }
      return;
    }

    ZX_DEBUG_ASSERT(state->WaitingForTK());

    // Set the lower bits to |tk|.
    tk = htole32(tk);
    state->tk.fill(0);
    std::memcpy(state->tk.data(), &tk, sizeof(tk));
    state->has_tk = true;

    ZX_DEBUG_ASSERT(state->InPhase2());

    // We have TK so we can generate the confirm value now.
    const DeviceAddress *ia, *ra;
    self->LEPairingAddresses(&ia, &ra);
    state->local_rand = common::RandomUInt128();
    util::C1(state->tk, state->local_rand, state->preq, state->pres, *ia, *ra,
             &state->local_confirm);

    // If we are the initiator then we just generated the "Mconfirm" value. We
    // start the exchange by sending this value to the peer. Otherwise this is
    // the "Sconfirm" value and we either:
    //    a. send it now if the peer has sent us its confirm value while we were
    //    waiting for the TK.
    //    b. send it later when we receive Mconfirm.
    if (state->features->initiator || state->has_peer_confirm) {
      self->LegacySendConfirmValue();
    }
  };

  ZX_DEBUG_ASSERT(delegate_);
  delegate_->OnTemporaryKeyRequest(legacy_state_->features->method,
                                   std::move(tk_callback));
}

void PairingState::LegacySendConfirmValue() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->InPhase2());
  ZX_DEBUG_ASSERT(!legacy_state_->sent_local_confirm);

  legacy_state_->sent_local_confirm = true;
  le_smp_->SendConfirmValue(legacy_state_->local_confirm);
}

void PairingState::LegacySendRandomValue() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->InPhase2());
  ZX_DEBUG_ASSERT(!legacy_state_->sent_local_rand);

  legacy_state_->sent_local_rand = true;
  le_smp_->SendRandomValue(legacy_state_->local_rand);
}

void PairingState::EndLegacyPairingPhase2() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->InPhase2());

  // Update the current security level. Even though the link is encrypted with
  // the STK (i.e. a temporary key) it provides a level of security.
  legacy_state_->stk_encrypted = true;
  SetSecurityProperties(FeaturesToProperties(*legacy_state_->features));

  if (legacy_state_->InPhase3()) {
    if (legacy_state_->features->initiator &&
        !legacy_state_->RequestedKeysObtained()) {
      ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kMaster);
      bt_log(TRACE, "sm", "waiting to receive keys from the responder");
      return;
    }

    // TODO(armansito): Add a test case for distributing the local keys here as
    // the initiator. We currently only distribute the LTK and we only do so
    // when we are the responder.
    if (!legacy_state_->LocalKeysSent()) {
      SendLocalKeys();
    }
  }

  // If there are no keys left to exchange then we're done with pairing. Since
  // we're only encrypted with the STK, the pairing will be short-term (this is
  // the case if the "bonding" flag was not set).
  if (legacy_state_->IsComplete()) {
    CompleteLegacyPairing();
  }
}

void PairingState::SendLocalKeys() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->InPhase3());
  ZX_DEBUG_ASSERT(!legacy_state_->LocalKeysSent());

  if (legacy_state_->ShouldSendLTK()) {
    // Generate completely random values for LTK, EDiv, and Rand.
    hci::LinkKey key(common::RandomUInt128(), common::Random<uint64_t>(),
                     common::Random<uint16_t>());

    // Assign the link key to make it available when the master initiates
    // encryption. The security properties of the LTK are based on the current
    // properties under which it gets exchanged.
    AssignLongTermKeyInternal(LTK(le_sec_, key));
    le_smp_->SendEncryptionKey(key);
  }

  legacy_state_->sent_local_keys = true;
}

void PairingState::CompleteLegacyPairing() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->IsComplete());
  ZX_DEBUG_ASSERT(le_smp_->pairing_started());

  le_smp_->StopTimer();

  // The security properties of all keys are determined by the security
  // properties of the link used to distribute them. This is already reflected
  // by |le_sec_|.

  PairingData pairing_data;
  if (ltk_) {
    pairing_data.ltk = *ltk_;
  }

  if (legacy_state_->has_irk) {
    // If there is an IRK there must also be an identity address.
    pairing_data.irk = Key(le_sec_, legacy_state_->irk);
    pairing_data.identity_address = legacy_state_->identity_address;
  }

  bt_log(TRACE, "sm", "LE legacy pairing complete");
  legacy_state_ = nullptr;

  // TODO(armansito): Report CSRK when we support it.
  ZX_DEBUG_ASSERT(delegate_);
  delegate_->OnPairingComplete(Status());
  delegate_->OnNewPairingData(pairing_data);

  // Separate out the requests that are satisfied by the current security level
  // from the ones that require a higher level. We'll retry pairing for the
  // latter.
  std::queue<PendingRequest> satisfied;
  std::queue<PendingRequest> unsatisfied;
  while (!request_queue_.empty()) {
    auto& request = request_queue_.front();
    if (request.level <= le_sec_.level()) {
      satisfied.push(std::move(request));
    } else {
      unsatisfied.push(std::move(request));
    }
    request_queue_.pop();
  }

  request_queue_ = std::move(unsatisfied);

  // Notify the satisfied requests with success.
  while (!satisfied.empty()) {
    satisfied.front().callback(Status(), le_sec_);
    satisfied.pop();
  }

  if (!unsatisfied.empty()) {
    BeginLegacyPairingPhase1(unsatisfied.front().level);
  }
}

void PairingState::OnPairingFailed(Status status) {
  bt_log(ERROR, "sm", "LE pairing failed: %s", status.ToString().c_str());

  // TODO(NET-1201): implement "waiting interval" to prevent repeated attempts
  // as described in Vol 3, Part H, 2.3.6.

  ZX_DEBUG_ASSERT(delegate_);
  delegate_->OnPairingComplete(status);

  auto requests = std::move(request_queue_);
  while (!requests.empty()) {
    requests.front().callback(status, le_sec_);
    requests.pop();
  }

  if (legacy_state_) {
    ZX_DEBUG_ASSERT(le_link_);
    le_link_->set_link_key(hci::LinkKey());
    legacy_state_ = nullptr;
  }
}

void PairingState::OnFeatureExchange(const PairingFeatures& features,
                                     const ByteBuffer& preq,
                                     const ByteBuffer& pres) {
  bt_log(SPEW, "sm", "obtained LE Pairing features");

  if (!features.initiator) {
    if (legacy_state_) {
      ZX_DEBUG_ASSERT(legacy_state_->features);

      // Reject if the peer sent a new pairing request while pairing is already
      // in progress.
      AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
      return;
    }

    legacy_state_ = std::make_unique<LegacyState>(next_pairing_id_++);
  }

  ZX_DEBUG_ASSERT(legacy_state_);
  legacy_state_->features = features;
  BeginLegacyPairingPhase2(preq, pres);
}

void PairingState::OnPairingConfirm(const UInt128& confirm) {
  // TODO(armansito): Have separate subroutines to handle this event for legacy
  // and secure connections.
  if (!legacy_state_) {
    bt_log(TRACE, "sm", "ignoring confirm value received while not pairing");
    return;
  }

  // Allow this command if:
  //    a. we are in Phase 2, or
  //    b. we are the responder but still waiting for a TK.
  // Reject pairing if neither of these is true.
  if (!legacy_state_->InPhase2() &&
      (!legacy_state_->WaitingForTK() || legacy_state_->features->initiator)) {
    bt_log(ERROR, "sm",
           "abort pairing due to confirm value received outside phase 2");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // abort pairing if we received a second confirm value from the peer. The
  // specification defines a certain order for the phase 2 commands.
  if (legacy_state_->has_peer_confirm) {
    bt_log(ERROR, "sm", "already received confirm value! aborting");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The confirm value shouldn't be sent after the random value. (See Vol 3,
  // Part H, 2.3.5.5 and Appendix C.2.1.1 for the specific order of events.
  if (legacy_state_->has_peer_rand) {
    bt_log(ERROR, "sm",
           "\"Pairing Confirm\" expected before \"Pairing Random\"");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  legacy_state_->peer_confirm = confirm;
  legacy_state_->has_peer_confirm = true;

  if (legacy_state_->features->initiator) {
    // We MUST have a TK and have previously generated an Mconfirm.
    ZX_DEBUG_ASSERT(legacy_state_->has_tk);

    // We are the master and have previously sent Mconfirm and just received
    // Sconfirm. We now send Mrand for the slave to compare.
    ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kMaster);
    LegacySendRandomValue();
  } else {
    // We are the slave and have just received Mconfirm.
    ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kSlave);

    if (!legacy_state_->WaitingForTK()) {
      LegacySendConfirmValue();
    }
  }
}

void PairingState::OnPairingRandom(const UInt128& random) {
  // TODO(armansito): Have separate subroutines to handle this event for legacy
  // and secure connections.
  if (!legacy_state_) {
    bt_log(TRACE, "sm", "ignoring confirm value received while not pairing");
    return;
  }

  if (!legacy_state_->InPhase2()) {
    bt_log(ERROR, "sm",
           "abort pairing due to confirm value received outside phase 2");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // We must have a TK and sent a confirm value by now (this is implied by
  // InPhase2() above).
  ZX_DEBUG_ASSERT(legacy_state_->has_tk);

  // abort pairing if we received a second random value from the peer. The
  // specification defines a certain order for the phase 2 commands.
  if (legacy_state_->has_peer_rand) {
    bt_log(ERROR, "sm", "already received random value! aborting");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The random value shouldn't be sent before the confirm value. (See Vol 3,
  // Part H, 2.3.5.5 and Appendix C.2.1.1 for the specific order of events.
  if (!legacy_state_->has_peer_confirm) {
    bt_log(ERROR, "sm", "\"Pairing Rand\" expected after \"Pairing Confirm\"");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Check that the order of the SMP commands is correct.
  if (legacy_state_->features->initiator) {
    ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kMaster);

    // The master distributes both values before the slave sends Srandom.
    if (!legacy_state_->sent_local_rand || !legacy_state_->sent_local_confirm) {
      bt_log(ERROR, "sm", "\"Pairing Random\" received in wrong order!");
      AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
      return;
    }
  } else {
    ZX_DEBUG_ASSERT(le_smp_->role() == hci::Connection::Role::kSlave);

    // We cannot have sent the Srand without receiving Mrand first.
    ZX_DEBUG_ASSERT(!legacy_state_->sent_local_rand);

    // We need to send Sconfirm before the master sends Mrand.
    if (!legacy_state_->sent_local_confirm) {
      bt_log(ERROR, "sm", "\"Pairing Random\" received in wrong order!");
      AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
      return;
    }
  }

  legacy_state_->peer_rand = random;
  legacy_state_->has_peer_rand = true;

  // We have both confirm and rand values from the peer. Generate it locally and
  // compare.
  const DeviceAddress *ia, *ra;
  LEPairingAddresses(&ia, &ra);
  UInt128 peer_confirm;
  util::C1(legacy_state_->tk, legacy_state_->peer_rand, legacy_state_->preq,
           legacy_state_->pres, *ia, *ra, &peer_confirm);
  if (peer_confirm != legacy_state_->peer_confirm) {
    bt_log(ERROR, "sm", "%sconfirm value does not match!",
           legacy_state_->features->initiator ? "S" : "M");
    AbortLegacyPairing(ErrorCode::kConfirmValueFailed);
    return;
  }

  ZX_DEBUG_ASSERT(le_link_);

  // Generate the STK.
  UInt128 stk;
  UInt128* initiator_rand = &legacy_state_->local_rand;
  UInt128* responder_rand = &legacy_state_->peer_rand;
  if (!legacy_state_->features->initiator) {
    std::swap(initiator_rand, responder_rand);
  }
  util::S1(legacy_state_->tk, *responder_rand, *initiator_rand, &stk);

  // Mask the key based on the requested encryption key size.
  uint8_t key_size = legacy_state_->features->encryption_key_size;
  if (key_size < 16) {
    MutableBufferView view(stk.data() + key_size, 16 - key_size);
    view.SetToZeros();
  }

  // EDiv and Rand values are set to 0 to generate the STK (Vol 3, Part H,
  // 2.4.4.1).
  le_link_->set_link_key(hci::LinkKey(stk, 0, 0));

  if (legacy_state_->features->initiator) {
    // Initiate link layer encryption with STK.
    if (!le_link_->StartEncryption()) {
      bt_log(ERROR, "sm", "failed to start encryption");
      AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    }
  } else {
    // Send Srand and wait for the master to encrypt the link with the STK.
    // |le_link_| will respond to the LE LTK request event with the STK that it
    // got assigned above.
    LegacySendRandomValue();
  }
}

void PairingState::OnLongTermKey(const common::UInt128& ltk) {
  if (!legacy_state_) {
    bt_log(TRACE, "sm", "ignoring LTK received while not in legacy pairing");
    return;
  }

  if (!legacy_state_->InPhase3()) {
    // The link MUST be encrypted with the STK for the transfer of the LTK to be
    // secure.
    bt_log(ERROR, "sm", "abort pairing due to LTK received outside phase 3");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (!legacy_state_->ShouldReceiveLTK()) {
    bt_log(ERROR, "sm", "received unexpected LTK");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // abort pairing if we received a second LTK from the peer.
  if (legacy_state_->has_ltk) {
    bt_log(ERROR, "sm", "already received LTK! aborting");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  ZX_DEBUG_ASSERT(!(legacy_state_->obtained_remote_keys & KeyDistGen::kEncKey));
  legacy_state_->ltk_bytes = ltk;
  legacy_state_->has_ltk = true;

  // Wait to receive EDiv and Rand
}

void PairingState::OnMasterIdentification(uint16_t ediv, uint64_t random) {
  if (!legacy_state_) {
    bt_log(TRACE, "sm",
           "ignoring ediv/rand received while not in legacy pairing");
    return;
  }

  if (!legacy_state_->InPhase3()) {
    // The link MUST be encrypted with the STK for the transfer of the LTK to be
    // secure.
    bt_log(ERROR, "sm",
           "abort pairing due to ediv/rand received outside phase 3");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  ZX_DEBUG_ASSERT(legacy_state_->stk_encrypted);

  if (!legacy_state_->ShouldReceiveLTK()) {
    bt_log(ERROR, "sm", "received unexpected ediv/rand");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // EDIV and Rand must be sent AFTER the LTK (Vol 3, Part H, 3.6.1).
  if (!legacy_state_->has_ltk) {
    bt_log(ERROR, "sm", "received EDIV and Rand before LTK!");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (legacy_state_->obtained_remote_keys & KeyDistGen::kEncKey) {
    bt_log(ERROR, "sm", "already received EDIV and Rand!");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The security properties of the LTK are determined by the current link
  // properties (i.e. the properties of the STK).
  AssignLongTermKeyInternal(
      LTK(le_sec_, hci::LinkKey(legacy_state_->ltk_bytes, random, ediv)));
  legacy_state_->obtained_remote_keys |= KeyDistGen::kEncKey;

  // "EncKey" received. Complete pairing if possible.
  OnExpectedKeyReceived();
}

void PairingState::OnIdentityResolvingKey(const common::UInt128& irk) {
  if (!legacy_state_) {
    bt_log(TRACE, "sm", "ignoring IRK received while not in legacy pairing!");
    return;
  }

  if (!legacy_state_->InPhase3()) {
    // The link must be encrypted with the STK for the transfer of the IRK to be
    // secure.
    bt_log(ERROR, "sm", "abort pairing due to IRK received outside phase 3");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (!legacy_state_->ShouldReceiveIdentity()) {
    bt_log(ERROR, "sm", "received unexpected IRK");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Abort if we receive an IRK more than once.
  if (legacy_state_->has_irk) {
    bt_log(ERROR, "sm", "already received IRK! aborting");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  ZX_DEBUG_ASSERT(!(legacy_state_->obtained_remote_keys & KeyDistGen::kIdKey));
  legacy_state_->irk = irk;
  legacy_state_->has_irk = true;

  // Wait to receive identity address
}

void PairingState::OnIdentityAddress(
    const common::DeviceAddress& identity_address) {
  if (!legacy_state_) {
    bt_log(TRACE, "sm",
           "ignoring identity address received while not in legacy pairing");
    return;
  }

  if (!legacy_state_->InPhase3()) {
    // The link must be encrypted with the STK for the transfer of the address
    // to be secure.
    bt_log(ERROR, "sm",
           "abort pairing due to identity address received outside phase 3");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (!legacy_state_->ShouldReceiveIdentity()) {
    bt_log(ERROR, "sm", "received unexpected identity address");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // The identity address must be sent after the IRK (Vol 3, Part H, 3.6.1).
  if (!legacy_state_->has_irk) {
    bt_log(ERROR, "sm", "received identity address before the IRK!");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  if (legacy_state_->obtained_remote_keys & KeyDistGen::kIdKey) {
    bt_log(ERROR, "sm", "already received identity information!");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Store the identity address and mark all identity info as received.
  legacy_state_->identity_address = identity_address;
  legacy_state_->obtained_remote_keys |= KeyDistGen::kIdKey;

  // "IdKey" received. Complete pairing if possible.
  OnExpectedKeyReceived();
}

void PairingState::OnSecurityRequest(AuthReqField auth_req) {
  ZX_DEBUG_ASSERT(!legacy_state_);
  ZX_DEBUG_ASSERT(le_link_);
  ZX_DEBUG_ASSERT(le_link_->role() == hci::Connection::Role::kMaster);

  SecurityLevel requested_level;
  if (auth_req & AuthReq::kMITM) {
    requested_level = SecurityLevel::kAuthenticated;
  } else {
    requested_level = SecurityLevel::kEncrypted;
  }

  // If we already have a LTK and its security properties satisfy the request,
  // then we start link layer encryption (which will either encrypt the link or
  // perform a key refresh). See Vol 3, Part H, Figure 2.7 for the algorithm.
  // TODO(armansito): This should compare the peer's SC requirement against the
  // LTK's security properties. Since we currently don't support LE Secure
  // Connections we assume that no local LTK ever satisfies this. If the peer
  // requests SC, then we'll initiate pairing as normal and let the peer accept
  // or reject the request.
  if (ltk_ && (ltk_->security().level() >= requested_level) &&
      !(auth_req & AuthReq::kSC)) {
    // The existing key satisfies the security requirement.
    bt_log(TRACE, "sm", "responding to security request using existing LTK");
    ZX_DEBUG_ASSERT(le_link_->ltk());
    ZX_DEBUG_ASSERT(*le_link_->ltk() == ltk_->key());
    le_link_->StartEncryption();
    return;
  }

  // Initiate pairing.
  UpgradeSecurity(requested_level, [](Status status, const auto& security) {
    bt_log(TRACE, "sm", "security request resolved - %s %s",
           status.ToString().c_str(), security.ToString().c_str());
  });
}

void PairingState::OnEncryptionChange(hci::Status status, bool enabled) {
  // First notify the delegate in case of failure.
  if (bt_is_error(status, ERROR, "sm", "link layer authentication failed")) {
    ZX_DEBUG_ASSERT(delegate_);
    delegate_->OnAuthenticationFailure(status);
  }

  if (!enabled) {
    bt_log(TRACE, "sm", "encryption disabled (handle: %#.4x)",
           le_link_->handle());
    SetSecurityProperties(sm::SecurityProperties());
  } else if (!legacy_state_) {
    bt_log(TRACE, "sm", "encryption enabled while not pairing");
    // If encryption was enabled while not pairing, then the link key must match
    // |ltk_|. We update the security properties based on that key. Otherwise,
    // we let the pairing handlers below determine the security properties (i.e.
    // EndLegacyPairingPhase2() and CompleteLegacyPairing().
    ZX_DEBUG_ASSERT(le_link_->ltk());
    ZX_DEBUG_ASSERT(ltk_);
    ZX_DEBUG_ASSERT(*le_link_->ltk() == ltk_->key());
    SetSecurityProperties(ltk_->security());
  }

  // Nothing to do if no pairing is in progress.
  if (!legacy_state_) {
    return;
  }

  if (!status || !enabled) {
    bt_log(ERROR, "sm", "failed to encrypt link during pairing");
    AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    return;
  }

  ZX_DEBUG_ASSERT(le_smp_->pairing_started());

  if (legacy_state_->InPhase2()) {
    bt_log(TRACE, "sm", "link encrypted with STK");
    EndLegacyPairingPhase2();
    return;
  }

  // If encryption was enabled after Phase 3 then this completes the pairing
  // procedure.
  if (legacy_state_->RequestedKeysObtained() &&
      legacy_state_->WaitingForEncryptionWithLTK()) {
    bt_log(TRACE, "sm", "link encrypted with LTK");
    legacy_state_->ltk_encrypted = true;
    CompleteLegacyPairing();
  }
}

void PairingState::OnExpectedKeyReceived() {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(!legacy_state_->ltk_encrypted);
  ZX_DEBUG_ASSERT(legacy_state_->stk_encrypted);

  if (!legacy_state_->RequestedKeysObtained()) {
    ZX_DEBUG_ASSERT(legacy_state_->InPhase3());
    bt_log(TRACE, "sm", "more keys pending");
    return;
  }

  if (legacy_state_->features->initiator && !legacy_state_->LocalKeysSent()) {
    SendLocalKeys();
  }

  // We are no longer in Phase 3.
  ZX_DEBUG_ASSERT(!legacy_state_->InPhase3());

  // Complete pairing now if we don't need to wait for encryption using the LTK.
  // Otherwise we'll mark it as complete when the link is encrypted with it.
  if (legacy_state_->IsComplete()) {
    CompleteLegacyPairing();
    return;
  }

  if (ltk_ && legacy_state_->features->initiator) {
    ZX_DEBUG_ASSERT(le_link_->ltk());
    ZX_DEBUG_ASSERT(*le_link_->ltk() == ltk_->key());
    if (!le_link_->StartEncryption()) {
      bt_log(ERROR, "sm", "failed to start encryption");
      AbortLegacyPairing(ErrorCode::kUnspecifiedReason);
    }
  }
}

void PairingState::LEPairingAddresses(const DeviceAddress** out_initiator,
                                      const DeviceAddress** out_responder) {
  ZX_DEBUG_ASSERT(legacy_state_);
  ZX_DEBUG_ASSERT(legacy_state_->features);

  if (legacy_state_->features->initiator) {
    *out_initiator = &le_link_->local_address();
    *out_responder = &le_link_->peer_address();
  } else {
    *out_initiator = &le_link_->peer_address();
    *out_responder = &le_link_->local_address();
  }
}

void PairingState::AssignLongTermKeyInternal(const LTK& ltk) {
  ltk_ = ltk;
  le_link_->set_link_key(ltk.key());
}

}  // namespace sm
}  // namespace btlib
