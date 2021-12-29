// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_request.h"

namespace bt::gap::internal {

LowEnergyConnectionRequest::LowEnergyConnectionRequest(
    PeerId peer_id, ConnectionResultCallback first_callback,
    LowEnergyConnectionOptions connection_options,
    Peer::InitializingConnectionToken peer_conn_token)
    : peer_id_(peer_id, MakeToStringInspectConvertFunction()),
      callbacks_(/*convert=*/[](const auto& cbs) { return cbs.size(); }),
      connection_options_(connection_options),
      peer_conn_token_(std::move(peer_conn_token)) {
  callbacks_.Mutable()->push_back(std::move(first_callback));
}

void LowEnergyConnectionRequest::NotifyCallbacks(fitx::result<HostError, RefFunc> result) {
  peer_conn_token_.reset();

  for (const auto& callback : *callbacks_) {
    if (result.is_error()) {
      callback(fitx::error(result.error_value()));
      continue;
    }
    auto conn_ref = result.value()();
    callback(fitx::ok(std::move(conn_ref)));
  }
}

void LowEnergyConnectionRequest::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  peer_id_.AttachInspect(inspect_node_, "peer_id");
  callbacks_.AttachInspect(inspect_node_, "callbacks");
}

}  // namespace bt::gap::internal
