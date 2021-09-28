// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_

#include <string>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"

namespace bt::hci {

// Constructs a DeviceAddress structure from the contents of the given
// advertising report. Returns false if the report contain an invalid value.
// The address will be returned in the |out_address| parameter. The value of
// |out_resolved| will indicate whether or not this address corresponds to a
// resolved RPA (Vol 2, Part E, 7.7.65.2).
bool DeviceAddressFromAdvReport(const hci_spec::LEAdvertisingReportData& report,
                                DeviceAddress* out_address, bool* out_resolved);

// Convert HCI LE device address type to our stack type.
DeviceAddress::Type AddressTypeFromHCI(hci_spec::LEAddressType type);
DeviceAddress::Type AddressTypeFromHCI(hci_spec::LEPeerAddressType type);

// Convert our stack LE address type to HCI type. |type| cannot be kBREDR.
hci_spec::LEAddressType AddressTypeToHCI(DeviceAddress::Type type);

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
