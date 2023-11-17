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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_DEFAULTS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_DEFAULTS_H_

#include <cstdint>

// This file contains default values that are listed in the HCI specification
// for certain commands. These are informational and for testing purposes only;
// each higher-layer library defines its own defaults.

namespace bt::hci_spec::defaults {

// 50 ms
constexpr uint16_t kLEConnectionIntervalMin = 0x0028;

// 70 ms
constexpr uint16_t kLEConnectionIntervalMax = 0x0038;

// 60 ms
constexpr uint16_t kLEScanInterval = 0x0060;

// 30 ms
constexpr uint16_t kLEScanWindow = 0x0030;

// 420 ms
constexpr uint16_t kLESupervisionTimeout = 0x002A;

}  // namespace bt::hci_spec::defaults

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_DEFAULTS_H_
