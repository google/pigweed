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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gap/pairing_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/sm/error.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"

namespace bt::sm {

// Delegate interface for pairing and bonding events.
class Delegate {
 public:
  virtual ~Delegate() = default;

  using ConfirmCallback = fit::callback<void(bool confirm)>;
  // Asks higher-level protocols outside bt-host to confirm the pairing request
  // from the device.
  virtual void ConfirmPairing(ConfirmCallback confirm) = 0;

  using DisplayMethod = gap::PairingDelegate::DisplayMethod;
  // Show the user the 6-digit |passkey| that should be compared to the peer's
  // passkey or entered into the peer. |confirm| may be called to accept a
  // comparison or to reject the pairing.
  virtual void DisplayPasskey(uint32_t passkey,
                              DisplayMethod method,
                              ConfirmCallback confirm) = 0;

  // Ask the user to enter a 6-digit passkey or reject pairing. Reports the
  // result by invoking |respond| with |passkey| - a negative value of |passkey|
  // indicates entry failed.
  using PasskeyResponseCallback = fit::callback<void(int64_t passkey)>;
  virtual void RequestPasskey(PasskeyResponseCallback respond) = 0;

  // Called to obtain the local identity information to distribute to the
  // peer. The delegate should return std::nullopt if there is no identity
  // information to share. Otherwise, the delegate should return the IRK and
  // the identity address to distribute.
  virtual std::optional<IdentityInfo> OnIdentityInformationRequest() = 0;

  // Called when an ongoing pairing is completed with the given |status|.
  virtual void OnPairingComplete(Result<> status) = 0;

  // Called when new pairing data has been obtained for this peer.
  virtual void OnNewPairingData(const PairingData& data) = 0;

  // Called when the link layer authentication procedure fails. This likely
  // indicates that the LTK or STK used to encrypt the connection was rejected
  // by the peer device.
  //
  // The underlying link should disconnect after this callback runs.
  virtual void OnAuthenticationFailure(hci::Result<> status) = 0;

  // Called when the security properties of the link change.
  virtual void OnNewSecurityProperties(const SecurityProperties& sec) = 0;

  using WeakPtr = WeakSelf<Delegate>::WeakPtr;
};

}  // namespace bt::sm
