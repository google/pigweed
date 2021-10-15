// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::sm {
namespace {

bool IsEncryptedKey(hci_spec::LinkKeyType lk_type) {
  return (lk_type == hci_spec::LinkKeyType::kDebugCombination ||
          lk_type == hci_spec::LinkKeyType::kUnauthenticatedCombination192 ||
          lk_type == hci_spec::LinkKeyType::kUnauthenticatedCombination256 ||
          lk_type == hci_spec::LinkKeyType::kAuthenticatedCombination192 ||
          lk_type == hci_spec::LinkKeyType::kAuthenticatedCombination256);
}

bool IsAuthenticatedKey(hci_spec::LinkKeyType lk_type) {
  return (lk_type == hci_spec::LinkKeyType::kAuthenticatedCombination192 ||
          lk_type == hci_spec::LinkKeyType::kAuthenticatedCombination256);
}

bool IsSecureConnectionsKey(hci_spec::LinkKeyType lk_type) {
  return (lk_type == hci_spec::LinkKeyType::kUnauthenticatedCombination256 ||
          lk_type == hci_spec::LinkKeyType::kAuthenticatedCombination256);
}

}  // namespace

PairingFeatures::PairingFeatures() { std::memset(this, 0, sizeof(*this)); }

PairingFeatures::PairingFeatures(bool initiator, bool sc, bool will_bond,
                                 std::optional<CrossTransportKeyAlgo> algo, PairingMethod method,
                                 uint8_t enc_key_size, KeyDistGenField local_kd,
                                 KeyDistGenField remote_kd)
    : initiator(initiator),
      secure_connections(sc),
      will_bond(will_bond),
      generate_ct_key(algo),
      method(method),
      encryption_key_size(enc_key_size),
      local_key_distribution(local_kd),
      remote_key_distribution(remote_kd) {}

bool HasKeysToDistribute(PairingFeatures features) {
  return DistributableKeys(features.local_key_distribution) ||
         DistributableKeys(features.remote_key_distribution);
}

const char* LevelToString(SecurityLevel level) {
  switch (level) {
    case SecurityLevel::kEncrypted:
      return "encrypted";
    case SecurityLevel::kAuthenticated:
      return "encrypted (MITM)";
    case SecurityLevel::kSecureAuthenticated:
      return "encrypted (MITM) with Secure Connections and 128-bit key";
    default:
      break;
  }
  return "not secure";
}

SecurityProperties::SecurityProperties() : SecurityProperties(false, false, false, 0u) {}

SecurityProperties::SecurityProperties(bool encrypted, bool authenticated, bool secure_connections,
                                       size_t enc_key_size)
    : properties_(0u), enc_key_size_(enc_key_size) {
  properties_ |= (encrypted ? Property::kEncrypted : 0u);
  properties_ |= (authenticated ? Property::kAuthenticated : 0u);
  properties_ |= (secure_connections ? Property::kSecureConnections : 0u);
}

SecurityProperties::SecurityProperties(SecurityLevel level, size_t enc_key_size,
                                       bool secure_connections)
    : SecurityProperties((level >= SecurityLevel::kEncrypted),
                         (level >= SecurityLevel::kAuthenticated), secure_connections,
                         enc_key_size) {}
// All BR/EDR link keys, even those from legacy pairing or based on 192-bit EC
// points, are stored in 128 bits, according to Core Spec v5.0, Vol 2, Part H
// Section 3.1 "Key Types."
SecurityProperties::SecurityProperties(hci_spec::LinkKeyType lk_type)
    : SecurityProperties(IsEncryptedKey(lk_type), IsAuthenticatedKey(lk_type),
                         IsSecureConnectionsKey(lk_type), kMaxEncryptionKeySize) {
  ZX_DEBUG_ASSERT_MSG(lk_type != hci_spec::LinkKeyType::kChangedCombination,
                      "Can't infer security information from a Changed Combination Key");
}

SecurityLevel SecurityProperties::level() const {
  auto level = SecurityLevel::kNoSecurity;
  if (properties_ & Property::kEncrypted) {
    level = SecurityLevel::kEncrypted;
    if (properties_ & Property::kAuthenticated) {
      level = SecurityLevel::kAuthenticated;
      if (enc_key_size_ == kMaxEncryptionKeySize && (properties_ & Property::kSecureConnections)) {
        level = SecurityLevel::kSecureAuthenticated;
      }
    }
  }
  return level;
}

std::optional<hci_spec::LinkKeyType> SecurityProperties::GetLinkKeyType() const {
  if (level() == SecurityLevel::kNoSecurity) {
    return std::nullopt;
  }
  if (authenticated()) {
    if (secure_connections()) {
      return hci_spec::LinkKeyType::kAuthenticatedCombination256;
    } else {
      return hci_spec::LinkKeyType::kAuthenticatedCombination192;
    }
  } else {
    if (secure_connections()) {
      return hci_spec::LinkKeyType::kUnauthenticatedCombination256;
    } else {
      return hci_spec::LinkKeyType::kUnauthenticatedCombination192;
    }
  }
}

std::string SecurityProperties::ToString() const {
  if (level() == SecurityLevel::kNoSecurity) {
    return "[no security]";
  }
  return bt_lib_cpp_string::StringPrintf(
      "[%s%s%skey size: %lu]", encrypted() ? "encrypted " : "",
      authenticated() ? "authenticated (MITM) " : "",
      secure_connections() ? "secure connections " : "legacy authentication ", enc_key_size());
}

bool SecurityProperties::IsAsSecureAs(const SecurityProperties& other) const {
  // clang-format off
  return
    (encrypted() || !other.encrypted()) &&
    (authenticated() || !other.authenticated()) &&
    (secure_connections() || !other.secure_connections()) &&
    (enc_key_size_ >= other.enc_key_size_);
  // clang-format on
}

LTK::LTK(const SecurityProperties& security, const hci_spec::LinkKey& key)
    : security_(security), key_(key) {}

Key::Key(const SecurityProperties& security, const UInt128& value)
    : security_(security), value_(value) {}

}  // namespace bt::sm
