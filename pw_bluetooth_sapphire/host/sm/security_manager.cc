// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "security_manager.h"

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
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_secure_connections.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_request_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "util.h"

namespace bt::sm {

namespace {

SecurityProperties FeaturesToProperties(const PairingFeatures& features) {
  return SecurityProperties(features.method == PairingMethod::kJustWorks
                                ? SecurityLevel::kEncrypted
                                : SecurityLevel::kAuthenticated,
                            features.encryption_key_size, features.secure_connections);
}
}  // namespace

class SecurityManagerImpl final : public SecurityManager,
                                  public PairingPhase::Listener,
                                  public PairingChannel::Handler {
 public:
  ~SecurityManagerImpl() override;
  SecurityManagerImpl(fxl::WeakPtr<hci::LowEnergyConnection> link, fbl::RefPtr<l2cap::Channel> smp,
                      IOCapability io_capability, fxl::WeakPtr<Delegate> delegate,
                      BondableMode bondable_mode, gap::LESecurityMode security_mode);
  // SecurityManager overrides:
  bool AssignLongTermKey(const LTK& ltk) override;
  void UpgradeSecurity(SecurityLevel level, PairingCallback callback) override;
  void Reset(IOCapability io_capability) override;
  void Abort(ErrorCode ecode) override;

 private:
  // Represents a pending request to update the security level.
  struct PendingRequest {
    PendingRequest(SecurityLevel level, PairingCallback callback);
    PendingRequest(PendingRequest&&) = default;
    PendingRequest& operator=(PendingRequest&&) = default;

    SecurityLevel level;
    PairingCallback callback;
  };

  // Called when we receive a peer security request as initiator, will start Phase 1.
  void OnSecurityRequest(AuthReqField auth_req);

  // Called when we receive a peer pairing request as responder, will start Phase 1.
  void OnPairingRequest(const PairingRequestParams& req_params);

  // Pulls the next PendingRequest off |request_queue_| and starts a security upgrade to that
  // |level| by either sending a Pairing Request as initiator or a Security Request as responder.
  void UpgradeSecurityInternal();

  // Creates the pairing phase responsible for sending the security upgrade request to the peer
  // (a PairingRequest if we are initiator, otherwise a SecurityRequest). Returns
  // fitx::error(ErrorCode:: kAuthenticationRequirements) if the local IOCapabilities are
  // insufficient for SecurityLevel, otherwise returns fitx::ok().
  [[nodiscard]] fitx::result<ErrorCode> RequestSecurityUpgrade(SecurityLevel level);

  // Called when the feature exchange (Phase 1) completes and the relevant features of both sides
  // have been resolved into `features`. `preq` and `pres` need to be retained for cryptographic
  // calculations in Phase 2. Causes a state transition from Phase 1 to Phase 2
  void OnFeatureExchange(PairingFeatures features, PairingRequestParams preq,
                         PairingResponseParams pres);

  // Called when Phase 2 generates an encryption key, so the link can be encrypted with it.
  void OnPhase2EncryptionKey(const UInt128& new_key);

  // Check if encryption using `current_ltk` will satisfy the current security requirements.
  static bool CurrentLtkInsufficientlySecureForEncryption(
      std::optional<LTK> current_ltk, SecurityRequestPhase* security_request_phase,
      gap::LESecurityMode mode);

  // Called when the encryption state of the LE link changes.
  void OnEncryptionChange(hci::Result<bool> enabled_result);

  // Called when the link is encrypted at the end of pairing Phase 2.
  void EndPhase2();

  // Cleans up pairing state, updates the current security level, and notifies parties that
  // requested security of the link's updated security properties.
  void OnPairingComplete(PairingData data);

  // After a call to UpgradeSecurity results in an increase of the link security level (through
  // pairing completion or SMP Security Requested encryption), this method notifies all the
  // callbacks associated with SecurityUpgrade requests.
  void NotifySecurityCallbacks();

  // Assign the current security properties and notify the delegate of the
  // change.
  void SetSecurityProperties(const SecurityProperties& sec);

  // Directly assigns the current |ltk_| and the underlying |le_link_|'s link key. This function
  // does not initiate link layer encryption and can be called during and outside of pairing.
  void OnNewLongTermKey(const LTK& ltk);

  // PairingPhase::Listener overrides:
  void OnPairingFailed(Error error) override;
  std::optional<IdentityInfo> OnIdentityRequest() override;
  void ConfirmPairing(ConfirmCallback confirm) override;
  void DisplayPasskey(uint32_t passkey, Delegate::DisplayMethod method,
                      ConfirmCallback cb) override;
  void RequestPasskey(PasskeyResponseCallback respond) override;

  // PairingChannel::Handler overrides. SecurityManagerImpl is only the fallback handler, meaning
  // these methods are only called by PairingChannel when no security upgrade is in progress:
  void OnRxBFrame(ByteBufferPtr sdu) override;
  void OnChannelClosed() override;

  // Starts the SMP timer. Stops and cancels any in-progress timers.
  void StartNewTimer();
  // Stops and resets the SMP Pairing Timer.
  void StopTimer();
  // Called when the pairing timer expires, forcing the pairing process to stop
  void OnPairingTimeout();

  // Returns a std::pair<InitiatorAddress, ResponderAddress>. Will assert if called outside active
  // pairing or before Phase 1 is complete.
  std::pair<DeviceAddress, DeviceAddress> LEPairingAddresses();

  // Puts the class into a non-pairing state.
  void ResetState();

  // Returns true if the pairing state machine is currently in Phase 2 of pairing.
  bool InPhase2() const {
    return std::holds_alternative<Phase2Legacy>(current_phase_) ||
           std::holds_alternative<Phase2SecureConnections>(current_phase_);
  }

  bool SecurityUpgradeInProgress() const {
    return !std::holds_alternative<std::monostate>(current_phase_);
  }

  // Validates that both SM and the link have stored LTKs, and that these values match. Disconnects
  // the link if it finds an issue. Should only be called when an LTK is expected to exist.
  Result<> ValidateExistingLocalLtk();

  // The ID that will be assigned to the next pairing operation.
  PairingProcedureId next_pairing_id_;

  // The higher-level class acting as a delegate for operations outside of SMP.
  fxl::WeakPtr<Delegate> delegate_;

  // Data for the currently registered LE-U link, if any.
  fxl::WeakPtr<hci::LowEnergyConnection> le_link_;

  // The IO capabilities of the device
  IOCapability io_cap_;

  // The current LTK assigned to this connection. This can be assigned directly
  // by calling AssignLongTermKey() or as a result of a pairing procedure.
  std::optional<LTK> ltk_;

  // If a pairing is in progress and Phase 1 (feature exchange) has completed, this will store the
  // result of that feature exchange. Otherwise, this will be std::nullopt.
  std::optional<PairingFeatures> features_;

  // The pending security requests added via UpgradeSecurity().
  std::queue<PendingRequest> request_queue_;

  // Fixed SMP Channel used to send/receive packets
  std::unique_ptr<PairingChannel> sm_chan_;

  // The role of the local device in pairing.
  Role role_;

  async::TaskClosureMethod<SecurityManagerImpl, &SecurityManagerImpl::OnPairingTimeout>
      timeout_task_{this};

  // The presence of a particular phase in this variant indicates that a security upgrade is in
  // progress at the stored phase. No security upgrade is in progress if std::monostate is present.
  std::variant<std::monostate, SecurityRequestPhase, std::unique_ptr<Phase1>, Phase2Legacy,
               Phase2SecureConnections, Phase3>
      current_phase_;

  fxl::WeakPtrFactory<SecurityManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SecurityManagerImpl);
};

