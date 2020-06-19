// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_3_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_3_H_

#include <lib/async/cpp/task.h>
#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

// Represents Phase 3 of SM pairing. In this phase, the keys the devices agreed to distribute
// during Phase 1 are exchanged. Phase 3 must take place on an already-encrypted link.
//
// THREAD SAFETY
//
// This class is not thread safe and is meant to be accessed on the thread it
// was created on. All callbacks will be run by the default dispatcher of a
// Phase3's creation thread.

using Phase3CompleteCallback = fit::function<void(PairingData)>;

class Phase3 final : public PairingPhase, public PairingChannelHandler {
 public:
  // Initializes Phase3 with the following parameters:
  //   - |chan|: The L2CAP SMP fixed channel.
  //   - |role|: The local device's HCI role.
  //   - |listener|: The current Phase's listener.
  //   - |io_capability|: The local I/O capability.
  //   - |features|: The features that determine pairing, negotiated during Phase 1. There must be
  //     some keys to distribute if Phase3 exists - construction will panic if both the local &
  //     remote key_distribution fields of features are 0.
  //   - |le_sec|: The current security properties of key encrypting the link.
  Phase3(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
         PairingFeatures features, SecurityProperties le_sec, Phase3CompleteCallback on_complete);
  ~Phase3() override = default;

  // Performs the key distribution phase of pairing.
  void Start() final;

 private:
  // Called when the Encryption Info (i.e. the LTK) is received from the peer.
  void OnEncryptionInformation(const EncryptionInformationParams& ltk);

  // Called when EDiv and Rand values are received from the peer.
  void OnMasterIdentification(const MasterIdentificationParams& params);

  // Called when the "Identity Resolving Key" is received from the peer.
  void OnIdentityInformation(const IRK& irk);

  // Called when the "Identity Address" is received from the peer.
  void OnIdentityAddressInformation(const IdentityAddressInformationParams& params);

  // Called whenever a complete key is received from the peer.
  void OnExpectedKeyReceived();

  // Called to send all agreed upon keys to the peer during Phase 3. Returns false if an error
  // occurs and pairing should be aborted.
  bool SendLocalKeys();

  // Only used during legacy pairing. Returns false if the command cannot be sent.
  bool SendEncryptionKey();

  // Sends the identity information. Returns false if the command cannot be sent.
  bool SendIdentityInfo();

  // Called to collect all pairing keys and notify the callback that we are complete
  void SignalComplete();

  // l2cap::Channel callbacks:
  void OnChannelClosed() final { PairingPhase::HandleChannelClosed(); };
  void OnRxBFrame(ByteBufferPtr sdu) final;

  // True if all keys that are expected from the remote have been received.
  bool RequestedKeysObtained() const;

  // True if all local keys that were agreed to be distributed have been sent to the peer.
  bool LocalKeysSent() const;

  bool KeyExchangeComplete() const { return RequestedKeysObtained() && LocalKeysSent(); }
  bool ShouldReceiveLtk() const;       // True if peer should send the LTK
  bool ShouldReceiveIdentity() const;  // True if peer should send identity
  bool ShouldSendLtk() const;          // True if we should send the LTK
  bool ShouldSendIdentity() const;     // True if we should send identity info

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() final {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const PairingFeatures features_;

  // Current security properties of the LE-U link.
  const SecurityProperties le_sec_;

  // The remote keys that have been obtained so far.
  KeyDistGenField obtained_remote_keys_;

  // True if all the local keys in features_ have been sent to the peer.
  bool sent_local_keys_;

  // Generated and distributed if the EncKey bit of the local device's KeyDistGenField is set.
  std::optional<LTK> local_ltk_;

  // Data from the peer tracked during Phase 3. Parts of the LTK are received in separate events.
  // The LTK is only received in Legacy pairing.
  std::optional<UInt128> peer_ltk_bytes_;  // LTK without ediv/rand.
  std::optional<LTK> peer_ltk_;            // Full LTK with ediv/rand
  std::optional<IRK> irk_;
  std::optional<DeviceAddress> identity_address_;

  const Phase3CompleteCallback on_complete_;

  fxl::WeakPtrFactory<Phase3> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Phase3);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PHASE_3_H_
