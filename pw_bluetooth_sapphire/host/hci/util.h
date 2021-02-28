// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_

#include <string>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt::hci {

// Helper functions to convert HCI data types to library objects.

// Returns a user-friendly string representation of |version|.
std::string HCIVersionToString(hci::HCIVersion version);

// Returns a user-friendly string representation of |status|.
std::string StatusCodeToString(hci::StatusCode code);

// Returns a user-friendly string representation of |link_type|.
std::string LinkTypeToString(hci::LinkType link_type);

// Constructs a DeviceAddress structure from the contents of the given
// advertising report. Returns false if the report contain an invalid value.
// The address will be returned in the |out_address| parameter. The value of
// |out_resolved| will indicate whether or not this address corresponds to a
// resolved RPA (Vol 2, Part E, 7.7.65.2).
bool DeviceAddressFromAdvReport(const hci::LEAdvertisingReportData& report,
                                DeviceAddress* out_address, bool* out_resolved);

// Convert HCI LE device address type to our stack type.
DeviceAddress::Type AddressTypeFromHCI(LEAddressType type);
DeviceAddress::Type AddressTypeFromHCI(LEPeerAddressType type);

// Convert our stack LE address type to HCI type. |type| cannot be kBREDR.
LEAddressType AddressTypeToHCI(DeviceAddress::Type type);

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