SecurityManagerImpl::PendingRequest::PendingRequest(SecurityLevel level, PairingCallback callback)
    : level(level), callback(std::move(callback)) {}

SecurityManagerImpl::~SecurityManagerImpl() {
  if (le_link_) {
    le_link_->set_encryption_change_callback({});
  }
}

SecurityManagerImpl::SecurityManagerImpl(fxl::WeakPtr<hci::LowEnergyConnection> link,
                                         fbl::RefPtr<l2cap::Channel> smp,
                                         IOCapability io_capability,
                                         fxl::WeakPtr<Delegate> delegate,
                                         BondableMode bondable_mode,
                                         gap::LESecurityMode security_mode)
    : SecurityManager(bondable_mode, security_mode),
      next_pairing_id_(0),
      delegate_(std::move(delegate)),
      le_link_(std::move(link)),
      io_cap_(io_capability),
      sm_chan_(std::make_unique<PairingChannel>(
          smp, fit::bind_member<&SecurityManagerImpl::StartNewTimer>(this))),
      role_(le_link_->role() == hci_spec::ConnectionRole::kCentral ? Role::kInitiator
                                                                   : Role::kResponder),
      weak_ptr_factory_(this) {
  ZX_ASSERT(delegate_);
  ZX_ASSERT(le_link_);
  ZX_ASSERT(smp);
  ZX_ASSERT(le_link_->handle() == smp->link_handle());
  ZX_ASSERT(smp->id() == l2cap::kLESMPChannelId);
  // `current_phase_` is default constructed into std::monostate in the initializer list as no
  // security upgrade is in progress upon construction.

  // Set up HCI encryption event.
  le_link_->set_encryption_change_callback(
      fit::bind_member<&SecurityManagerImpl::OnEncryptionChange>(this));
  sm_chan_->SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void SecurityManagerImpl::OnSecurityRequest(AuthReqField auth_req) {
  ZX_ASSERT(!SecurityUpgradeInProgress());

  if (role_ != Role::kInitiator) {
    bt_log(INFO, "sm", "Received spurious Security Request while not acting as SM initiator");
    sm_chan_->SendMessageNoTimerReset(kPairingFailed, ErrorCode::kCommandNotSupported);
    return;
  }

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
  if (fitx::result result = RequestSecurityUpgrade(requested_level); result.is_error()) {
    // Per v5.3 Vol. 3 Part H 2.4.6, "When a Central receives a Security Request command it may
    // [...] reject the request.", which we do here as we know we are unable to fulfill it.
    sm_chan_->SendMessageNoTimerReset(kPairingFailed, result.error_value());
    // If we fail to start pairing, we need to stop the timer.
    StopTimer();
  }
}

void SecurityManagerImpl::UpgradeSecurity(SecurityLevel level, PairingCallback callback) {
  if (SecurityUpgradeInProgress()) {
    bt_log(TRACE, "sm", "LE security upgrade in progress; request for %s security queued",
           LevelToString(level));
    request_queue_.emplace(level, std::move(callback));
    return;
  }

  if (level <= security().level()) {
    callback(fitx::ok(), security());
    return;
  }

  // Secure Connections only mode only permits Secure Connections authenticated pairing with a 128-
  // bit encryption key, so we force all security upgrade requests to that level.
  if (security_mode() == gap::LESecurityMode::SecureConnectionsOnly) {
    level = SecurityLevel::kSecureAuthenticated;
  }

  // |request_queue| must be empty if there is no active security upgrade request, which is
  // equivalent to being in idle phase with no pending security request.
  ZX_ASSERT(request_queue_.empty());
  request_queue_.emplace(level, std::move(callback));
  UpgradeSecurityInternal();
}

void SecurityManagerImpl::OnPairingRequest(const PairingRequestParams& req_params) {
  // Only the initiator may send the Pairing Request (V5.0 Vol. 3 Part H 3.5.1).
  if (role_ != Role::kResponder) {
    bt_log(INFO, "sm", "rejecting \"Pairing Request\" as initiator");
    sm_chan_->SendMessageNoTimerReset(kPairingFailed, ErrorCode::kCommandNotSupported);
    return;
  }
  // V5.1 Vol. 3 Part H Section 3.4: "Upon [...] reception of the Pairing Request command, the
  // Security Manager Timer shall be reset and started."
  StartNewTimer();

  // We only require authentication as Responder if there is a pending Security Request for it.
  SecurityRequestPhase* security_req_phase = std::get_if<SecurityRequestPhase>(&current_phase_);
  auto required_level = security_req_phase ? security_req_phase->pending_security_request()
                                           : SecurityLevel::kEncrypted;

  // Secure Connections only mode only permits Secure Connections authenticated pairing with a 128-
  // bit encryption key, so we force all security upgrade requests to that level.
  if (security_mode() == gap::LESecurityMode::SecureConnectionsOnly) {
    required_level = SecurityLevel::kSecureAuthenticated;
  }

  current_phase_ = Phase1::CreatePhase1Responder(
      sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), req_params, io_cap_, bondable_mode(),
      required_level, fit::bind_member<&SecurityManagerImpl::OnFeatureExchange>(this));
  std::get<std::unique_ptr<Phase1>>(current_phase_)->Start();
}

