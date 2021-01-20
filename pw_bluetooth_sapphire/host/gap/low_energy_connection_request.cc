// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_request.h"

namespace bt::gap::internal {

LowEnergyConnectionRequest::LowEnergyConnectionRequest(
    const DeviceAddress& address, ConnectionResultCallback first_callback,
    LowEnergyConnectionOptions connection_options)
    : address_(address, MakeToStringInspectConvertFunction()),
      callbacks_(std::list<ConnectionResultCallback>(), [](const auto& cbs) { return cbs.size(); }),
      connection_options_(connection_options),
      connection_attempts_(0) {
  callbacks_.Mutable()->push_back(std::move(first_callback));
}

void LowEnergyConnectionRequest::NotifyCallbacks(fit::result<RefFunc, HostError> result) {
  for (const auto& callback : *callbacks_) {
    if (result.is_error()) {
      callback(fit::error(result.error()));
      continue;
    }
    auto conn_ref = result.value()();
    callback(fit::ok(std::move(conn_ref)));
  }
}

void LowEnergyConnectionRequest::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  address_.AttachInspect(inspect_node_, "address");
  callbacks_.AttachInspect(inspect_node_, "callbacks");
  connection_attempts_.AttachInspect(inspect_node_, "connection_attempts");
}

}  // namespace bt::gap::internal
