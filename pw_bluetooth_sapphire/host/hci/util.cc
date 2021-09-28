// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include <endian.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::hci {

bool DeviceAddressFromAdvReport(const hci_spec::LEAdvertisingReportData& report,
                                DeviceAddress* out_address, bool* out_resolved) {
  ZX_DEBUG_ASSERT(out_address);
  ZX_DEBUG_ASSERT(out_resolved);

  DeviceAddress::Type type;
  switch (report.address_type) {
    case hci_spec::LEAddressType::kPublicIdentity:
      type = DeviceAddress::Type::kLEPublic;
      *out_resolved = true;
      break;
    case hci_spec::LEAddressType::kPublic:
      type = DeviceAddress::Type::kLEPublic;
      *out_resolved = false;
      break;
    case hci_spec::LEAddressType::kRandomIdentity:
      type = DeviceAddress::Type::kLERandom;
      *out_resolved = true;
      break;
    case hci_spec::LEAddressType::kRandom:
      type = DeviceAddress::Type::kLERandom;
      *out_resolved = false;
      break;
    default:
      bt_log(WARN, "hci", "invalid address type in advertising report: %#.2x",
             static_cast<uint8_t>(report.address_type));
      return false;
  }

  *out_address = DeviceAddress(type, report.address);
  return true;
}

DeviceAddress::Type AddressTypeFromHCI(hci_spec::LEAddressType type) {
  DeviceAddress::Type result;
  switch (type) {
    case hci_spec::LEAddressType::kPublic:
    case hci_spec::LEAddressType::kPublicIdentity:
      result = DeviceAddress::Type::kLEPublic;
      break;
    case hci_spec::LEAddressType::kRandom:
    case hci_spec::LEAddressType::kRandomIdentity:
    case hci_spec::LEAddressType::kRandomUnresolved:
      result = DeviceAddress::Type::kLERandom;
      break;
    case hci_spec::LEAddressType::kAnonymous:
      result = DeviceAddress::Type::kLEAnonymous;
      break;
  }
  return result;
}

DeviceAddress::Type AddressTypeFromHCI(hci_spec::LEPeerAddressType type) {
  DeviceAddress::Type result;
  switch (type) {
    case hci_spec::LEPeerAddressType::kPublic:
      result = DeviceAddress::Type::kLEPublic;
      break;
    case hci_spec::LEPeerAddressType::kRandom:
      result = DeviceAddress::Type::kLERandom;
      break;
    case hci_spec::LEPeerAddressType::kAnonymous:
      result = DeviceAddress::Type::kLEAnonymous;
      break;
  }
  return result;
}

hci_spec::LEAddressType AddressTypeToHCI(DeviceAddress::Type type) {
  hci_spec::LEAddressType result = hci_spec::LEAddressType::kPublic;
  switch (type) {
    case DeviceAddress::Type::kLEPublic:
      result = hci_spec::LEAddressType::kPublic;
      break;
    case DeviceAddress::Type::kLERandom:
      result = hci_spec::LEAddressType::kRandom;
      break;
    case DeviceAddress::Type::kLEAnonymous:
      result = hci_spec::LEAddressType::kAnonymous;
      break;
    default:
      ZX_PANIC("invalid address type: %u", static_cast<unsigned int>(type));
      break;
  }
  return result;
}

}  // namespace bt::hci