void SecurityManagerImpl::UpgradeSecurityInternal() {
  ZX_ASSERT_MSG(!SecurityUpgradeInProgress(),
                "cannot upgrade security while security upgrade already in progress!");
  const PendingRequest& next_req = request_queue_.front();
  if (fitx::result result = RequestSecurityUpgrade(next_req.level); result.is_error()) {
    next_req.callback(ToResult(result.error_value()), security());
    request_queue_.pop();
    if (!request_queue_.empty()) {
      UpgradeSecurityInternal();
    }
  }
}

fitx::result<ErrorCode> SecurityManagerImpl::RequestSecurityUpgrade(SecurityLevel level) {
  if (level >= SecurityLevel::kAuthenticated && io_cap_ == IOCapability::kNoInputNoOutput) {
    bt_log(WARN, "sm",
           "cannot fulfill authenticated security request as IOCapabilities are NoInputNoOutput");
    return fitx::error(ErrorCode::kAuthenticationRequirements);
  }

  if (role_ == Role::kInitiator) {
    current_phase_ = Phase1::CreatePhase1Initiator(
        sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), io_cap_, bondable_mode(), level,
        fit::bind_member<&SecurityManagerImpl::OnFeatureExchange>(this));
    std::get<std::unique_ptr<Phase1>>(current_phase_)->Start();
  } else {
    current_phase_.emplace<SecurityRequestPhase>(
        sm_chan_->GetWeakPtr(), weak_ptr_factory_.GetWeakPtr(), level, bondable_mode(),
        fit::bind_member<&SecurityManagerImpl::OnPairingRequest>(this));
    std::get<SecurityRequestPhase>(current_phase_).Start();
  }
  return fitx::ok();
}

