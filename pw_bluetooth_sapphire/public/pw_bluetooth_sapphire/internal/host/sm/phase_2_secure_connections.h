// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_2_SECURE_CONNECTIONS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_2_SECURE_CONNECTIONS_H_

#include <lib/fit/function.h>

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/common/uint128.h"
#include "pw_bluetooth_sapphire/internal/host/hci/connection.h"
#include "pw_bluetooth_sapphire/internal/host/sm/ecdh_key.h"
#include "pw_bluetooth_sapphire/internal/host/sm/pairing_channel.h"
#include "pw_bluetooth_sapphire/internal/host/sm/pairing_phase.h"
#include "pw_bluetooth_sapphire/internal/host/sm/sc_stage_1.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::sm {
// Phase2SecureConnections encapsulates Phase 2 of LE Secure Connections
// Pairing, which takes care of authentication and shared encryption key
// generation using FIPS approved ECDH key protocols (see V5.1 Vol. 3 Part H
// Section 2.3.5.6). Each Phase2SecureConnections instance generates a new
// public-private ECDH key pair, i.e. each Secure Connections pairing uses a
// unique ECDH key pair.
//
// This class is not thread safe and is meant to be accessed on the thread it
// was created on. All callbacks will be run by the default dispatcher of a
// Phase2SecureConnections's creation thread.
class Phase2SecureConnections final : public PairingPhase,
                                      public PairingChannelHandler {
 public:
  // |chan|, |listener|, and |role|: used to construct the base PairingPhase
  // |features|: features negotiated in Phase 1 of pairing
  // |preq, pres|: Exchanged in Phase 1, these are used to generate the DHKey
  // Check value E. |initiator_addr|, |responder_addr|: 48-bit bd-address of the
  // initiator and responder, used
  //                                     used for cryptographic hashing
  // |cb|: Callback used to notify when the Phase2 has negotiated a new
  // encryption key.
  Phase2SecureConnections(PairingChannel::WeakPtr chan,
                          Listener::WeakPtr listener,
                          Role role,
                          PairingFeatures features,
                          PairingRequestParams preq,
                          PairingResponseParams pres,
                          const DeviceAddress& initiator_addr,
                          const DeviceAddress& responder_addr,
                          OnPhase2KeyGeneratedCallback cb);
  ~Phase2SecureConnections() override { InvalidatePairingChannelHandler(); }

  // Begin Phase 2 of LE Secure Connections pairing. This is called after LE
  // pairing features are exchanged and results (asynchronously) in the
  // generation and encryption of a link using the STK. Follows (roughly) the
  // following steps:
  //    1. ECDH key exchange to prevent passive eavesdropping attacks.
  //    2. Generate local random->confirm values, and then exchange
  //    confirm->random values (in the
  //       order written). The random/confirm value generation depends on the
  //       pairing method used.
  //    3. Generate ECDH key check, and exchange/compare them with the peer to
  //    validate pairing.
  void Start() final;

 private:
  // The devices exchange ECDH Public Keys using the below methods (SMP
  // Section 2.3.5.6.1).
  void SendLocalPublicKey();
  fit::result<ErrorCode> CanReceivePeerPublicKey() const;
  void OnPeerPublicKey(PairingPublicKeyParams peer_pub_key);

  // After exchanging ECDH Public Keys, the devices perform one of four possible
  // authentication protocols to prove who they are to each other in Stage 1.
  // (SMP Section 2.3.5.6.2-3).
  // TODO(fxbug.dev/601): Implement Stage 1 OOB pairing (SMP Section 2.3.6.5.4).
  void StartAuthenticationStage1();
  void OnAuthenticationStage1Complete(
      fit::result<ErrorCode, ScStage1::Output> result);

  void OnPairingConfirm(PairingConfirmValue confirm);
  void OnPairingRandom(PairingRandomValue rand);

  // If Stage 1 completes successfully, this uses the results to perform Stage 2
  // (i.e. computing the LTK and exchanging DHKey, SMP Section 2.3.5.6.5).
  void StartAuthenticationStage2();
  void SendDhKeyCheckE();
  // Receives and stores the peer DH Key check
  void OnDhKeyCheck(PairingDHKeyCheckValueE dh_key_check);
  fit::result<ErrorCode> CanReceiveDhKeyCheck() const;
  // Checks that the received DH key check matches what we calculate locally.
  void ValidatePeerDhKeyCheck();

  // l2cap::Channel callback
  void OnRxBFrame(ByteBufferPtr sdu) final;
  void OnChannelClosed() final { PairingPhase::HandleChannelClosed(); }

  // PairingPhase overrides.
  std::string ToStringInternal() override {
    return bt_lib_cpp_string::StringPrintf(
        "Secure Connections Pairing Phase 2 (encryption key agreement) - "
        "pairing with %s method",
        util::PairingMethodToString(features_.method).c_str());
  }

  bool is_just_works_or_numeric_comparison() const {
    return features_.method == PairingMethod::kJustWorks ||
           features_.method == PairingMethod::kNumericComparison;
  }

  bool is_passkey_entry() const {
    return features_.method == PairingMethod::kPasskeyEntryDisplay ||
           features_.method == PairingMethod::kPasskeyEntryInput;
  }

  bool ecdh_exchange_complete() const {
    return sent_local_ecdh_ && peer_ecdh_.has_value();
  }
  bool stage_1_complete() const { return stage_1_results_.has_value(); }

  bool sent_local_ecdh_;
  std::optional<LocalEcdhKey> local_ecdh_;
  std::optional<EcdhKey> peer_ecdh_;

  // Stage 1 of Secure Connections pairing depends on the method used (Just
  // Works/Numeric Comparison, Passkey Entry, or Out-Of-Band). Each possible
  // method is implemented as a separate class and takes responsibility for the
  // entirety of authentication stage 1.
  std::unique_ptr<ScStage1> stage_1_;
  std::optional<ScStage1::Output> stage_1_results_;

  bool sent_local_dhkey_check_;
  std::optional<PairingDHKeyCheckValueE> local_dhkey_check_ = std::nullopt;
  std::optional<PairingDHKeyCheckValueE> expected_peer_dhkey_check_ =
      std::nullopt;
  std::optional<PairingDHKeyCheckValueE> actual_peer_dhkey_check_ =
      std::nullopt;

  PairingFeatures features_;
  PairingRequestParams preq_;
  PairingResponseParams pres_;
  const DeviceAddress initiator_addr_;
  const DeviceAddress responder_addr_;
  std::optional<UInt128> ltk_;

  WeakSelf<Phase2SecureConnections> weak_self_;

  OnPhase2KeyGeneratedCallback on_ltk_ready_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Phase2SecureConnections);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_2_SECURE_CONNECTIONS_H_
