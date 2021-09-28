// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LINK_KEY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LINK_KEY_H_

#include <cstdint>

#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"

namespace bt::hci_spec {

// Represents a key used to encrypt a link.
class LinkKey final {
 public:
  LinkKey();
  LinkKey(const UInt128& value, uint64_t rand, uint16_t ediv);

  // 128-bit BR/EDR link key, LE Long Term Key, or LE Short Term key.
  const UInt128& value() const { return value_; }

  // Encrypted diversifier and random values used to identify the LTK. These
  // values are set to 0 for the LE Legacy STK, LE Secure Connections LTK, and
  // BR/EDR Link Key.
  uint64_t rand() const { return rand_; }
  uint16_t ediv() const { return ediv_; }

  bool operator==(const LinkKey& other) const {
    return value() == other.value() && rand() == other.rand() && ediv() == other.ediv();
  }

 private:
  UInt128 value_;
  uint64_t rand_;
  uint16_t ediv_;
};

}  // namespace bt::hci_spec

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_LINK_KEY_H_
