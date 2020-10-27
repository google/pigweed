// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_IDENTITY_RESOLVING_LIST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_IDENTITY_RESOLVING_LIST_H_

#include <optional>
#include <unordered_map>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"

namespace bt::gap {

// This class provides functions to obtain an identity address by resolving a
// given RPA. Resolution is performed using identity information stored in the
// registry.
//
// TODO(fxbug.dev/835): Manage the controller-based list here.
class IdentityResolvingList final {
 public:
  IdentityResolvingList() = default;
  ~IdentityResolvingList() = default;

  // Associate the given |irk| with |identity|. If |identity| is already in the
  // list, the existing entry is updated with the new IRK.
  void Add(DeviceAddress identity, const UInt128& irk);

  // Delete |identity| and associated IRK, if present.
  void Remove(DeviceAddress identity);

  // Tries to resolve the given RPA against the identities in the registry.
  // Returns std::nullopt if the address is not a RPA or cannot be resolved.
  // Otherwise, returns a value containing the identity address.
  std::optional<DeviceAddress> Resolve(DeviceAddress rpa) const;

 private:
  // Maps identity addresses to IRKs.
  std::unordered_map<DeviceAddress, UInt128> registry_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(IdentityResolvingList);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_IDENTITY_RESOLVING_LIST_H_
