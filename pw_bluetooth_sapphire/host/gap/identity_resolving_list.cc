// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "identity_resolving_list.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {
namespace gap {

using common::DeviceAddress;
using common::UInt128;

void IdentityResolvingList::Add(const DeviceAddress& identity,
                                const UInt128& irk) {
  registry_[identity] = irk;
}

std::optional<DeviceAddress> IdentityResolvingList::Resolve(
    const DeviceAddress& rpa) const {
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
