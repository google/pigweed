// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_state.h"

#include <zircon/assert.h>
#include <zircon/status.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "lib/async/default.h"
#include "lib/fit/function.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/idle_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_secure_connections.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "util.h"

namespace bt {

namespace sm {

namespace {

SecurityProperties FeaturesToProperties(const PairingFeatures& features) {
  return SecurityProperties(features.method == PairingMethod::kJustWorks
                                ? SecurityLevel::kEncrypted
                                : SecurityLevel::kAuthenticated,
                            features.encryption_key_size, features.secure_connections);
}
}  // namespace

PairingState::PendingRequest::PendingRequest(SecurityLevel level, PairingCallback callback)
    : level(level), callback(std::move(callback)) {}

PairingState::~PairingState() {
  if (le_link_) {
    le_link_->set_encryption_change_callback({});
  }
}

PairingState::PairingState(fxl::WeakPtr<hci::Connection> link, fbl::RefPtr<l2cap::Channel> smp,
                           IOCapability io_capability, fxl::WeakPtr<Delegate> delegate,
                           BondableMode bondable_mode)
    : next_pairing_id_(0),
      delegate_(std::move(delegate)),
      le_link_(std::move(link)),
      io_cap_(io_capability),
      bondable_mode_(bondable_mode),
      le_sec_(SecurityProperties()),
      sm_chan_(std::make_unique<PairingChannel>(smp)),
      role_(le_link_->role() == hci::Connection::Role::kMaster ? Role::kInitiator
                                                               : Role::kResponder),
      weak_ptr_factory_(this) {
  ZX_ASSERT(delegate_);
  ZX_ASSERT(le_link_);
  ZX_ASSERT(smp);
  ZX_ASSERT(le_link_->handle() == smp->link_handle());
  ZX_ASSERT(le_link_->ll_type() == hci::Connection::LinkType::kLE);
  ZX_ASSERT(smp->id() == l2cap::kLESMPChannelId);

  // `current_phase_` is default constructed into std::monostate in the initializer list, but we
  // immediately assign IdlePhase to `current_phase_` through this `GoToIdlePhase` call.
  GoToIdlePhase();

  // Set up HCI encryption event.
  le_link_->set_encryption_change_callback(
      fit::bind_member(this, &PairingState::OnEncryptionChange));
}

void PairingState::OnSecurityRequest(AuthReqField auth_req) {
  ZX_ASSERT(std::holds_alternative<IdlePhase>(current_phase_));
  ZX_ASSERT(role_ == Role::kInitiator);

  SecurityLevel requested_level;
  if (auth_req & AuthReq::kMITM) {
    requested_level = SecurityLevel::kAuthenticated;
  } else {
    requested_level = SecurityLevel::kEncrypted;
  }

  // If we already have a LTK and its security properties satisfy the request, then we start link
  // layer encryption (which will either encrypt the link or perform a key refresh). See Vol 3,
  // Part H, Figure 2.7 for the algorithm.
  if (ltk_ && (ltk_->security().level() >= requested_level) &&
      (!(auth_req & AuthReq::kSC) || ltk_->security().secure_connections())) {
    if (bt_is_error(ValidateExistingLocalLtk(), ERROR, "sm",
                    "disconnecting link as it cannot be encrypted with LTK status")) {
      return;
    }
    le_link_->StartEncryption();
    return;
  }
  // V5.1 Vol. 3 Part H Section 3.4: "Upon [...] reception of the Security Request command, the
  // Security Manager Timer shall be [...] restarted."
  StartNewTimer();
  // Initiate pairing.
  UpgradeSecurity(requested_level, [](Status status, const auto& security) {
    bt_log(DEBUG, "sm", "security request resolved - %s %s", bt_str(status), bt_str(security));
  });
}

void PairingState::UpgradeSecurity(SecurityLevel level, PairingCallback callback) {
  // If pairing is in progress then we queue the request.
  IdlePhase* idle_phase = std::get_if<IdlePhase>(&current_phase_);
  if (!idle_phase || idle_phase->pending_security_request().has_value()) {
    bt_log(TRACE, "sm", "LE security upgrade in progress; request for %s security queued",
           LevelToString(level));
    request_queue_.emplace(level, std::move(callback));
    return;
  }

  if (level <= le_sec_.level()) {
    callback(Status(), le_sec_);
    return;
  }
  // |request_queue| must be empty if there is no active security upgrade request, which is
  // equivalent to being in idle phase with no pending security request.
  ZX_ASSERT(request_queue_.empty());
  request_queue_.emplace(level, std::move(callback));
  UpgradeSecurityInternal();
}

void PairingState::OnPairingRequest(const PairingRequestParams& req_params) {
  IdlePhase* idle_phase = std::get_if<IdlePhase>(&current_phase_);
  ZX_ASSERT(idle_phase);
  ZX_ASSERT(role_ == Role::kResponder);
  // V5.1 Vol. 3 Part H Section 3.4: "Upon [...] reception of the Pairing Request command, the
  // Security Manager Timer shall be reset and started."
  StartNewTimer();

  // We only require authentication as Responder if there is a pending Security Request for it.
  // TODO(52999): This will need to be modified to support Secure Connections Only mode.
  auto required_level = idle_phase->pending_security_request().value_or(SecurityLevel::kEncrypted);

  current_phase_ = Phase1::CreatePhase1Responder(
      sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), req_params, io_cap_, bondable_mode_,
      required_level, fit::bind_member(this, &PairingState::OnFeatureExchange));
  std::get<std::unique_ptr<Phase1>>(current_phase_)->Start();
}

void PairingState::UpgradeSecurityInternal() {
  IdlePhase* idle_phase = std::get_if<IdlePhase>(&current_phase_);
  ZX_ASSERT_MSG(idle_phase, "cannot upgrade security while security upgrade already in progress!");
  const PendingRequest& next_req = request_queue_.front();
  if (next_req.level >= SecurityLevel::kAuthenticated &&
      io_cap_ == IOCapability::kNoInputNoOutput) {
    bt_log(WARN, "sm",
           "cannot fulfill authenticated security request as IOCapabilities are NoInputNoOutput");
    next_req.callback(Status(ErrorCode::kAuthenticationRequirements), le_sec_);
    request_queue_.pop();
    if (!request_queue_.empty()) {
      UpgradeSecurityInternal();
    }
    return;
  }

  // V5.1 Vol. 3 Part H Section 3.4: "Upon transmission of the Pairing Request command [...] the
  // [SM] Timer shall be [..] started" and "Upon transmission of the Security Request command [...]
  // the [SM] Timer shall be reset and restarted".
  StartNewTimer();
  if (role_ == Role::kInitiator) {
    current_phase_ = Phase1::CreatePhase1Initiator(
        sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), io_cap_, bondable_mode_,
        next_req.level, fit::bind_member(this, &PairingState::OnFeatureExchange));
    std::get<std::unique_ptr<Phase1>>(current_phase_)->Start();
  } else {
    idle_phase->MakeSecurityRequest(next_req.level, bondable_mode_);
  }
}

