// Copyright 2021 The Pigweed Authors
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

#include "pw_router/static_router.h"

#include <algorithm>
#include <mutex>

namespace pw::router {

Status StaticRouter::RoutePacket(ConstByteSpan packet) {
  uint32_t address;

  {
    // Only packet parsing is synchronized within the router; egresses must be
    // synchronized externally.
    std::lock_guard lock(mutex_);

    if (!parser_.Parse(packet)) {
      parser_errors_.Increment();
      return Status::DataLoss();
    }

    std::optional<uint32_t> result = parser_.GetDestinationAddress();
    if (!result.has_value()) {
      parser_errors_.Increment();
      return Status::DataLoss();
    }

    address = result.value();
  }

  auto route = std::find_if(routes_.begin(), routes_.end(), [&](auto r) {
    return r.address == address;
  });
  if (route == routes_.end()) {
    route_errors_.Increment();
    return Status::NotFound();
  }

  if (Status status = route->egress.SendPacket(packet); !status.ok()) {
    egress_errors_.Increment();
    return Status::Unavailable();
  }

  return OkStatus();
}

}  // namespace pw::router
