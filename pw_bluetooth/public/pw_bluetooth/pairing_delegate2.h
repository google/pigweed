// Copyright 2024 The Pigweed Authors
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

#include "pw_async2/dispatcher.h"
#include "pw_bluetooth/internal/raii_ptr.h"
#include "pw_bluetooth/peer.h"

namespace pw::bluetooth {

/// Pairing event handler implemented by the API client.
class PairingDelegate2 {
 public:
  /// Request models an active pairing procedure.
  class Request {
   public:
    enum class KeypressEvent : uint8_t {
      /// The user has entered a single digit.
      kDigitEntered,

      /// The user has erased a single digit.
      kDigitErased,

      /// The user has cleared the entire passkey.
      kPasskeyCleared,

      /// The user has finished entering the passkey.
      kPasskeyEntered
    };

    /// Indicates what type of interaction is required locally.
    enum class Method {
      /// The user is asked to accept or reject pairing.
      /// This is the minimum method - even when both devices do not support
      /// input or output, the delegate will be asked to confirm any pairing
      /// not initiated with user intent.
      kConsent,
      /// The user is shown a 6-digit numerical passkey on this device which
      /// they
      /// must key in on the peer device.
      /// The passkey to be displayed is provided.
      kPasskeyDisplay,
      /// The user is shown a 4-digit numerical pin on this device which they
      /// must key in on the peer device.
      /// The passkey to be displayed is provided.
      kPinDisplay,
      /// The user is shown a 6-digit numerical passkey on this device which
      /// will also be shown on the peer device. The user must compare the
      /// passkeys and accept the pairing if the passkeys match.
      /// The passkey to be displayed is provided.
      kPasskeyConfirmation,
      /// The user is asked to enter a 6-digit passkey on this device which is
      /// communicated via the peer device.
      kPasskeyEntry,
      /// The user is asked to enter a 4-digit pin on this device which is
      /// communicated via the peer device.
      kPinEntry,
    };

    virtual ~Request() = default;

    /// The peer that initiated the pairing request.
    virtual Peer peer() = 0;

    /// Indicates what pairing interaction is required locally.
    virtual Method method() = 0;

    /// If the pairing method requires a passkey to be displayed
    /// (`Method::*Display`, `Method::*Confirmation`), this method returns the
    /// passkey. Returns null otherwise.
    virtual std::optional<uint32_t> passkey() = 0;

    /// Accept the pairing request.
    /// @param entered_passkey Required if `Method::*Entry` is used.
    virtual void Accept(std::optional<uint32_t> entered_passkey) = 0;

    /// Reject the pairing request.
    virtual void Reject() = 0;

    /// Used to communicate local keypresses to update the remote peer on
    /// the progress of the pairing.
    virtual void Keypress(KeypressEvent keypress) = 0;

    /// When the pairing method is passkey_display, can be used to update the UI
    /// to indicate reception of keypresses. Awakens `cx` on the next keypress.
    virtual async2::Poll<KeypressEvent> PendKeypress(async2::Context& cx) = 0;

    /// Ready when the pairing is completed. The `Request` should be
    /// destroyed once pairing is complete. Awakens `cx` on pairing completion.
    ///
    /// @returns
    /// * @OK: Pairing completed successfully.
    /// * @CANCELLED: Pairing was rejected via `Reject()` or the peer cancelled
    ///   the pairing.
    /// * @DEADLINE_EXCEEDED: Pairing timed out.
    /// * @INTERNAL: Pairing failed unexpectedly due to an internal error.
    virtual async2::Poll<pw::Status> PendComplete(async2::Context& cx) = 0;

   private:
    /// Reject the request if it is not complete yet and release resources. This
    /// method is called by the ~Request::Ptr() when it goes out of scope, the
    /// API client should never call this method.
    virtual void Release() = 0;

   public:
    /// Movable Request smart pointer.
    using Ptr = internal::RaiiPtr<Request, &Request::Release>;
  };

  virtual ~PairingDelegate2() = default;

  /// Called when a pairing is started with the a peer. The pairing process is
  /// continued using `request`.
  ///
  /// `Request::method()` indicates how the request should be responded to.
  ///
  /// Multiple requests can be active at one time for different peers.
  /// Destroying `request` will automatically reject the pairing.
  virtual void OnRequest(Request::Ptr&& request) = 0;
};

}  // namespace pw::bluetooth