void PairingState::OnFeatureExchange(PairingFeatures features, PairingRequestParams preq,
                                     PairingResponseParams pres) {
  ZX_ASSERT(std::holds_alternative<std::unique_ptr<Phase1>>(current_phase_));
  bt_log(TRACE, "sm", "obtained LE Pairing features");
  next_pairing_id_++;
  features_ = features;

  const auto [initiator_addr, responder_addr] = LEPairingAddresses();
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (!features.secure_connections) {
    auto preq_pdu = util::NewPdu(sizeof(PairingRequestParams)),
         pres_pdu = util::NewPdu(sizeof(PairingResponseParams));
    PacketWriter preq_writer(kPairingRequest, preq_pdu.get()),
        pres_writer(kPairingResponse, pres_pdu.get());
    *preq_writer.mutable_payload<PairingRequestParams>() = preq;
    *pres_writer.mutable_payload<PairingRequestParams>() = pres;
    current_phase_.emplace<Phase2Legacy>(
        sm_chan_->GetWeakPtr(), self, role_, features, *preq_pdu, *pres_pdu, initiator_addr,
        responder_addr, fit::bind_member(this, &PairingState::OnPhase2EncryptionKey));
    std::get<Phase2Legacy>(current_phase_).Start();
  } else {
    current_phase_.emplace<Phase2SecureConnections>(
        sm_chan_->GetWeakPtr(), self, role_, features, preq, pres, initiator_addr, responder_addr,
        fit::bind_member(this, &PairingState::OnPhase2EncryptionKey));
    std::get<Phase2SecureConnections>(current_phase_).Start();
  }
}

