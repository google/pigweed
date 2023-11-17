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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_UTIL_H_

#include <string>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

namespace bt::hci_spec {

using AdvertisingEventBits = uint16_t;

// Helper functions to convert HCI data types to library objects.

// Returns a user-friendly string representation of |version|.
std::string HCIVersionToString(
    pw::bluetooth::emboss::CoreSpecificationVersion version);

// Returns a user-friendly string representation of |status|.
std::string StatusCodeToString(pw::bluetooth::emboss::StatusCode code);

// Returns a user-friendly string representation of |link_type|.
const char* LinkTypeToString(pw::bluetooth::emboss::LinkType link_type);

// Returns a user-friendly string representation of |key_type|.
const char* LinkKeyTypeToString(hci_spec::LinkKeyType key_type);

// Returns a user-friendly string representation of |role|.
std::string ConnectionRoleToString(pw::bluetooth::emboss::ConnectionRole role);

// Convert a LEAdvertisingType's properties (e.g. connectable, scannable,
// directed, etc) to the appropriate advertising event bits for use in
// HCI_LE_Set_Extended_Advertising_Parameters (Core Spec, Volume 4, Part E,
// Section 7.8.53)
std::optional<AdvertisingEventBits> AdvertisingTypeToEventBits(
    pw::bluetooth::emboss::LEAdvertisingType type);

}  // namespace bt::hci_spec

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_UTIL_H_
