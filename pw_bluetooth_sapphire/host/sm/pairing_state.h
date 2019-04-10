// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_

#include <memory>
#include <queue>

#include "src/connectivity/bluetooth/core/bt-host/hci/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/bearer.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace sm {

// Represents the pairing state of a connected peer. The peer device must be a
// LE or a BR/EDR/LE device.
class PairingState final : public Bearer::Listener {
 public:
  // Delegate interface for pairing and bonding events.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called to obtain a Temporary Key during legacy pairing. This should
    // return a TK by invoking the |tk_response| parameter.
    //
    // If the delegate fails to obtain the TK, it should signal this by setting
    // the |success| parameter to false. This will abort the pairing procedure.
    //
    // The delegate should use the following algorithm to provide a temporary
    // key:
    //
    //   1. If |method| is kJustWorks, the |tk| should be set to 0. Depending on
    //   the I/O capabilities the user should be asked to confirm or reject the
    //   pairing.
    //
    //   2. If |method| is kPasskeyEntryDisplay, the |tk| should be set to a
    //   random integer between 0 and 999,999. This should be displayed to the
    //   user until the user has finished entering the passkey on the peer
    //   device.
    //   TODO(armansito): Notify the delegate on SMP keypress events to
    //   automatically dismiss the dialog.
    //
    //   3. If |method| is kPasskeyEntryInput, the user should be prompted to
    //   enter a 6-digit passkey. The |tk| should be set to this passkey.
    using TkResponse = fit::function<void(bool success, uint32_t tk)>;
    virtual void OnTemporaryKeyRequest(PairingMethod method,
                                       TkResponse response) = 0;

    // Called when an ongoing pairing is completed with the given |status|.
    virtual void OnPairingComplete(Status status) = 0;

    // Called when new pairing data has been obtained for this peer.
    virtual void OnNewPairingData(const PairingData& data) = 0;

    // Called when the link layer authentication procedure fails. This likely
    // indicates that the LTK or STK used to encrypt the connection was rejected
    // by the peer device.
    //
    // The underlying link will disconnect after this callback runs.
    virtual void OnAuthenticationFailure(hci::Status status) = 0;

