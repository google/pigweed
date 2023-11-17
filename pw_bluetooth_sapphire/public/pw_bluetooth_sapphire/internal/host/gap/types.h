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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_

#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::gap {

// Represents security requirements of a link key.
struct BrEdrSecurityRequirements {
  bool authentication;
  bool secure_connections;

  bool operator==(const BrEdrSecurityRequirements& rhs) const {
    return authentication == rhs.authentication &&
           secure_connections == rhs.secure_connections;
  }
};

// Returns true if a key's security properties satisfy the specified security
// requirements.
bool SecurityPropertiesMeetRequirements(sm::SecurityProperties properties,
                                        BrEdrSecurityRequirements requirements);

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_TYPES_H_