void PairingState::OnPhase2EncryptionKey(const UInt128& new_key) {
  ZX_ASSERT(le_link_);
  ZX_ASSERT(features_);
  ZX_ASSERT(InPhase2());
  // EDiv and Rand values are 0 for Phase 2 keys generated by Legacy or Secure Connections (Vol 3,
  // Part H, 2.4.4 / 2.4.4.1). Secure Connections generates an LTK, while Legacy generates an STK.
  auto new_link_key = hci::LinkKey(new_key, 0, 0);

  if (features_->secure_connections) {
    OnNewLongTermKey(LTK(FeaturesToProperties(*features_), new_link_key));
  } else {
    // `set_le_ltk` sets the encryption key of the LE link (which is the STK for Legacy), not the
    // long-term key that results from pairing (which is generated in Phase 3 for Legacy).
    le_link_->set_le_ltk(new_link_key);
  }
  // If we're the initiator, we encrypt the link. If we're the responder, we wait for the initiator
  // to encrypt the link with the new key.|le_link_| will respond to the HCI "LTK request" event
  // with the `new_link_key` assigned above, which should trigger OnEncryptionChange.
  if (role_ == Role::kInitiator) {
    if (!le_link_->StartEncryption()) {
      bt_log(ERROR, "sm", "failed to start encryption");
      Abort(ErrorCode::kUnspecifiedReason);
    }
  }
}

void PairingState::OnEncryptionChange(hci::Status status, bool enabled) {
  // First notify the delegate in case of failure.
  if (bt_is_error(status, ERROR, "sm", "link layer authentication failed")) {
    ZX_ASSERT(delegate_);
    delegate_->OnAuthenticationFailure(status);
  }

  IdlePhase* idle_phase = std::get_if<IdlePhase>(&current_phase_);
  if (!status || !enabled) {
    bt_log(WARN, "sm", "encryption of link (handle: %#.4x) %s%s!", le_link_->handle(),
           !status ? fxl::StringPrintf("failed with %s", bt_str(status)).c_str() : "disabled",
           idle_phase ? "" : " during pairing");
    SetSecurityProperties(sm::SecurityProperties());
    if (!idle_phase) {
      Abort(ErrorCode::kUnspecifiedReason);
    }
    return;
  }

  if (idle_phase) {
    bt_log(DEBUG, "sm", "encryption enabled while not pairing");
    if (bt_is_error(ValidateExistingLocalLtk(), ERROR, "sm",
                    "disconnecting link as it cannot be encrypted with LTK status")) {
      return;
    }
    std::optional<SecurityLevel> security_req = idle_phase->pending_security_request();
    if (security_req.has_value() && *security_req > ltk_->security().level()) {
      bt_log(ERROR, "sm",
             "peer responded to security request by encrypting link with a bonded "
             "LTK of insufficient security properties - disconnecting link");
      (*sm_chan_)->SignalLinkError();
      return;
    }
    // If encryption is enabled while not pairing, we update the security properties to those of
    // `ltk_`. Otherwise, we let the EndPhase2 pairing function determine the security properties.
    SetSecurityProperties(ltk_->security());
    if (security_req.has_value()) {
      ZX_ASSERT(role_ == Role::kResponder);
      ZX_ASSERT(!request_queue_.empty());
      NotifySecurityCallbacks();
    }
    return;
  }

  if (InPhase2()) {
    bt_log(DEBUG, "sm", "link encrypted with phase 2 generated key");
    EndPhase2();
  }
}

