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

#pragma once
#include <optional>
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/uint128.h"

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

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(IdentityResolvingList);
};

}  // namespace bt::gap
