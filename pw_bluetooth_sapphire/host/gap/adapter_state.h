// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_STATE_H_

#include <cstdint>

#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/android_vendor_capabilities.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_state.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/lmp_feature_set.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

namespace bt::gap {

// The member variables in this class consist of controller settings that are
// shared between LE and BR/EDR controllers. LE and BR/EDR specific state is
// stored in corresponding data structures.
struct AdapterState final {
  TechnologyType type() const {
    // Note: we don't support BR/EDR only controllers.
    if (IsBREDRSupported()) {
      return TechnologyType::kDualMode;
    }
    return TechnologyType::kLowEnergy;
  }

  // Returns true if the indicated feature is supported by Controller.
  bool IsControllerFeatureSupported(pw::bluetooth::Controller::FeaturesBits feature) const {
    return feature & controller_features;
  }

  // Helpers for querying LMP capabilities.
  inline bool IsBREDRSupported() const {
    return !features.HasBit(0u, hci_spec::LMPFeature::kBREDRNotSupported);
  }

  inline bool IsLowEnergySupported() const {
    return features.HasBit(0u, hci_spec::LMPFeature::kLESupported);
  }

  // Returns true if |command_bit| in the given |octet| is set in the supported
  // command list.
  inline bool IsCommandSupported(size_t octet, hci_spec::SupportedCommand command_bit) const {
    BT_DEBUG_ASSERT(octet < sizeof(supported_commands));
    return supported_commands[octet] & static_cast<uint8_t>(command_bit);
  }

  // HCI version supported by the controller.
  pw::bluetooth::emboss::CoreSpecificationVersion hci_version;

  // The Features that are supported by this adapter.
  hci_spec::LMPFeatureSet features;

  // Features reported by Controller.
  pw::bluetooth::Controller::FeaturesBits controller_features{0};

  // Bitmask list of HCI commands that the controller supports.
  uint8_t supported_commands[64] = {0};

  // This returns Bluetooth Controller address. This address has the following
  // meaning based on the controller capabilities:
  //   - On BR/EDR this is the Bluetooth Controller Address, or BD_ADDR.
  //   - On LE this is the Public Device Address. This value can be used as the
  //     device's identity address. This value can be zero if a Public Device
  //     Address is not used.
  //   - On BR/EDR/LE this is the LE Public Device Address AND the BD_ADDR.
  DeviceAddressBytes controller_address;

  // The BR/EDR ACL data buffer size. We store this here as it is needed on
  // dual-mode controllers even if the host stack is compiled for LE-only.
  hci::DataBufferInfo bredr_data_buffer_info;

  // The SCO buffer size.
  hci::DataBufferInfo sco_buffer_info;

  // BLE-specific state.
  LowEnergyState low_energy_state;

  // Android vendor extensions capabilities
  // NOTE: callers should separately check that the controller actually supports android vendor
  // extensions first.
  AndroidVendorCapabilities android_vendor_capabilities;

  // Local name
  std::string local_name;
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_ADAPTER_STATE_H_
