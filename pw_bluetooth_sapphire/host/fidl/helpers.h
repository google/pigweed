// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_

#include <fuchsia/bluetooth/control/cpp/fidl.h>
#include <fuchsia/bluetooth/cpp/fidl.h>
#include <fuchsia/bluetooth/gatt/cpp/fidl.h>
#include <fuchsia/bluetooth/host/cpp/fidl.h>
#include <fuchsia/bluetooth/le/cpp/fidl.h>

#include <optional>

#include "lib/fidl/cpp/type_converter.h"
#include "lib/fidl/cpp/vector.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"

// Helpers for implementing the Bluetooth FIDL interfaces.

namespace bt {
namespace gap {

class DiscoveryFilter;

}  // namespace gap
}  // namespace bt

namespace bthost {
namespace fidl_helpers {

// TODO(BT-305): Temporary logic for converting between the stack identifier
// type (integer) and FIDL identifier type (string). Remove these once all FIDL
// interfaces have been converted to use integer IDs.
std::optional<bt::PeerId> PeerIdFromString(const std::string& id);

// Convert a string of the form "XX:XX:XX:XX:XX" to the DeviceAddressBytes it represents.
// returns nullopt when the conversion fails (due to wrong format)
std::optional<bt::DeviceAddressBytes> AddressBytesFromString(const std::string& addr);

// Functions for generating a FIDL bluetooth::Status

fuchsia::bluetooth::ErrorCode HostErrorToFidl(bt::HostError host_error);

fuchsia::bluetooth::Status NewFidlError(fuchsia::bluetooth::ErrorCode error_code,
                                        std::string description);

template <typename ProtocolErrorCode>
fuchsia::bluetooth::Status StatusToFidl(const bt::Status<ProtocolErrorCode>& status,
                                        std::string msg = "") {
  fuchsia::bluetooth::Status fidl_status;
  if (status.is_success()) {
    return fidl_status;
  }

  auto error = fuchsia::bluetooth::Error::New();
  error->error_code = HostErrorToFidl(status.error());
  error->description = msg.empty() ? status.ToString() : std::move(msg);
  if (status.is_protocol_error()) {
    error->protocol_error_code = static_cast<uint32_t>(status.protocol_error());
  }

  fidl_status.error = std::move(error);
  return fidl_status;
}

bt::UUID UuidFromFidl(const fuchsia::bluetooth::Uuid& input);

// Functions that convert FIDL types to library objects.
bt::sm::IOCapability IoCapabilityFromFidl(const fuchsia::bluetooth::control::InputCapabilityType,
                                          const fuchsia::bluetooth::control::OutputCapabilityType);

// Functions to construct FIDL control library objects from library objects.
fuchsia::bluetooth::control::AdapterInfo NewAdapterInfo(const bt::gap::Adapter& adapter);
fuchsia::bluetooth::control::RemoteDevice NewRemoteDevice(const bt::gap::Peer& peer);
fuchsia::bluetooth::control::RemoteDevicePtr NewRemoteDevicePtr(const bt::gap::Peer& peer);

// Functions to convert Control FIDL library objects.
bt::sm::PairingData PairingDataFromFidl(const fuchsia::bluetooth::control::LEData& data);
bt::UInt128 LocalKeyFromFidl(const fuchsia::bluetooth::control::LocalKey& key);
std::optional<bt::sm::LTK> BrEdrKeyFromFidl(const fuchsia::bluetooth::control::BREDRData& data);
fuchsia::bluetooth::control::BondingData NewBondingData(const bt::gap::Adapter& adapter,
                                                        const bt::gap::Peer& peer);

// Functions to construct FIDL LE library objects from library objects.
fuchsia::bluetooth::le::RemoteDevicePtr NewLERemoteDevice(const bt::gap::Peer& peer);

// Validates the contents of a ScanFilter.
bool IsScanFilterValid(const fuchsia::bluetooth::le::ScanFilter& fidl_filter);

// Populates a library DiscoveryFilter based on a FIDL ScanFilter. Returns false
// if |fidl_filter| contains any malformed data and leaves |out_filter|
// unmodified.
bool PopulateDiscoveryFilter(const fuchsia::bluetooth::le::ScanFilter& fidl_filter,
                             bt::gap::DiscoveryFilter* out_filter);

// Converts the given |mode_hint| to a stack interval value.
bt::gap::AdvertisingInterval AdvertisingIntervalFromFidl(
    fuchsia::bluetooth::le::AdvertisingModeHint mode_hint);

bt::gap::AdvertisingData AdvertisingDataFromFidl(
    const fuchsia::bluetooth::le::AdvertisingData& input);
fuchsia::bluetooth::le::AdvertisingData AdvertisingDataToFidl(
    const bt::gap::AdvertisingData& input);

// Constructs a fuchsia.bluetooth.le Peer type from the stack representation.
fuchsia::bluetooth::le::Peer PeerToFidlLe(const bt::gap::Peer& peer);

}  // namespace fidl_helpers
}  // namespace bthost

// fidl::TypeConverter specializations for ByteBuffer and friends.
template <>
struct fidl::TypeConverter<std::vector<uint8_t>, bt::ByteBuffer> {
  static std::vector<uint8_t> Convert(const bt::ByteBuffer& from);
};

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_
