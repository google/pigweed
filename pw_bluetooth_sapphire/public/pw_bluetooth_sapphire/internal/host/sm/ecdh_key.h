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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ECDH_KEY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ECDH_KEY_H_

#include <openssl/base.h>

#include <cstdint>
#include <optional>

#include "pw_bluetooth_sapphire/internal/host/common/uint256.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"

namespace bt::sm {

// Class exposing operations on ECDH keys needed for Secure Connections pairing.
// The only valid operation on a moved-from EcdhKey is to reassign it to an
// existing key.
class EcdhKey {
 public:
  // Returns a new public key on the P-256 Elliptic Curve parsed from a peer
  // public key, or std::nullopt if the peer key is not a valid point on the
  // curve.
  static std::optional<EcdhKey> ParseFromPublicKey(
      sm::PairingPublicKeyParams pub_key);

  EcdhKey(EcdhKey&& other) noexcept;
  EcdhKey& operator=(EcdhKey&& other) noexcept;
  virtual ~EcdhKey();

  // Returns a representation of the public key for SMP (Vol. 3 Part H
  // Section 3.5.6).
  sm::PairingPublicKeyParams GetSerializedPublicKey() const;

  // Returns the X/Y value of the public key as std::array wrappers for e.g.
  // comparisons, crypto.
  UInt256 GetPublicKeyX() const;
  UInt256 GetPublicKeyY() const;

  const EC_KEY* boringssl_key() const { return key_; }

 protected:
  EcdhKey();

  EC_KEY* mut_boringssl_key() { return key_; }
  void set_boringssl_key(EC_KEY* k) { key_ = k; }

 private:
  EC_KEY* key_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(EcdhKey);
};

// Specialization of EcdhKey used to ensure that when calculating the shared DH
// Key between two EcdhKeys, at least one has a private key.
class LocalEcdhKey : public EcdhKey {
 public:
  // Returns a new random public-private key pair on the P-256 Elliptic Curve
  // used for DH Key exchange in Secure Connections, or std::nullopt if
  // allocation fails.
  static std::optional<LocalEcdhKey> Create();

  LocalEcdhKey(LocalEcdhKey&& other) noexcept;
  LocalEcdhKey& operator=(LocalEcdhKey&& other) noexcept;

  // Returns a 256-bit DHKey calculated from our private key and the peer public
  // key.
  UInt256 CalculateDhKey(const EcdhKey& peer_public_key) const;

  // Used to verify correct DHKey calculation with known, non-random private and
  // public keys.
  void SetPrivateKeyForTesting(const UInt256& private_key);

 private:
  LocalEcdhKey();
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LocalEcdhKey);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ECDH_KEY_H_
