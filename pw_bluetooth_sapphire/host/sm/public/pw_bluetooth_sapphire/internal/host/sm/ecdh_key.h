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

#include <cstdint>
#include <optional>

#include "pw_bluetooth_sapphire/internal/host/common/uint256.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_crypto/ecdh.h"

namespace bt::sm {

// Class exposing operations on ECDH keys needed for Secure Connections pairing.
// The only valid operation on a moved-from EcdhKey is to reassign it to an
// existing key.
class EcdhKey final {
 public:
  // Returns a new public key on the P-256 Elliptic Curve parsed from a peer
  // public key, or std::nullopt if the peer key is not a valid point on the
  // curve.
  static std::optional<EcdhKey> ParseFromPublicKey(
      sm::PairingPublicKeyParams pub_key);

  EcdhKey(EcdhKey&& other) noexcept;
  EcdhKey& operator=(EcdhKey&& other) noexcept;

  // Returns a representation of the public key for SMP (Vol. 3 Part H
  // Section 3.5.6).
  sm::PairingPublicKeyParams GetSerializedPublicKey() const;

  // Returns the X/Y value of the public key as std::array wrappers for e.g.
  // comparisons, crypto.
  UInt256 GetPublicKeyX() const;
  UInt256 GetPublicKeyY() const;

 private:
  explicit EcdhKey(pw::crypto::ecdh::P256PublicKey&& key);
  friend class LocalEcdhKey;

  pw::crypto::ecdh::P256PublicKey key_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(EcdhKey);
};

// A local key which contains a private key, allowing for Diffie-Hellman.
class LocalEcdhKey final {
 public:
  // Returns a new random public-private key pair on the P-256 Elliptic Curve
  // used for DH Key exchange in Secure Connections, or std::nullopt if
  // allocation fails.
  static std::optional<LocalEcdhKey> Create();

  // Used to verify correct DHKey calculation with known, non-random private and
  // public keys. Returns std::nullopt if allocation fails or if the point is
  // not on the curve.
  static std::optional<LocalEcdhKey> CreateForTesting(
      const UInt256& private_key, const UInt256& x, const UInt256& y);

  LocalEcdhKey(LocalEcdhKey&& other) noexcept;
  LocalEcdhKey& operator=(LocalEcdhKey&& other) noexcept;

  // Returns the X/Y value of the public key as std::array wrappers for e.g.
  // comparisons, crypto.
  UInt256 GetPublicKeyX() const;
  UInt256 GetPublicKeyY() const;

  // Returns a representation of the public key for SMP (Vol. 3 Part H
  // Section 3.5.6).
  sm::PairingPublicKeyParams GetSerializedPublicKey() const;

  // Returns a 256-bit DHKey calculated from our private key and the peer public
  // key.
  UInt256 CalculateDhKey(const EcdhKey& peer_public_key) const;

 private:
  LocalEcdhKey(pw::crypto::ecdh::P256Keypair key);

  pw::crypto::ecdh::P256Keypair key_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LocalEcdhKey);
};

}  // namespace bt::sm
