// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace sm {
namespace {

SecurityLevel SecurityLevelFromLinkKey(hci::LinkKeyType lk_type) {
  switch (lk_type) {
    case hci::LinkKeyType::kDebugCombination:
    case hci::LinkKeyType::kUnauthenticatedCombination192:
    case hci::LinkKeyType::kUnauthenticatedCombination256:
      return SecurityLevel::kEncrypted;
    case hci::LinkKeyType::kAuthenticatedCombination192:
    case hci::LinkKeyType::kAuthenticatedCombination256:
      return SecurityLevel::kAuthenticated;
    default:
      break;
  }
  return SecurityLevel::kNoSecurity;
}

bool IsSecureConnectionsKey(hci::LinkKeyType lk_type) {
  return (lk_type == hci::LinkKeyType::kUnauthenticatedCombination256 ||
          lk_type == hci::LinkKeyType::kAuthenticatedCombination256);
}

}  // namespace

PairingFeatures::PairingFeatures() { std::memset(this, 0, sizeof(*this)); }

PairingFeatures::PairingFeatures(bool initiator, bool sc, PairingMethod method,
                                 uint8_t enc_key_size, KeyDistGenField local_kd,
                                 KeyDistGenField remote_kd)
    : initiator(initiator),
      secure_connections(sc),
      method(method),
      encryption_key_size(enc_key_size),
      local_key_distribution(local_kd),
      remote_key_distribution(remote_kd) {}

const char* LevelToString(SecurityLevel level) {
  switch (level) {
    case SecurityLevel::kEncrypted:
      return "encrypted";
    case SecurityLevel::kAuthenticated:
      return "encrypted (MITM)";
    default:
      break;
  }
  return "not secure";
}

SecurityProperties::SecurityProperties()
    : level_(SecurityLevel::kNoSecurity), enc_key_size_(0u), sc_(false) {}

SecurityProperties::SecurityProperties(SecurityLevel level, size_t enc_key_size,
                                       bool secure_connections)
    : level_(level), enc_key_size_(enc_key_size), sc_(secure_connections) {}

// All BR/EDR link keys, even those from legacy pairing or based on 192-bit EC
// points, are stored in 128 bits, according to Core Spec v5.0, Vol 2, Part H
// Section 3.1 "Key Types."
SecurityProperties::SecurityProperties(hci::LinkKeyType lk_type)
    : SecurityProperties(SecurityLevelFromLinkKey(lk_type),
                         kMaxEncryptionKeySize,
                         IsSecureConnectionsKey(lk_type)) {
  ZX_DEBUG_ASSERT_MSG(
      lk_type != hci::LinkKeyType::kChangedCombination,
      "Can't infer security information from a Changed Combination Key");
}

std::string SecurityProperties::ToString() const {
  return fxl::StringPrintf(
      "[security: %s, key size: %lu, %s]", LevelToString(level()),
      enc_key_size(),
      secure_connections() ? "secure connections" : "legacy authentication");
}

LTK::LTK(const SecurityProperties& security, const hci::LinkKey& key)
    : security_(security), key_(key) {}

Key::Key(const SecurityProperties& security, const common::UInt128& value)
    : security_(security), value_(value) {}

}  // namespace sm
}  // namespace bt
