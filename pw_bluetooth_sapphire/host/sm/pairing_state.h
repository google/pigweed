// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_

#include <zircon/assert.h>

#include <memory>
#include <queue>
#include <variant>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/idle_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_1.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_legacy.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_2_secure_connections.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/phase_3.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {
namespace sm {

// Represents the pairing state of a connected peer. The peer device must be a
// LE or a BR/EDR/LE device.
class PairingState final : public PairingPhase::Listener {
 public:
  // |link|: The LE logical link over which pairing procedures occur.
  // |smp|: The L2CAP LE SMP fixed channel that operates over |link|.
  // |io_capability|: The initial I/O capability.
  // |delegate|: Delegate which handles SMP interactions with the rest of the Bluetooth stack.
  // |bondable_mode|: the operating bondable mode of the device (see v5.2, Vol. 3, Part C 9.4).
  // |security_mode|: the security mode this PairingState is in (see v5.2, Vol. 3, Part C 10.2).
  PairingState(fxl::WeakPtr<hci::Connection> link, fbl::RefPtr<l2cap::Channel> smp,
               IOCapability io_capability, fxl::WeakPtr<Delegate> delegate,
               BondableMode bondable_mode, gap::LeSecurityMode security_mode);
  ~PairingState() override;

  // Returns the current security properties of the LE link.
  const SecurityProperties& security() const { return le_sec_; }

  // Assigns the requested |ltk| to this connection, adopting the security properties of |ltk|. If
  // the local device is the master of the underlying link, then the link layer authentication
  // procedure will be initiated.
  //
  // Returns false if a pairing procedure is in progress when this method is called. If the link
  // layer authentication procedure fails, then the link will be disconnected by the controller
  // (Vol 2, Part E, 7.8.24; hci::Connection guarantees this by severing the link directly).
  //
  // This function is mainly intended to assign an existing LTK to a connection (e.g. from bonding
  // data). This function overwrites any previously assigned LTK.
  bool AssignLongTermKey(const LTK& ltk);

  // TODO(52937): Add function to register a BR/EDR link and SMP channel.

  // Attempt to raise the security level of the connection to the desired |level| and notify the
  // result in |callback|.
  //
  // If the desired security properties are already satisfied, this procedure will succeed
  // immediately (|callback| will be run with the current security properties).
  //
  // If a pairing procedure has already been initiated (either by us or the peer), the request will
  // be queued and |callback| will be notified when the procedure completes. If the resulting
  // security level does not satisfy |level|, pairing will be re-initiated. Note that this means
  // security requests of different |level|s may not complete in the order they are made.
  //
  // If no pairing is in progress then the local device will initiate pairing.
  //
  // If pairing fails |callback| will be called with a |status| that represents the error.
  using PairingCallback = fit::function<void(Status status, const SecurityProperties& sec_props)>;
  void UpgradeSecurity(SecurityLevel level, PairingCallback callback);

  // Assign I/O capabilities. This aborts any ongoing pairing procedure and sets
  // up the I/O capabilities to use for future requests.
  void Reset(IOCapability io_capability);

  // Abort all ongoing pairing procedures and notify pairing callbacks with the provided error.
  void Abort(ErrorCode ecode = ErrorCode::kUnspecifiedReason);

  // Returns whether or not the pairing state is in bondable mode. Note that being in bondable mode
  // does not guarantee that pairing will necessarily bond.
  BondableMode bondable_mode() const { return bondable_mode_; }

  // Sets the bondable mode of the pairing state. Any in-progress pairings will not be affected -
  // if bondable mode needs to be reset during a pairing Reset() or Abort() must be called first.
  void set_bondable_mode(sm::BondableMode mode) { bondable_mode_ = mode; }

  // Sets the LE Security mode of the pairing state - see enum definition for details of each mode.
  // If a security upgrade is in-progress, this will only take effect on the next security upgrade.
  void set_security_mode(gap::LeSecurityMode mode) { security_mode_ = mode; }

