// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TYPES_H_

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/link_key.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace btlib {
namespace sm {

// Represents the features exchanged during Pairing Phase 1.
struct PairingFeatures final {
  PairingFeatures();
  PairingFeatures(bool initiator, bool sc, PairingMethod method,
                  uint8_t enc_key_size, KeyDistGenField local_kd,
                  KeyDistGenField remote_kd);

  // True if the local device is in the "initiator" role.
  bool initiator;

  // True if LE Secure Connections pairing should be used. Otherwise, LE Legacy
  // Pairing should be used.
  bool secure_connections;

  // Indicates the key generation model used for Phase 2.
  PairingMethod method;

  // The negotiated encryption key size.
  uint8_t encryption_key_size;

  // The keys that we must distribute to the peer.
  KeyDistGenField local_key_distribution;

  // The keys that will be distributed to us by the peer.
  KeyDistGenField remote_key_distribution;
};

enum class SecurityLevel {
  // No encryption
  kNoSecurity = 0,

  // Encrypted without MITM protection (unauthenticated)
  kEncrypted = 1,

  // Encrypted with MITM protection (authenticated)
  kAuthenticated = 2,
};

// Returns a string representation of |level| for debug messages.
const char* LevelToString(SecurityLevel level);

// Represents the security properties of a key. The security properties of a
// connection's LTK defines the security properties of the connection.
class SecurityProperties final {
 public:
  SecurityProperties();
  SecurityProperties(SecurityLevel level, size_t enc_key_size,
                     bool secure_connections);

  // Build from a BR/EDR Link Key that resulted from pairing. |lk_type| should
  // not be kChangedCombination, because that means that the link key is the
  // same type as before it was changed.
  //
  // Legacy pairing keys will be considered to have security level kNoSecurity
  // because legacy pairing is superceded by Secure Simple Pairing in Core Spec
  // v2.1 + EDR in 2007. Backwards compatiblity is optional per v5.0, Vol 3,
  // Part C, Section 5. Furthermore, the last Core Spec with only legacy pairing
  // (v2.0 + EDR) is to be withdrawn by Bluetooth SIG on 2019-01-28.
  explicit SecurityProperties(hci::LinkKeyType lk_type);

  SecurityLevel level() const { return level_; }
  size_t enc_key_size() const { return enc_key_size_; }
  bool secure_connections() const { return sc_; }
  bool authenticated() const { return level_ == SecurityLevel::kAuthenticated; }

  // Returns a string representation of these properties.
  std::string ToString() const;

  // Compare two properties for equality.
  bool operator==(const SecurityProperties& other) const {
    return level_ == other.level_ && enc_key_size_ == other.enc_key_size_ &&
           sc_ == other.sc_;
  }

  bool operator!=(const SecurityProperties& other) const {
    return !(*this == other);
  }

 private:
  SecurityLevel level_;
  size_t enc_key_size_;
  bool sc_;
};

// Represents a reusable long term key for a specific transport.
class LTK final {
 public:
  LTK() = default;
  LTK(const SecurityProperties& security, const hci::LinkKey& key);

  const SecurityProperties& security() const { return security_; }
  const hci::LinkKey& key() const { return key_; }

  bool operator==(const LTK& other) const {
    return security() == other.security() && key() == other.key();
  }

 private:
  SecurityProperties security_;
  hci::LinkKey key_;
};

// Represents a 128-bit key.
class Key final {
 public:
  Key() = default;
  Key(const SecurityProperties& security, const common::UInt128& value);

  const SecurityProperties& security() const { return security_; }
  const common::UInt128& value() const { return value_; }

  bool operator==(const Key& other) const {
    return security() == other.security() && value() == other.value();
  }

 private:
  SecurityProperties security_;
  common::UInt128 value_;
};

// Container for LE pairing data.
struct PairingData final {
  // The identity address.
  std::optional<common::DeviceAddress> identity_address;

  // The long term key used for link encryption.
  std::optional<sm::LTK> ltk;

  // The identity resolving key used to resolve RPAs to |identity|.
  std::optional<sm::Key> irk;

  // The connection signature resolving key used in LE security mode 2.
  std::optional<sm::Key> csrk;

  bool operator==(const PairingData& other) const {
    return identity_address == other.identity_address && ltk == other.ltk &&
           irk == other.irk && csrk == other.csrk;
  }
};

}  // namespace sm
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_TYPES_H_