void SecurityManagerImpl::OnFeatureExchange(PairingFeatures features, PairingRequestParams preq,
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
        responder_addr, fit::bind_member<&SecurityManagerImpl::OnPhase2EncryptionKey>(this));
    std::get<Phase2Legacy>(current_phase_).Start();
  } else {
    current_phase_.emplace<Phase2SecureConnections>(
        sm_chan_->GetWeakPtr(), self, role_, features, preq, pres, initiator_addr, responder_addr,
        fit::bind_member<&SecurityManagerImpl::OnPhase2EncryptionKey>(this));
    std::get<Phase2SecureConnections>(current_phase_).Start();
  }
}

void SecurityManagerImpl::OnPhase2EncryptionKey(const UInt128& new_key) {
  ZX_ASSERT(le_link_);
  ZX_ASSERT(features_);
  ZX_ASSERT(InPhase2());
  // EDiv and Rand values are 0 for Phase 2 keys generated by Legacy or Secure Connections (Vol 3,
  // Part H, 2.4.4 / 2.4.4.1). Secure Connections generates an LTK, while Legacy generates an STK.
  auto new_link_key = hci_spec::LinkKey(new_key, 0, 0);

  if (features_->secure_connections) {
    OnNewLongTermKey(LTK(FeaturesToProperties(*features_), new_link_key));
  } else {
    // `set_le_ltk` sets the encryption key of the LE link (which is the STK for Legacy), not the
    // long-term key that results from pairing (which is generated in Phase 3 for Legacy).
    le_link_->set_ltk(new_link_key);
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

bool SecurityManagerImpl::CurrentLtkInsufficientlySecureForEncryption(
    std::optional<LTK> current_ltk, SecurityRequestPhase* security_request_phase,
    gap::LESecurityMode mode) {
  SecurityLevel current_ltk_sec =
      current_ltk ? current_ltk->security().level() : SecurityLevel::kNoSecurity;
  return (security_request_phase &&
          security_request_phase->pending_security_request() > current_ltk_sec) ||
         (mode == gap::LESecurityMode::SecureConnectionsOnly &&
          current_ltk_sec != SecurityLevel::kSecureAuthenticated);
}

void SecurityManagerImpl::OnEncryptionChange(hci::Result<bool> enabled_result) {
  // First notify the delegate in case of failure.
  if (bt_is_error(enabled_result, ERROR, "sm", "link layer authentication failed")) {
    ZX_ASSERT(delegate_);
    delegate_->OnAuthenticationFailure(fitx::error(enabled_result.error_value()));
  }

  if (enabled_result.is_error() || !enabled_result.value()) {
    bt_log(WARN, "sm", "encryption of link (handle: %#.4x) %s%s!", le_link_->handle(),
           enabled_result.is_error()
               ? bt_lib_cpp_string::StringPrintf("failed with %s", bt_str(enabled_result)).c_str()
               : "disabled",
           SecurityUpgradeInProgress() ? "" : " during security upgrade");
    SetSecurityProperties(sm::SecurityProperties());
    if (SecurityUpgradeInProgress()) {
      Abort(ErrorCode::kUnspecifiedReason);
    }
    return;
  }

  SecurityRequestPhase* security_request_phase = std::get_if<SecurityRequestPhase>(&current_phase_);
  if (CurrentLtkInsufficientlySecureForEncryption(ltk_, security_request_phase, security_mode())) {
    bt_log(WARN, "sm", "peer encrypted link with insufficiently secure key, disconnecting");
    delegate_->OnAuthenticationFailure(ToResult(HostError::kInsufficientSecurity));
    sm_chan_->SignalLinkError();
    return;
  }

  if (!SecurityUpgradeInProgress() || security_request_phase) {
    bt_log(DEBUG, "sm", "encryption enabled while not pairing");
    if (bt_is_error(ValidateExistingLocalLtk(), ERROR, "sm",
                    "disconnecting link as it cannot be encrypted with LTK status")) {
      return;
    }
    // If encryption is enabled while not pairing, we update the security properties to those of
    // `ltk_`. Otherwise, we let the EndPhase2 pairing function determine the security properties.
    SetSecurityProperties(ltk_->security());
    if (security_request_phase) {
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

void SecurityManagerImpl::EndPhase2() {
  ZX_ASSERT(features_.has_value());
  ZX_ASSERT(InPhase2());

  SetSecurityProperties(FeaturesToProperties(*features_));
  // If there are no keys to distribute, don't bother creating Phase 3
  if (!HasKeysToDistribute(*features_)) {
    OnPairingComplete(PairingData());
    return;
  }
  auto self = weak_ptr_factory_.GetWeakPtr();
  current_phase_.emplace<Phase3>(sm_chan_->GetWeakPtr(), self, role_, *features_, security(),
                                 fit::bind_member<&SecurityManagerImpl::OnPairingComplete>(this));
  std::get<Phase3>(current_phase_).Start();
}

void SecurityManagerImpl::OnPairingComplete(PairingData pairing_data) {
  // We must either be in Phase3 or Phase 2 with no keys to distribute if pairing has completed.
  if (!std::holds_alternative<Phase3>(current_phase_)) {
    ZX_ASSERT(InPhase2());
    ZX_ASSERT(!HasKeysToDistribute(*features_));
  }
  ZX_ASSERT(delegate_);
  ZX_ASSERT(features_.has_value());
  bt_log(DEBUG, "sm", "LE pairing complete");
  delegate_->OnPairingComplete(fitx::ok());
  // In Secure Connections, the LTK will be generated in Phase 2, not exchanged in Phase 3, so
  // we want to ensure that it is still put in the pairing_data.
  if (features_->secure_connections) {
    ZX_ASSERT(ltk_.has_value());
    pairing_data.peer_ltk = pairing_data.local_ltk = ltk_;
  } else {
    // The SM-internal LTK is used to validate future encryption events on the existing link.
    // Encryption with LTKs generated by LE legacy pairing uses the key received by the link-layer
    // central - so as initiator, this is the peer key, and as responder, this is the local key.
    const std::optional<LTK>& new_ltk =
        role_ == Role::kInitiator ? pairing_data.peer_ltk : pairing_data.local_ltk;
    if (new_ltk.has_value()) {
      OnNewLongTermKey(*new_ltk);
    }
  }

  if (features_->generate_ct_key.has_value()) {
    // If we are generating the CT key, we must be using secure connections, and as such the peer
    // and local LTKs will be equivalent.
    ZX_ASSERT(features_->secure_connections);
    ZX_ASSERT(pairing_data.peer_ltk == pairing_data.local_ltk);
    std::optional<UInt128> ct_key_value =
        util::LeLtkToBrEdrLinkKey(ltk_->key().value(), features_->generate_ct_key.value());
    if (ct_key_value) {
      pairing_data.cross_transport_key =
          sm::LTK(ltk_->security(), hci_spec::LinkKey(*ct_key_value, 0, 0));
    } else {
      bt_log(WARN, "sm", "failed to generate cross-transport key");
    }
  }

  if (features_->will_bond) {
    delegate_->OnNewPairingData(pairing_data);
  } else {
    bt_log(INFO, "gap-le", " %s pairing complete in non-bondable mode with [%s%s%s%s%s]",
           features_->secure_connections ? "secure connections" : "legacy",
           pairing_data.peer_ltk ? "peer_ltk " : "", pairing_data.local_ltk ? "local_ltk " : "",
           pairing_data.irk ? "irk " : "",
           pairing_data.identity_address
               ? bt_lib_cpp_string::StringPrintf("(identity: %s) ",
                                                 bt_str(*pairing_data.identity_address))
                     .c_str()
               : "",
           pairing_data.csrk ? "csrk " : "");
  }
  // So we can pair again if need be.
  ResetState();

  NotifySecurityCallbacks();
}

void SecurityManagerImpl::NotifySecurityCallbacks() {
  // Separate out the requests that are satisfied by the current security level from those that
  // require a higher level. We'll retry pairing for the latter.
  std::queue<PendingRequest> satisfied;
  std::queue<PendingRequest> unsatisfied;
  while (!request_queue_.empty()) {
    auto& request = request_queue_.front();
    if (request.level <= security().level()) {
      satisfied.push(std::move(request));
    } else {
      unsatisfied.push(std::move(request));
    }
    request_queue_.pop();
  }

  request_queue_ = std::move(unsatisfied);

  // Notify the satisfied requests with success.
  while (!satisfied.empty()) {
    satisfied.front().callback(fitx::ok(), security());
    satisfied.pop();
  }

  if (!request_queue_.empty()) {
    UpgradeSecurityInternal();
  }
}

void SecurityManagerImpl::Reset(IOCapability io_capability) {
  Abort(ErrorCode::kUnspecifiedReason);
  io_cap_ = io_capability;
  ResetState();
}

void SecurityManagerImpl::ResetState() {
  StopTimer();
  features_.reset();
  sm_chan_->SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
  current_phase_ = std::monostate{};
}

bool SecurityManagerImpl::AssignLongTermKey(const LTK& ltk) {
  if (SecurityUpgradeInProgress()) {
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

void SecurityManagerImpl::SetSecurityProperties(const SecurityProperties& sec) {
  if (sec != security()) {
    bt_log(DEBUG, "sm", "security properties changed - handle: %#.4x, new: %s, old: %s",
           le_link_->handle(), bt_str(sec), bt_str(security()));
    set_security(sec);
    delegate_->OnNewSecurityProperties(security());
  }
}

void SecurityManagerImpl::Abort(ErrorCode ecode) {
  std::visit(
      [=](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<std::unique_ptr<Phase1>, T>) {
          arg->Abort(ecode);
        } else if constexpr (std::is_base_of_v<PairingPhase, T>) {
          arg.Abort(ecode);
        } else {
          bt_log(DEBUG, "sm", "Attempted to abort security upgrade while not in progress");
        }
      },
      current_phase_);
  // "Abort" should trigger OnPairingFailed.
}

std::optional<IdentityInfo> SecurityManagerImpl::OnIdentityRequest() {
  // This is called by the bearer to determine if we have local identity
  // information to distribute.
  ZX_ASSERT(delegate_);
  return delegate_->OnIdentityInformationRequest();
}

void SecurityManagerImpl::ConfirmPairing(ConfirmCallback confirm) {
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

void SecurityManagerImpl::DisplayPasskey(uint32_t passkey, Delegate::DisplayMethod method,
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

void SecurityManagerImpl::RequestPasskey(PasskeyResponseCallback respond) {
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

void SecurityManagerImpl::OnRxBFrame(ByteBufferPtr sdu) {
  fpromise::result<ValidPacketReader, ErrorCode> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    bt_log(INFO, "sm", "dropped SMP packet: %s", bt_str(ToResult(maybe_reader.error())));
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingRequest) {
    OnPairingRequest(reader.payload<PairingRequestParams>());
  } else if (smp_code == kSecurityRequest) {
    OnSecurityRequest(reader.payload<AuthReqField>());
  } else {
    bt_log(INFO, "sm", "dropped unexpected SMP code %#.2X when not pairing", smp_code);
  }
}

void SecurityManagerImpl::OnChannelClosed() {
  bt_log(DEBUG, "sm", "SMP channel closed while not pairing");
}

void SecurityManagerImpl::OnPairingFailed(Error error) {
  std::string phase_status = std::visit(
      [=](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        std::string s;
        if constexpr (std::is_same_v<std::unique_ptr<Phase1>, T>) {
          s = arg->ToString();
        } else if constexpr (std::is_base_of_v<PairingPhase, T>) {
          s = arg.ToString();
        } else {
          ZX_PANIC("security upgrade cannot fail when current_phase_ is std::monostate!");
        }
        return s;
      },
      current_phase_);
  bt_log(ERROR, "sm", "LE pairing failed: %s. Current pairing phase: %s", bt_str(error),
         phase_status.c_str());
  StopTimer();
  // TODO(fxbug.dev/910): implement "waiting interval" to prevent repeated attempts
  // as described in Vol 3, Part H, 2.3.6.

  ZX_ASSERT(delegate_);
  delegate_->OnPairingComplete(fitx::error(error));

  auto requests = std::move(request_queue_);
  while (!requests.empty()) {
    requests.front().callback(fitx::error(error), security());
    requests.pop();
  }

  if (SecurityUpgradeInProgress()) {
    ZX_ASSERT(le_link_);
    le_link_->set_ltk(hci_spec::LinkKey());
  }
  ResetState();
  // Reset state before potentially disconnecting link to avoid causing pairing phase to fail twice.
  if (error.is(HostError::kTimedOut)) {
    // Per v5.2 Vol. 3 Part H 3.4, after a pairing timeout "No further SMP commands shall be sent
    // over the L2CAP Security Manager Channel. A new Pairing process shall only be performed when a
    // new physical link has been established."
    bt_log(WARN, "sm", "pairing timed out! disconnecting link");
    sm_chan_->SignalLinkError();
  }
}

void SecurityManagerImpl::StartNewTimer() {
  if (timeout_task_.is_pending()) {
    ZX_ASSERT(timeout_task_.Cancel() == ZX_OK);
  }
  timeout_task_.PostDelayed(async_get_default_dispatcher(), kPairingTimeout);
}

void SecurityManagerImpl::StopTimer() {
  if (timeout_task_.is_pending()) {
    zx_status_t status = timeout_task_.Cancel();
    if (status != ZX_OK) {
      bt_log(TRACE, "sm", "smp: failed to stop timer: %s", zx_status_get_string(status));
    }
  }
}

void SecurityManagerImpl::OnPairingTimeout() {
  std::visit(
      [=](auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<std::unique_ptr<Phase1>, T>) {
          arg->OnFailure(Error(HostError::kTimedOut));
        } else if constexpr (std::is_base_of_v<PairingPhase, T>) {
          arg.OnFailure(Error(HostError::kTimedOut));
        } else {
          ZX_PANIC("cannot timeout when current_phase_ is std::monostate!");
        }
      },
      current_phase_);
}

std::pair<DeviceAddress, DeviceAddress> SecurityManagerImpl::LEPairingAddresses() {
  ZX_ASSERT(SecurityUpgradeInProgress());
  const DeviceAddress *initiator = &le_link_->local_address(),
                      *responder = &le_link_->peer_address();
  if (role_ == Role::kResponder) {
    std::swap(initiator, responder);
  }
  return std::make_pair(*initiator, *responder);
}

void SecurityManagerImpl::OnNewLongTermKey(const LTK& ltk) {
  ltk_ = ltk;
  le_link_->set_ltk(ltk.key());
}

Result<> SecurityManagerImpl::ValidateExistingLocalLtk() {
  Result<> status = fitx::ok();
  if (!ltk_.has_value() || !le_link_->ltk().has_value()) {
    // The LTKs should always be present when this method is called.
    status = fitx::error(Error(HostError::kNotFound));
  } else if (!(*le_link_->ltk() == ltk_->key())) {
    // As only SM should ever change the LE Link encryption key, these two values should always be
    // in sync, i.e. something in the system is acting unreliably if they get out of sync.
    status = fitx::error(Error(HostError::kNotReliable));
  }
  if (status.is_error()) {
    // SM does not own the link, so although the checks above should never fail, disconnecting the
    // link (vs. ASSERTing these checks) is safer against non-SM code potentially touching the key.
    delegate_->OnAuthenticationFailure(ToResult(hci_spec::StatusCode::kPinOrKeyMissing));
    sm_chan_->SignalLinkError();
  }
  return status;
}

std::unique_ptr<SecurityManager> SecurityManager::Create(
    fxl::WeakPtr<hci::LowEnergyConnection> link, fbl::RefPtr<l2cap::Channel> smp,
    IOCapability io_capability, fxl::WeakPtr<Delegate> delegate, BondableMode bondable_mode,
    gap::LESecurityMode security_mode) {
  return std::unique_ptr<SecurityManagerImpl>(
      new SecurityManagerImpl(std::move(link), std::move(smp), io_capability, std::move(delegate),
                              bondable_mode, security_mode));
}

SecurityManager::SecurityManager(BondableMode bondable_mode, gap::LESecurityMode security_mode)
    : bondable_mode_(bondable_mode), security_mode_(security_mode) {}

}  // namespace bt::sm
