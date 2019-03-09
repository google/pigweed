// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_DISCOVERY_FILTER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_DISCOVERY_FILTER_H_

#include <string>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"

namespace btlib {

namespace common {
class ByteBuffer;
}  // namespace common

namespace gap {

class RemoteDevice;

// A DiscoveryFilter allows clients of discovery procedures to filter results
// based on certain parameters, such as service UUIDs that might be present in
// EIR or advertising data, or based on available proximity information, to name
// a few.
class DiscoveryFilter final {
 public:
  DiscoveryFilter() = default;

  // Discovery filter based on the "Flags" bit field in LE Advertising Data. If
  // |require_all| is true, then The filter is considered satisifed if ALL of
  // the bits set in |flags_bitfield| are present in the advertisement.
  // Otherwise, the filter is considered satisfied as long as one of the bits
  // set in |flags_bitfield| is present.
  void set_flags(uint8_t flags_bitfield, bool require_all = false) {
    flags_ = flags_bitfield;
    all_flags_required_ = require_all;
  }

  // Discovery filter based on whether or not a device is connectable.
  void set_connectable(bool connectable) { connectable_ = connectable; }

  // Sets the service UUIDs that should be present in a scan result. A scan
  // result satisfies this filter if it provides at least one of the provided
  // UUIDs.
  //
  // Passing an empty value for |service_uuids| effectively disables this
  // filter.
  void set_service_uuids(const std::vector<common::UUID>& service_uuids) {
    service_uuids_ = service_uuids;
  }

  // Sets a string to be matched against the device name. A scan result
  // satisifes this filter if part of the complete or shortened device name
  // fields matches |name_substring|.
  //
  // Passing an empty value for |name_substring| effectively disables this
  // filter.
  void set_name_substring(const std::string& name_substring) {
    name_substring_ = name_substring;
  }

  // Sets a device to be filtered by the pathloss (in dBm) of the radio wave.
  // This value is calculated using the received signal strength (measured
  // locally) and the transmission power of the signal (as reported by the
  // remote):
  //
  //   Path Loss = RSSI - Tx Power
  //
  // If this filter parameter has been set and the pathloss value calculated for
  // a device greater than the provided |pathloss| value, then the scan result
  // will fail to satisfy this filter.
  //
  // If this filter parameter has been set and the pathloss value cannot be
  // calculated because the remote device did not report its transmission power,
  // then the device will fail to satisfy this filter UNLESS an RSSI filter
  // parameter has been set via SetRSSI() that is satisfied by the scan result.
  void set_pathloss(int8_t pathloss) { pathloss_ = pathloss; }

  // Sets a device to be filtered by RSSI. While this can produce inaccurate
  // results when used alone to approximate proximity, it can still be useful
  // when the scan results do not provide the remote device's Transmission
  // Power.
  //
  // A remote device is considered to satisfy this filter parameter if the RSSI
  // of the received transmission is greater than or equal to |rssi|, except if
  // a path loss filter was provided via SetPathLoss() which the remote device
  // failed to satisfy (see comments on SetPathLoss()).
  void set_rssi(int8_t rssi) { rssi_ = rssi; }

  // Sets a device to be filtered by manufacturer specific data. A scan result
  // satisfies this filter if it advertises manufacturer specific data
  // containing |manufacturer_code|.
  void set_manufacturer_code(uint16_t manufacturer_code) {
    manufacturer_code_ = manufacturer_code;
  }

  // Sets this filter up for the "General Discovery" procedure.
  void SetGeneralDiscoveryFlags();

  // Returns true, if the given LE scan result satisfies this filter. Otherwise
  // returns false. |advertising_data| should include scan response data, if
  // any.
  bool MatchLowEnergyResult(const common::ByteBuffer& advertising_data,
                            bool connectable, int8_t rssi) const;

  // Clears all the fields of this filter.
  void Reset();

 private:
  std::vector<common::UUID> service_uuids_;
  std::string name_substring_;
  std::optional<uint8_t> flags_;
  bool all_flags_required_;
  std::optional<bool> connectable_;
  std::optional<uint16_t> manufacturer_code_;
  std::optional<int8_t> pathloss_;
  std::optional<int8_t> rssi_;
};

}  // namespace gap
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_DISCOVERY_FILTER_H_
