// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sm/ecdh_key.h"

#include <zircon/assert.h>

#include <algorithm>
#include <cstddef>
#include <memory>

#include <openssl/ec_key.h>

#include "openssl/base.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/nid.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint256.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace bt {

namespace sm {

std::optional<EcdhKey> EcdhKey::ParseFromPublicKey(sm::PairingPublicKeyParams pub_key) {
  auto new_key = EcdhKey();
  new_key.key_ = EC_KEY_new_by_curve_name(EC_curve_nist2nid("P-256"));
  if (!new_key.key_) {
    return std::nullopt;
  }
  BIGNUM x, y;
  BN_init(&x);
  BN_init(&y);
  BN_le2bn(pub_key.x, sizeof(pub_key.x), &x);
  BN_le2bn(pub_key.y, sizeof(pub_key.y), &y);

  // One potential cause of failure is if pub_key is not a valid ECDH key on the P-256 curve.
  int success = (EC_KEY_set_public_key_affine_coordinates(new_key.key_, &x, &y) == 1);
  BN_free(&x);
  BN_free(&y);
  if (success) {
    return new_key;
  }
  return std::nullopt;
}

EcdhKey& EcdhKey::operator=(EcdhKey&& other) noexcept {
  this->key_ = other.key_;
  other.key_ = nullptr;
  return *this;
}

EcdhKey::EcdhKey(EcdhKey&& other) noexcept : key_(other.key_) { other.key_ = nullptr; }

sm::PairingPublicKeyParams EcdhKey::GetSerializedPublicKey() const {
  sm::PairingPublicKeyParams params;
  BIGNUM x, y;
  BN_init(&x);
  BN_init(&y);
  ZX_ASSERT(EC_POINT_get_affine_coordinates_GFp(
                EC_KEY_get0_group(key_), EC_KEY_get0_public_key(key_), &x, &y, nullptr) == 1);
  ZX_ASSERT(BN_bn2le_padded(params.x, sizeof(params.x), &x) == 1);
  ZX_ASSERT(BN_bn2le_padded(params.y, sizeof(params.y), &y) == 1);
  BN_free(&x);
  BN_free(&y);
  return params;
}

UInt256 EcdhKey::GetPublicKeyX() const {
  BIGNUM x;
  BN_init(&x);
  bool success =
      EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key_), EC_KEY_get0_public_key(key_), &x,
                                          nullptr, nullptr) == 1;
  ZX_ASSERT(success);
  UInt256 out{};
  success = BN_bn2le_padded(out.data(), out.size(), &x) == 1;
  ZX_ASSERT(success);
  BN_free(&x);
  return out;
}

UInt256 EcdhKey::GetPublicKeyY() const {
  BIGNUM y;
  BN_init(&y);
  bool success =
      EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(key_), EC_KEY_get0_public_key(key_),
                                          nullptr, &y, nullptr) == 1;
  ZX_ASSERT(success);
  UInt256 out{};
  success = BN_bn2le_padded(out.data(), out.size(), &y) == 1;
  ZX_ASSERT(success);
  BN_free(&y);
  return out;
}

EcdhKey::EcdhKey() : key_(nullptr) {}

EcdhKey::~EcdhKey() {
  if (key_) {
    EC_KEY_free(key_);
  }
}

LocalEcdhKey::LocalEcdhKey() : EcdhKey(){};

LocalEcdhKey::LocalEcdhKey(LocalEcdhKey&& other) noexcept : EcdhKey(std::move(other)) {}

LocalEcdhKey& LocalEcdhKey::operator=(LocalEcdhKey&& other) noexcept {
  EcdhKey::operator=(std::move(other));
  return *this;
}

std::optional<LocalEcdhKey> LocalEcdhKey::Create() {
  auto new_key = LocalEcdhKey();
  new_key.set_boringssl_key(EC_KEY_new_by_curve_name(EC_curve_nist2nid("P-256")));
  if (!new_key.boringssl_key()) {
    return std::nullopt;
  }
  if (EC_KEY_generate_key(new_key.mut_boringssl_key()) != 1) {
    return std::nullopt;
  }
  return new_key;
}

UInt256 LocalEcdhKey::CalculateDhKey(const EcdhKey& peer_public_key) const {
  UInt256 out{0};
  bool success = ECDH_compute_key(out.data(), out.size(),
                                  EC_KEY_get0_public_key(peer_public_key.boringssl_key()),
                                  boringssl_key(), nullptr) == out.size();
  ZX_ASSERT(success);
  std::reverse(out.begin(), out.end());
  return out;
}

void LocalEcdhKey::SetPrivateKeyForTesting(const UInt256& private_key) {
  BIGNUM pkey;
  BN_init(&pkey);
  BN_le2bn(private_key.data(), private_key.size(), &pkey);
  ZX_ASSERT_MSG(EC_KEY_set_private_key(mut_boringssl_key(), &pkey) == 1,
                "Could not set private key in test");
  BN_free(&pkey);
}

}  // namespace sm
}  // namespace bt
