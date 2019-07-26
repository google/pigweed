// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identity_resolving_list.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {
namespace gap {

void IdentityResolvingList::Add(DeviceAddress identity, const UInt128& irk) {
  registry_[identity] = irk;
}

void IdentityResolvingList::Remove(DeviceAddress identity) { registry_.erase(identity); }

std::optional<DeviceAddress> IdentityResolvingList::Resolve(DeviceAddress rpa) const {
  if (!rpa.IsResolvablePrivate()) {
    return std::nullopt;
  }

  for (const auto& [identity, irk] : registry_) {
    if (sm::util::IrkCanResolveRpa(irk, rpa)) {
      return identity;
    }
  }

  return std::nullopt;
}

}  // namespace gap
}  // namespace bt