void PairingState::EndPhase2() {
  ZX_ASSERT(features_.has_value());
  ZX_ASSERT(InPhase2());

  SetSecurityProperties(FeaturesToProperties(*features_));
  // If there are no keys to distribute, don't bother creating Phase 3
  if (!features_->local_key_distribution && !features_->remote_key_distribution) {
    OnPairingComplete(PairingData());
    return;
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  current_phase_.emplace<Phase3>(sm_chan_->GetWeakPtr(), self, role_, *features_, le_sec_,
                                 fit::bind_member(this, &PairingState::OnPairingComplete));
  std::get<Phase3>(current_phase_).Start();
}

void PairingState::OnPairingComplete(PairingData pairing_data) {
  // We must either be in Phase3 or Phase 2 with no keys to distribute if pairing has completed.
  if (!std::holds_alternative<Phase3>(current_phase_)) {
    ZX_ASSERT(InPhase2());
    ZX_ASSERT(!features_->local_key_distribution);
    ZX_ASSERT(!features_->remote_key_distribution);
  }
  ZX_ASSERT(delegate_);
  ZX_ASSERT(features_.has_value());
  bt_log(DEBUG, "sm", "LE pairing complete");
  delegate_->OnPairingComplete(Status());
  // In Secure Connections, the LTK will be generated in Phase 2, not exchanged in Phase 3, so
  // we want to ensure that it is still put in the pairing_data.
  if (features_->secure_connections) {
    ZX_ASSERT(ltk_.has_value());
    pairing_data.peer_ltk = pairing_data.local_ltk = ltk_;
  } else {
    // The SM-internal LTK is used to validate future encryption events on the existing link.
    // Encryption with LTKs generated by LE legacy pairing uses the key received by the link-layer
    // master - so as initiator, this is the peer key, and as responder, this is the local key.
    const std::optional<LTK>& new_ltk =
        role_ == Role::kInitiator ? pairing_data.peer_ltk : pairing_data.local_ltk;
    if (new_ltk.has_value()) {
      OnNewLongTermKey(*new_ltk);
    }
  }
  if (features_->will_bond) {
    delegate_->OnNewPairingData(pairing_data);
  } else {
    bt_log(
        INFO, "gap-le", " %s pairing complete in non-bondable mode with [%s%s%s%s%s]",
        features_->secure_connections ? "secure connections" : "legacy",
        pairing_data.peer_ltk ? "peer_ltk " : "", pairing_data.local_ltk ? "local_ltk " : "",
        pairing_data.irk ? "irk " : "",
        pairing_data.identity_address
            ? fxl::StringPrintf("(identity: %s) ", bt_str(*pairing_data.identity_address)).c_str()
            : "",
        pairing_data.csrk ? "csrk " : "");
  }
  // Go back to IdlePhase so we can pair again if need be.
  ResetState();

  NotifySecurityCallbacks();
}

void PairingState::NotifySecurityCallbacks() {
  // Separate out the requests that are satisfied by the current security level from those that
  // require a higher level. We'll retry pairing for the latter.
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

  if (!request_queue_.empty()) {
    UpgradeSecurityInternal();
  }
}

void PairingState::Reset(IOCapability io_capability) {
  Abort();
  io_cap_ = io_capability;
  ResetState();
}

void PairingState::ResetState() {
  StopTimer();
  features_.reset();
  GoToIdlePhase();
}

bool PairingState::AssignLongTermKey(const LTK& ltk) {
  if (!std::holds_alternative<IdlePhase>(current_phase_)) {
    bt_log(DEBUG, "sm", "Cannot directly assign LTK while pairing is in progress");
    return false;
  }

  OnNewLongTermKey(ltk);

  // The initiatior starts encryption when it receives a new LTK from GAP.
  if (role_ == Role::kInitiator && !le_link_->StartEncryption()) {
    bt_log(ERROR, "sm", "Failed to initiate authentication procedure");
    return false;
  }

  return true;
}

void PairingState::SetSecurityProperties(const SecurityProperties& sec) {
  if (sec != le_sec_) {
    bt_log(DEBUG, "sm", "security properties changed - handle: %#.4x, new: %s, old: %s",
           le_link_->handle(), bt_str(sec), bt_str(le_sec_));
    le_sec_ = sec;
    delegate_->OnNewSecurityProperties(le_sec_);
  }
}

void PairingState::Abort(ErrorCode ecode) {
  std::visit(
      [=](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<std::unique_ptr<Phase1>, T>) {
          arg->Abort(ecode);
        } else if constexpr (std::is_base_of_v<PairingPhase, T>) {
          arg.Abort(ecode);
        } else {
          ZX_PANIC("cannot abort when current_phase_ is std::monostate!");
        }
      },
      current_phase_);
  // "Abort" should trigger OnPairingFailed.
}

std::optional<IdentityInfo> PairingState::OnIdentityRequest() {
  // This is called by the bearer to determine if we have local identity
  // information to distribute.
  ZX_ASSERT(delegate_);
  return delegate_->OnIdentityInformationRequest();
}

void PairingState::ConfirmPairing(ConfirmCallback confirm) {
  ZX_ASSERT(delegate_);
  delegate_->ConfirmPairing([id = next_pairing_id_, self = weak_ptr_factory_.GetWeakPtr(),
                             cb = std::move(confirm)](bool confirm) {
    if (!self || self->next_pairing_id_ != id) {
      bt_log(TRACE, "sm", "ignoring user confirmation for expired pairing: id = %lu", id);
      return;
    }
    cb(confirm);
  });
}

