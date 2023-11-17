// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_STATE_H_

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_channel.h"

namespace bt::gap {

// Stores Bluetooth Low Energy settings and state information.
class LowEnergyState final {
 public:
  // Returns true if |feature_bit| is set as supported in the local LE features
  // list.
  inline bool IsFeatureSupported(
      hci_spec::LESupportedFeature feature_bit) const {
    return supported_features_ & static_cast<uint64_t>(feature_bit);
  }

  uint64_t supported_features() const { return supported_features_; }

  // Returns the LE ACL data buffer capacity.
  const hci::DataBufferInfo& data_buffer_info() const {
    return data_buffer_info_;
  }

 private:
  friend class Adapter;
  friend class AdapterImpl;

  // Storage capacity information about the controller's internal ACL data
  // buffers.
  hci::DataBufferInfo data_buffer_info_;

  // Local supported LE Features reported by the controller.
  uint64_t supported_features_ = 0;

  // Local supported LE states reported by the controller.
  uint64_t supported_states_ = 0;
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_STATE_H_
