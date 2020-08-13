// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_

#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt::gap {

// Represents security requirements of a link key.
struct BrEdrSecurityRequirements {
  bool authentication;
  bool secure_connections;

  bool operator==(const BrEdrSecurityRequirements& rhs) const {
    return authentication == rhs.authentication && secure_connections == rhs.secure_connections;
  }
};

// Returns true if a key's security properties satisfy the specified security requirements.
bool SecurityPropertiesMeetRequirements(sm::SecurityProperties properties,
                                        BrEdrSecurityRequirements requirements);

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_
