// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/util.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"

#pragma clang diagnostic ignored "-Wswitch-enum"

namespace bt::hci {

bool DeviceAddressFromAdvReport(const hci_spec::LEAdvertisingReportData& report,
                                DeviceAddress* out_address,
                                bool* out_resolved) {
  BT_DEBUG_ASSERT(out_address);
  BT_DEBUG_ASSERT(out_resolved);

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
      bt_log(WARN,
             "hci",
             "invalid address type in advertising report: %#.2x",
             static_cast<uint8_t>(report.address_type));
      return false;
  }

  *out_address = DeviceAddress(type, report.address);
  return true;
}

}  // namespace bt::hci
