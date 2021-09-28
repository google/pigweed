// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