 private:
  // Represents a pending request to update the security level.
  struct PendingRequest {
    PendingRequest(SecurityLevel level, PairingCallback callback);
    PendingRequest(PendingRequest&&) = default;
    PendingRequest& operator=(PendingRequest&&) = default;

    SecurityLevel level;
    PairingCallback callback;
  };

  // Puts the pairing state machine back into the idle i.e. non-pairing state.
  void GoToIdlePhase();

  // Called when we receive a peer security request as initiator, will start Phase 1.
  void OnSecurityRequest(AuthReqField auth_req);

  // Called when we receive a peer pairing request as responder, will start Phase 1.
  void OnPairingRequest(const PairingRequestParams& req_params);

  // Pulls the next PendingRequest off |request_queue_| and starts a security upgrade to that
  // |level| by either sending a Pairing Request as initiator or a Security Request as responder.
  void UpgradeSecurityInternal();

  // Called when the feature exchange (Phase 1) completes and the relevant features of both sides
  // have been resolved into `features`. `preq` and `pres` need to be retained for cryptographic
  // calculations in Phase 2. Causes a state transition from Phase 1 to Phase 2
  void OnFeatureExchange(PairingFeatures features, PairingRequestParams preq,
                         PairingResponseParams pres);

  // Called when Phase 2 generates an encryption key, so the link can be encrypted with it.
  void OnPhase2EncryptionKey(const UInt128& new_key);

  // Check if encryption using `current_ltk` will satisfy the current security requirements.
  static bool CurrentLtkInsufficientlySecureForEncryption(std::optional<LTK> current_ltk,
                                                          IdlePhase* idle_phase,
                                                          gap::LeSecurityMode mode);

  // Called when the encryption state of the LE link changes.
  void OnEncryptionChange(hci::Status status, bool enabled);

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
  void OnPairingFailed(Status status) override;
  std::optional<IdentityInfo> OnIdentityRequest() override;
  void ConfirmPairing(ConfirmCallback confirm) override;
  void DisplayPasskey(uint32_t passkey, Delegate::DisplayMethod method,
                      ConfirmCallback cb) override;
  void RequestPasskey(PasskeyResponseCallback respond) override;

  // Starts the SMP timer. Stops and cancels any in-progress timers.
  bool StartNewTimer();
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

  // Validates that both SM and the link have stored LTKs, and that these values match. Disconnects
  // the link if it finds an issue. Should only be called when an LTK is expected to exists.
  Status ValidateExistingLocalLtk();

  // The ID that will be assigned to the next pairing operation.
  PairingProcedureId next_pairing_id_;

  // The higher-level class acting as a delegate for operations outside of SMP.
  fxl::WeakPtr<Delegate> delegate_;

  // Data for the currently registered LE-U link, if any.
  fxl::WeakPtr<hci::Connection> le_link_;

  // The IO capabilities of the device
  IOCapability io_cap_;

  // The operating bondable mode of the device.
  BondableMode bondable_mode_;

  // The current GAP security mode of the device (v5.2 Vol. 3 Part C Section 10.2)
  gap::LeSecurityMode security_mode_;

  SecurityProperties le_sec_;  // Current security properties of the LE-U link.

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

  async::TaskClosureMethod<PairingState, &PairingState::OnPairingTimeout> timeout_task_{this};

  // The presence of a particular phase in this variant indicates that the pairing state machine is
  // currently in that phase. `weak_ptr_factory_` must be the last declared member so that all weak
  // pointers to PairingState are invalidated at the beginning of destruction, but all of the Phase
  // class ctors take a WeakPtr<PairingState>. Thus, we include std::monostate in the variant so
  // `current_phase_` can be default-constructible in the initializer list, and construct an Idle
  // Phase in the PairingState ctor.
  // TODO(fxbug.dev/53946): Clean up usage of PairingPhases, especially re:std::monostate.
  std::variant<std::monostate, IdlePhase, std::unique_ptr<Phase1>, Phase2Legacy,
               Phase2SecureConnections, Phase3>
      current_phase_;

  fxl::WeakPtrFactory<PairingState> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingState);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_
