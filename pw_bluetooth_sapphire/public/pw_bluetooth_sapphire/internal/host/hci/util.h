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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_

#include <string>

#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

namespace bt::hci {

// Constructs a DeviceAddress structure from the contents of the given
// advertising report. Returns false if the report contain an invalid value.
// The address will be returned in the |out_address| parameter. The value of
// |out_resolved| will indicate whether or not this address corresponds to a
// resolved RPA (Vol 2, Part E, 7.7.65.2).
bool DeviceAddressFromAdvReport(const hci_spec::LEAdvertisingReportData& report,
                                DeviceAddress* out_address,
                                bool* out_resolved);

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_UTIL_H_
