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

#include "pw_bluetooth_sapphire/internal/host/sm/ecdh_key.h"

#include <pw_assert/check.h>

#include <algorithm>
#include <cstddef>
#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/uint256.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"
#include "pw_span/span.h"

namespace bt::sm {

std::optional<EcdhKey> EcdhKey::ParseFromPublicKey(
    sm::PairingPublicKeyParams pub_key) {
  auto new_key =
      pw::crypto::ecdh::P256PublicKey::Import(pw::as_bytes(pw::span(pub_key.x)),
                                              pw::as_bytes(pw::span(pub_key.y)),
                                              pw::endian::little);
  if (!new_key.ok()) {
    return std::nullopt;
  }
  return EcdhKey(std::move(*new_key));
}

EcdhKey& EcdhKey::operator=(EcdhKey&& other) noexcept = default;
EcdhKey::EcdhKey(EcdhKey&& other) noexcept = default;

sm::PairingPublicKeyParams EcdhKey::GetSerializedPublicKey() const {
  sm::PairingPublicKeyParams params;

  PW_CHECK_OK(
      key_.GetX(pw::as_writable_bytes(pw::span(params.x)), pw::endian::little));
  PW_CHECK_OK(
      key_.GetY(pw::as_writable_bytes(pw::span(params.y)), pw::endian::little));

  return params;
}

UInt256 EcdhKey::GetPublicKeyX() const {
  UInt256 out{};
  PW_CHECK_OK(
      key_.GetX(pw::as_writable_bytes(pw::span(out)), pw::endian::little));
  return out;
}

UInt256 EcdhKey::GetPublicKeyY() const {
  UInt256 out{};
  PW_CHECK_OK(
      key_.GetY(pw::as_writable_bytes(pw::span(out)), pw::endian::little));
  return out;
}

EcdhKey::EcdhKey(pw::crypto::ecdh::P256PublicKey&& key)
    : key_(std::move(key)) {}

LocalEcdhKey::LocalEcdhKey(pw::crypto::ecdh::P256Keypair key)
    : key_(std::move(key)) {}

LocalEcdhKey::LocalEcdhKey(LocalEcdhKey&& other) noexcept = default;
LocalEcdhKey& LocalEcdhKey::operator=(LocalEcdhKey&& other) noexcept = default;

std::optional<LocalEcdhKey> LocalEcdhKey::Create() {
  auto new_key = pw::crypto::ecdh::P256Keypair::Generate();
  if (!new_key.ok()) {
    return std::nullopt;
  }
  return LocalEcdhKey(std::move(*new_key));
}

std::optional<LocalEcdhKey> LocalEcdhKey::CreateForTesting(
    const UInt256& private_key, const UInt256& x, const UInt256& y) {
  auto new_key = pw::crypto::ecdh::P256Keypair::ImportForTesting(
      pw::as_bytes(pw::span(private_key)),
      pw::as_bytes(pw::span(x)),
      pw::as_bytes(pw::span(y)),
      pw::endian::little);
  if (!new_key.ok()) {
    return std::nullopt;
  }
  return LocalEcdhKey(std::move(*new_key));
}

UInt256 LocalEcdhKey::CalculateDhKey(const EcdhKey& peer_public_key) const {
  UInt256 out{0};
  PW_CHECK_OK(key_.ComputeDiffieHellman(peer_public_key.key_,
                                        pw::as_writable_bytes(pw::span(out))));
  std::reverse(out.begin(), out.end());
  return out;
}

sm::PairingPublicKeyParams LocalEcdhKey::GetSerializedPublicKey() const {
  sm::PairingPublicKeyParams params;

  PW_CHECK_OK(
      key_.GetX(pw::as_writable_bytes(pw::span(params.x)), pw::endian::little));
  PW_CHECK_OK(
      key_.GetY(pw::as_writable_bytes(pw::span(params.y)), pw::endian::little));

  return params;
}

UInt256 LocalEcdhKey::GetPublicKeyX() const {
  UInt256 out{};
  PW_CHECK_OK(
      key_.GetX(pw::as_writable_bytes(pw::span(out)), pw::endian::little));
  return out;
}

UInt256 LocalEcdhKey::GetPublicKeyY() const {
  UInt256 out{};
  PW_CHECK_OK(
      key_.GetY(pw::as_writable_bytes(pw::span(out)), pw::endian::little));
  return out;
}

}  // namespace bt::sm
