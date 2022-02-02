// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_

#include <fuchsia/bluetooth/cpp/fidl.h>
#include <fuchsia/bluetooth/gatt/cpp/fidl.h>
#include <fuchsia/bluetooth/gatt2/cpp/fidl.h>
#include <fuchsia/bluetooth/host/cpp/fidl.h>
#include <fuchsia/bluetooth/le/cpp/fidl.h>

#include <optional>

#include "fuchsia/bluetooth/sys/cpp/fidl.h"
#include "lib/fidl/cpp/type_converter.h"
#include "lib/fidl/cpp/vector.h"
#include "lib/fpromise/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/types.h"

// Helpers for implementing the Bluetooth FIDL interfaces.

namespace bt::gap {

class DiscoveryFilter;

}  // namespace bt::gap

namespace bthost::fidl_helpers {

// TODO(fxbug.dev/898): Temporary logic for converting between the stack identifier
// type (integer) and FIDL identifier type (string). Remove these once all FIDL
// interfaces have been converted to use integer IDs.
std::optional<bt::PeerId> PeerIdFromString(const std::string& id);

// Functions for generating a FIDL bluetooth::Status

fuchsia::bluetooth::ErrorCode HostErrorToFidlDeprecated(bt::HostError host_error);

fuchsia::bluetooth::Status NewFidlError(fuchsia::bluetooth::ErrorCode error_code,
                                        std::string description);

template <typename ProtocolErrorCode>
fuchsia::bluetooth::Status ResultToFidlDeprecated(
    const fitx::result<bt::Error<ProtocolErrorCode>>& result, std::string msg = "") {
  fuchsia::bluetooth::Status fidl_status;
  if (result.is_ok()) {
    return fidl_status;
  }

  auto error = std::make_unique<fuchsia::bluetooth::Error>();
  error->description = msg.empty() ? bt_str(result) : std::move(msg);
  if (result.is_error()) {
    result.error_value().Visit(
        [&error](bt::HostError c) { error->error_code = HostErrorToFidlDeprecated(c); },
        [&](ProtocolErrorCode c) {
          if constexpr (bt::Error<ProtocolErrorCode>::may_hold_protocol_error()) {
            error->error_code = fuchsia::bluetooth::ErrorCode::PROTOCOL_ERROR;
            error->protocol_error_code = static_cast<uint32_t>(c);
          } else {
            ZX_PANIC("Protocol branch visited by bt::Error<NoProtocolError>");
          }
        });
  }

  fidl_status.error = std::move(error);
  return fidl_status;
}

// Convert a bt::HostError to fuchsia.bluetooth.sys.Error. This function does only
// deals with bt::HostError types and does not support Bluetooth protocol-specific errors; to
// represent such errors use protocol-specific FIDL error types. An |error| value of
// HostError::kNoError is not allowed.
fuchsia::bluetooth::sys::Error HostErrorToFidl(bt::HostError error);

// Convert a bt::Error to fuchsia.bluetooth.sys.Error. This function does only deals with
// bt::HostError codes and does not support Bluetooth protocol-specific errors; to
// represent such errors use protocol-specific FIDL error types.
template <typename ProtocolErrorCode>
fuchsia::bluetooth::sys::Error HostErrorToFidl(const bt::Error<ProtocolErrorCode>& error) {
  if (!error.is_host_error()) {
    return fuchsia::bluetooth::sys::Error::FAILED;
  }
  return HostErrorToFidl(error.host_error());
}

// Convert any bt::Status to a fpromise::result that uses the fuchsia.bluetooth.sys library error
// codes.
template <typename ProtocolErrorCode>
fpromise::result<void, fuchsia::bluetooth::sys::Error> ResultToFidl(
    const fitx::result<bt::Error<ProtocolErrorCode>>& status) {
  if (status.is_ok()) {
    return fpromise::ok();
  } else {
    return fpromise::error(HostErrorToFidl(std::move(status).error_value()));
  }
}

// Convert a bt::att::Error to fuchsia.bluetooth.gatt.Error.
fuchsia::bluetooth::gatt::Error GattErrorToFidl(const bt::att::Error& error);

// Convert a bt::att::Error to fuchsia.bluetooth.gatt2.Error.
fuchsia::bluetooth::gatt2::Error AttErrorToGattFidlError(const bt::att::Error& error);

bt::UUID UuidFromFidl(const fuchsia::bluetooth::Uuid& input);
fuchsia::bluetooth::Uuid UuidToFidl(const bt::UUID& uuid);

// Functions that convert FIDL types to library objects.
bt::sm::IOCapability IoCapabilityFromFidl(const fuchsia::bluetooth::sys::InputCapability,
                                          const fuchsia::bluetooth::sys::OutputCapability);
bt::gap::LeSecurityMode LeSecurityModeFromFidl(const fuchsia::bluetooth::sys::LeSecurityMode mode);
std::optional<bt::sm::SecurityLevel> SecurityLevelFromFidl(
    const fuchsia::bluetooth::sys::PairingSecurityLevel level);

// fuchsia.bluetooth.sys library helpers.
fuchsia::bluetooth::sys::TechnologyType TechnologyTypeToFidl(bt::gap::TechnologyType type);
fuchsia::bluetooth::sys::HostInfo HostInfoToFidl(const bt::gap::Adapter& adapter);
fuchsia::bluetooth::sys::Peer PeerToFidl(const bt::gap::Peer& peer);

// Functions to convert bonding data structures from FIDL.
std::optional<bt::DeviceAddress> AddressFromFidlBondingData(
    const fuchsia::bluetooth::sys::BondingData& data);
bt::sm::PairingData LePairingDataFromFidl(bt::DeviceAddress peer_address,
                                          const fuchsia::bluetooth::sys::LeBondData& data);
std::optional<bt::sm::LTK> BredrKeyFromFidl(const fuchsia::bluetooth::sys::BredrBondData& data);
std::vector<bt::UUID> BredrServicesFromFidl(const fuchsia::bluetooth::sys::BredrBondData& data);

// Function to construct a bonding data structure for a peer.
fuchsia::bluetooth::sys::BondingData PeerToFidlBondingData(const bt::gap::Adapter& adapter,
                                                           const bt::gap::Peer& peer);

// Functions to construct FIDL LE library objects from library objects. Returns nullptr if the peer
// is not LE or if the peer's advertising data failed to parse.
fuchsia::bluetooth::le::RemoteDevicePtr NewLERemoteDevice(const bt::gap::Peer& peer);

// Validates the contents of a ScanFilter.
bool IsScanFilterValid(const fuchsia::bluetooth::le::ScanFilter& fidl_filter);

// Populates a library DiscoveryFilter based on a FIDL ScanFilter. Returns false
// if |fidl_filter| contains any malformed data and leaves |out_filter|
// unmodified.
bool PopulateDiscoveryFilter(const fuchsia::bluetooth::le::ScanFilter& fidl_filter,
                             bt::gap::DiscoveryFilter* out_filter);
bt::gap::DiscoveryFilter DiscoveryFilterFromFidl(const fuchsia::bluetooth::le::Filter& fidl_filter);

// Converts the given |mode_hint| to a stack interval value.
bt::gap::AdvertisingInterval AdvertisingIntervalFromFidl(
    fuchsia::bluetooth::le::AdvertisingModeHint mode_hint);

std::optional<bt::AdvertisingData> AdvertisingDataFromFidl(
    const fuchsia::bluetooth::le::AdvertisingData& input);
fuchsia::bluetooth::le::AdvertisingData AdvertisingDataToFidl(const bt::AdvertisingData& input);
fuchsia::bluetooth::le::AdvertisingDataDeprecated AdvertisingDataToFidlDeprecated(
    const bt::AdvertisingData& input);
fuchsia::bluetooth::le::ScanData AdvertisingDataToFidlScanData(const bt::AdvertisingData& input,
                                                               zx::time timestamp);

// Constructs a fuchsia.bluetooth.le Peer type from the stack representation.
fuchsia::bluetooth::le::Peer PeerToFidlLe(const bt::gap::Peer& peer);

// Functions that convert FIDL GATT types to library objects.
bt::gatt::ReliableMode ReliableModeFromFidl(
    const fuchsia::bluetooth::gatt::WriteOptions& write_options);
// TODO(fxbug.dev/63438): The 64 bit `fidl_gatt_id` can overflow the 16 bits of a bt:att::Handle
// that underlies Characteristic/DescriptorHandles when directly casted. Fix this.
bt::gatt::CharacteristicHandle CharacteristicHandleFromFidl(uint64_t fidl_gatt_id);
bt::gatt::DescriptorHandle DescriptorHandleFromFidl(uint64_t fidl_gatt_id);

// Constructs a sdp::ServiceRecord from a FIDL ServiceDefinition |definition|
fpromise::result<bt::sdp::ServiceRecord, fuchsia::bluetooth::ErrorCode>
ServiceDefinitionToServiceRecord(const fuchsia::bluetooth::bredr::ServiceDefinition& definition);

bt::gap::BrEdrSecurityRequirements FidlToBrEdrSecurityRequirements(
    const fuchsia::bluetooth::bredr::ChannelParameters& fidl);

fpromise::result<bt::hci_spec::SynchronousConnectionParameters> FidlToScoParameters(
    const fuchsia::bluetooth::bredr::ScoConnectionParameters& params);
fpromise::result<std::vector<bt::hci_spec::SynchronousConnectionParameters>>
FidlToScoParametersVector(
    const std::vector<fuchsia::bluetooth::bredr::ScoConnectionParameters>& params);

// Returns true if |handle| is within the valid handle range.
bool IsFidlGattHandleValid(fuchsia::bluetooth::gatt2::Handle handle);

}  // namespace bthost::fidl_helpers

// fidl::TypeConverter specializations for ByteBuffer and friends.
template <>
struct fidl::TypeConverter<std::vector<uint8_t>, bt::ByteBuffer> {
  static std::vector<uint8_t> Convert(const bt::ByteBuffer& from);
};

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_HELPERS_H_