void PairingState::DisplayPasskey(uint32_t passkey, Delegate::DisplayMethod method,
                                  ConfirmCallback confirm) {
  ZX_ASSERT(delegate_);
  delegate_->DisplayPasskey(passkey, method,
                            [id = next_pairing_id_, self = weak_ptr_factory_.GetWeakPtr(), method,
                             cb = std::move(confirm)](bool confirm) {
                              if (!self || self->next_pairing_id_ != id) {
                                bt_log(TRACE, "sm",
                                       "ignoring %s response for expired pairing: id = %lu",
                                       util::DisplayMethodToString(method).c_str(), id);
                                return;
                              }
                              cb(confirm);
                            });
}

void PairingState::RequestPasskey(PasskeyResponseCallback respond) {
  ZX_ASSERT(delegate_);
  delegate_->RequestPasskey([id = next_pairing_id_, self = weak_ptr_factory_.GetWeakPtr(),
                             cb = std::move(respond)](int64_t passkey) {
    if (!self || self->next_pairing_id_ != id) {
      bt_log(TRACE, "sm", "ignoring passkey input response for expired pairing: id = %lu", id);
      return;
    }
    cb(passkey);
  });
}

void PairingState::OnPairingFailed(Status status) {
  bt_log(ERROR, "sm", "LE pairing failed: %s", status.ToString().c_str());
  StopTimer();
  // TODO(NET-1201): implement "waiting interval" to prevent repeated attempts
  // as described in Vol 3, Part H, 2.3.6.

  ZX_ASSERT(delegate_);
  delegate_->OnPairingComplete(status);

  auto requests = std::move(request_queue_);
  while (!requests.empty()) {
    requests.front().callback(status, le_sec_);
    requests.pop();
  }

  if (!std::holds_alternative<IdlePhase>(current_phase_)) {
    ZX_ASSERT(le_link_);
    le_link_->set_le_ltk(hci::LinkKey());
  }
  ResetState();
}

bool PairingState::StartNewTimer() {
  if (timeout_task_.is_pending()) {
    ZX_ASSERT(timeout_task_.Cancel() == ZX_OK);
  }
  timeout_task_.PostDelayed(async_get_default_dispatcher(), kPairingTimeout);
  return true;
}

void PairingState::StopTimer() {
  if (timeout_task_.is_pending()) {
    zx_status_t status = timeout_task_.Cancel();
    if (status != ZX_OK) {
      bt_log(TRACE, "sm", "smp: failed to stop timer: %s", zx_status_get_string(status));
    }
  }
}

void PairingState::OnPairingTimeout() {
  std::visit(
      [=](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<std::unique_ptr<Phase1>, T>) {
          arg->OnPairingTimeout();
        } else if constexpr (std::is_base_of_v<PairingPhase, T>) {
          arg.OnPairingTimeout();
        } else {
          ZX_PANIC("cannot timeout when current_phase_ is std::monostate!");
        }
      },
      current_phase_);
}

std::pair<DeviceAddress, DeviceAddress> PairingState::LEPairingAddresses() {
  ZX_ASSERT(!std::holds_alternative<IdlePhase>(current_phase_));
  const DeviceAddress *initiator = &le_link_->local_address(),
                      *responder = &le_link_->peer_address();
  if (role_ == Role::kResponder) {
    std::swap(initiator, responder);
  }
  return std::make_pair(*initiator, *responder);
}

void PairingState::OnNewLongTermKey(const LTK& ltk) {
  ltk_ = ltk;
  le_link_->set_le_ltk(ltk.key());
}

void PairingState::GoToIdlePhase() {
  current_phase_.emplace<IdlePhase>(sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), role_,
                                    fit::bind_member(this, &PairingState::OnPairingRequest),
                                    fit::bind_member(this, &PairingState::OnSecurityRequest));
}

Status PairingState::ValidateExistingLocalLtk() {
  auto err = HostError::kNoError;
  if (!ltk_.has_value() || !le_link_->ltk().has_value()) {
    // The LTKs should always be present when this method is called.
    err = HostError::kNotFound;
  } else if (!(*le_link_->ltk() == ltk_->key())) {
    // As only SM should ever change the LE Link encryption key, these two values should always be
    // in sync, i.e. something in the system is acting unreliably if they get out of sync.
    err = HostError::kNotReliable;
  }
  Status status(err);
  if (!status) {
    // SM does not own the link, so although the checks above should never fail, disconnecting the
    // link (vs. ASSERTing these checks) is safer against non-SM code potentially touching the key.
    delegate_->OnAuthenticationFailure(hci::Status(hci::StatusCode::kPinOrKeyMissing));
    (*sm_chan_)->SignalLinkError();
  }
  return status;
}

}  // namespace sm
}  // namespace bt
