// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_

#include <string>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace btlib {
namespace hci {

// Helper functions to convert HCI data types to library objects.

// Returns a user-friendly string representation of |version|.
std::string HCIVersionToString(hci::HCIVersion version);

// Returns a user-friendly string representation of |status|.
std::string StatusCodeToString(hci::StatusCode code);

// Constructs a common::DeviceAddress structure from the contents of the given
// advertising report. Returns false if the report contain an invalid value.
// The address will be returned in the |out_address| parameter. The value of
// |out_resolved| will indicate whether or not this address corresponds to a
// resolved RPA (Vol 2, Part E, 7.7.65.2).
bool DeviceAddressFromAdvReport(const hci::LEAdvertisingReportData& report,
                                common::DeviceAddress* out_address,
                                bool* out_resolved);

// Convert HCI LE device address type to our stack type.
common::DeviceAddress::Type AddressTypeFromHCI(LEAddressType type);
common::DeviceAddress::Type AddressTypeFromHCI(LEPeerAddressType type);

// Convert our stack LE address type to HCI type. |type| cannot be kBREDR.
LEAddressType AddressTypeToHCI(common::DeviceAddress::Type type);

}  // namespace hci
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