    // Called when the security properties of the link change.
    virtual void OnNewSecurityProperties(const SecurityProperties& sec) = 0;
  };

  // |link|: The LE logical link over which pairing procedures occur.
  // |smp|: The L2CAP LE SMP fixed channel that operates over |link|.
  // |ioc|: The initial I/O capability.
  // |delegate|: Delegate responsible for handling authentication challenges and
  //             storing pairing information.
  PairingState(fxl::WeakPtr<hci::Connection> link,
               fbl::RefPtr<l2cap::Channel> smp, IOCapability io_capability,
               fxl::WeakPtr<Delegate> delegate);
  ~PairingState() override;

  // Returns the current security properties of the LE link.
  const SecurityProperties& security() const { return le_sec_; }

  // Assigns the requested |ltk| to this connection, adopting the security
  // properties of |ltk|. If the local device is the master of the underlying
  // link, then the link layer authentication procedure will be initiated.
  //
  // Returns false if a pairing procedure is in progress when this method is
  // called. If the link layer authentication procedure fails, then the link
  // will be disconnected by the controller (Vol 2, Part E, 7.8.24;
  // hci::Connection guarantees this by severing the link directly).
  //
  // This function is mainly intended to assign an existing LTK to a connection
  // (e.g. from bonding data). This function overwrites any previously assigned
  // LTK.
  bool AssignLongTermKey(const LTK& ltk);

  // TODO(armansito): Add function to register a BR/EDR link and SMP channel.

  // Attempt to raise the security level of a the connection to the desired
  // |level| and notify the result in |callback|.
  //
  // If the desired security properties are already satisfied, this procedure
  // will succeed immediately (|callback| will be run with the current security
  // properties).
  //
  // If a pairing procedure has already been initiated (either by us or the
  // peer), the request will be queued and |callback| will be notified when the
  // procedure completes. If the resulting security level does not satisfy
  // |level|, pairing will be re-initiated.
  //
  // If no pairing is in progress then the local device will initiate pairing.
  //
  // If pairing fails |callback| will be called with a |status| that represents
  // the error.
  using PairingCallback =
      fit::function<void(Status status, const SecurityProperties& sec_props)>;
  void UpgradeSecurity(SecurityLevel level, PairingCallback callback);

  // Assign I/O capabilities. This aborts any ongoing pairing procedure and sets
  // up the I/O capabilities to use for future requests.
  void Reset(IOCapability io_capability);

  // Abort all ongoing pairing procedures and notify pairing callbacks with an
  // error.
  // TODO(armansito): Add a "pairing canceled" callback to notify the pairing
  // delegate so that it can dismiss any user challenge.
  void Abort();

 private:
  static constexpr size_t kPairingRequestSize =
      sizeof(Header) + sizeof(PairingRequestParams);

  // Represents the state for LE legacy pairing.
  struct LegacyState final {
    // |id| uniquely identifies the pairing procedure that this state object is
    // tied to. This is generated by PairingState.
    explicit LegacyState(uint64_t id);

    // We are in Phase 1 if pairing features have not been obtained.
    bool InPhase1() const;

    // We are in Phase 2 if we have obtained the TK and waiting for STK
    // encryption.
    bool InPhase2() const;

    // We are in Phase 3 if the link is encrypted with the STK but not all
    // requested keys have been obtained.
    bool InPhase3() const;

    bool IsComplete() const;

    // True if we are in the beginning of Phase 2 where we have not obtained the
    // TK yet.
    bool WaitingForTK() const;

    // True if all keys that are expected from the remote have been received.
    bool RequestedKeysObtained() const;

    // True if all local keys that were agreed to be distributed have been sent
    // to the peer.
    bool LocalKeysSent() const;

    bool KeyExchangeComplete() const {
      return RequestedKeysObtained() && LocalKeysSent();
    }

    bool ShouldReceiveLTK() const;       // True if peer should send the LTK
    bool ShouldReceiveIdentity() const;  // True if peer should send identity
    bool ShouldSendLTK() const;          // True if we should send the LTK
    bool ShouldSendIdentity() const;     // True if we should send identity info

    // True if LTK will be exchanged and the link is yet to be encrypted with
    // the LTK.
    bool WaitingForEncryptionWithLTK() const;

    // A unique token for this pairing state.
    uint64_t id;

    // The pairing features obtained during Phase 1. If invalid, we're in
    // Phase 1. Otherwise, we're in Phase 2.
    std::optional<PairingFeatures> features;

    // True if the link has been encrypted with the STK. This means that we're
    // in Phase 3. Otherwise we're in Phase 1 or 2.
    bool stk_encrypted;

    // True if the link has been encrypted with the LTK. If the LTK should be
    // exchanged, then pairing is considered complete when the link is
    // encrypted with the LTK.
    bool ltk_encrypted;

    // The remote keys that have been obtained so far.
    KeyDistGenField obtained_remote_keys;

    // True, if all local keys have been sent to the peer.
    bool sent_local_keys;

    // Data used to generate STK and confirm values in Phase 2.
    bool has_tk;
    bool has_peer_confirm;
    bool has_peer_rand;
    bool sent_local_confirm;
    bool sent_local_rand;
    common::UInt128 tk;
    common::UInt128 local_confirm;
    common::UInt128 peer_confirm;
    common::UInt128 local_rand;
    common::UInt128 peer_rand;
    common::StaticByteBuffer<kPairingRequestSize> preq;
    common::StaticByteBuffer<kPairingRequestSize> pres;

    // Data from the peer tracked during Phase 3. Parts of LTK are received in
    // separate events.
    bool has_ltk;
    bool has_irk;
    common::UInt128 ltk_bytes;  // LTK without ediv/rand
    common::UInt128 irk;
    common::DeviceAddress identity_address;
  };

  // Represents a pending request to update the security level.
  struct PendingRequest {
    PendingRequest(SecurityLevel level, PairingCallback callback);
    PendingRequest(PendingRequest&&) = default;
    PendingRequest& operator=(PendingRequest&&) = default;

    SecurityLevel level;
    PairingCallback callback;
  };

  // Assign the current security properties and notify the delegate of the
  // change.
  void SetSecurityProperties(const SecurityProperties& sec);

  // Aborts an ongoing legacy pairing procedure.
  void AbortLegacyPairing(ErrorCode error_code);

  // Begin Phase 1 of LE legacy pairing with the given |level|.
  void BeginLegacyPairingPhase1(SecurityLevel level);

  // Begin Phase 2 of LE legacy pairing. This is called after LE pairing
  // features have been exchanged and results (asynchronously) in the generation
  // and encryption of a link using the STK. This follows (roughly) the
  // following steps:
  //    1. Asynchronously obtain the TK.
  //    2. Generate the local confirm/rand values.
  //    3. If initiator, start the exchange, otherwise wait for the peer to send
  //    its confirm value.
  void BeginLegacyPairingPhase2(const common::ByteBuffer& preq,
                                const common::ByteBuffer& pres);
  void LegacySendConfirmValue();
  void LegacySendRandomValue();

  // Called when the link is encrypted with the STK at the end of Legacy
  // Pairing Phase 2.
  void EndLegacyPairingPhase2();

  // Called to send all agreed upon keys to the peer during Phase 3.
  void SendLocalKeys();

  // Completes the legacy pairing process by cleaning up pairing state, updates
  // the current security level, and notifies parties that have requested
  // security.
  void CompleteLegacyPairing();

  // Directly assigns the current |ltk_| and the underlying |le_link_|'s link
  // key. This function does not initiation link layer encryption and can be
  // called during and outside of pairing.
  void AssignLongTermKeyInternal(const LTK& ltk);

  // Bearer::Listener overrides:
  void OnPairingFailed(Status status) override;
  void OnFeatureExchange(const PairingFeatures& features,
                         const common::ByteBuffer& preq,
                         const common::ByteBuffer& pres) override;
  void OnPairingConfirm(const common::UInt128& confirm) override;
  void OnPairingRandom(const common::UInt128& random) override;
  void OnLongTermKey(const common::UInt128& ltk) override;
  void OnMasterIdentification(uint16_t ediv, uint64_t random) override;
  void OnIdentityResolvingKey(const common::UInt128& irk) override;
  void OnIdentityAddress(const common::DeviceAddress& address) override;
  void OnSecurityRequest(AuthReqField auth_req) override;

  // Called when the encryption state of the LE link changes.
  void OnEncryptionChange(hci::Status status, bool enabled);

  // Called when an expected key was received from the peer during Phase 3 of
  // legacy pairing. Completes the ongoing pairing procedure if all expected
  // keys have been received. If a LTK was obtained then the link is encrypted
  // before pairing is complete.
  void OnExpectedKeyReceived();

  // Returns pointers to the initiator and responder addresses. This can be
  // called after pairing Phase 1.
  void LEPairingAddresses(const common::DeviceAddress** out_initiator,
                          const common::DeviceAddress** out_responder);

  // The ID that will be assigned to the next pairing state.
  unsigned int next_pairing_id_;

  fxl::WeakPtr<Delegate> delegate_;

  // Data for the currently registered LE-U link, if any.
  fxl::WeakPtr<hci::Connection> le_link_;
  std::unique_ptr<Bearer> le_smp_;  // SMP data bearer for the LE-U link.
  SecurityProperties le_sec_;  // Current security properties of the LE-U link.

  // The current LTK assigned to this connection. This can be assigned directly
  // by calling AssignLongTermKey() or as a result of a pairing procedure.
  std::optional<LTK> ltk_;

  // The state of the LE legacy pairing procedure, if any.
  std::unique_ptr<LegacyState> legacy_state_;

  // The pending security requests added via UpgradeSecurity().
  std::queue<PendingRequest> request_queue_;

  // TODO(armansito): Support SMP over ACL-U for LE Secure Connections.

  fxl::WeakPtrFactory<PairingState> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingState);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_STATE_H_
