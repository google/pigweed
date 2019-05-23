// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BONDING_DATA_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BONDING_DATA_H_

#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace gap {

// A |BondingData| struct can be passed to the peer cache and allows for
// flexibility in adding new fields to cache.
struct BondingData {
  PeerId identifier;
  DeviceAddress address;
  std::optional<std::string> name;
  sm::PairingData le_pairing_data;
  std::optional<sm::LTK> bredr_link_key;
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BONDING_DATA_H_
