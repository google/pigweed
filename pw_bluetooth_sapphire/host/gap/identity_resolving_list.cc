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

#include "pw_bluetooth_sapphire/internal/host/gap/identity_resolving_list.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/sm/util.h"

namespace bt::gap {

void IdentityResolvingList::Add(DeviceAddress identity, const UInt128& irk) {
  bt_log(DEBUG, "gap", "Adding IRK for identity address %s", bt_str(identity));
  registry_[identity] = irk;
}

void IdentityResolvingList::Remove(DeviceAddress identity) {
  bt_log(
      DEBUG, "gap", "Removing IRK for identity address %s", bt_str(identity));
  registry_.erase(identity);
}

std::optional<DeviceAddress> IdentityResolvingList::Resolve(
    DeviceAddress rpa) const {
  if (!rpa.IsResolvablePrivate()) {
    return std::nullopt;
  }

  for (const auto& [identity, irk] : registry_) {
    if (sm::util::IrkCanResolveRpa(irk, rpa)) {
      bt_log(
          DEBUG, "gap", "RPA %s resolved to %s", bt_str(rpa), bt_str(identity));
      return identity;
    }
  }

  return std::nullopt;
}

}  // namespace bt::gap
