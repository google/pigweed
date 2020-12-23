// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_request.h"

namespace bt::gap::internal {

LowEnergyConnectionRequest::LowEnergyConnectionRequest(
    const DeviceAddress& address, ConnectionResultCallback first_callback,
    LowEnergyConnectionOptions connection_options)
    : address_(address), connection_options_(connection_options), connection_attempts_(0) {
  callbacks_.push_back(std::move(first_callback));
}

void LowEnergyConnectionRequest::NotifyCallbacks(fit::result<RefFunc, HostError> result) {
  for (const auto& callback : callbacks_) {
    if (result.is_error()) {
      callback(fit::error(result.error()));
      continue;
    }
    auto conn_ref = result.value()();
    callback(fit::ok(std::move(conn_ref)));
  }
}

}  // namespace bt::gap::internal
