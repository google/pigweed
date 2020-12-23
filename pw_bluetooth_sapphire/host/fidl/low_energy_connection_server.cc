// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_server.h"

namespace bthost {

LowEnergyConnectionServer::LowEnergyConnectionServer(
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection, zx::channel handle)
    : ServerBase(this, std::move(handle)), conn_(std::move(connection)) {
  ZX_DEBUG_ASSERT(conn_);

  set_error_handler([this](zx_status_t) { OnClosed(); });
  conn_->set_closed_callback(fit::bind_member(this, &LowEnergyConnectionServer::OnClosed));
}

void LowEnergyConnectionServer::OnClosed() {
  auto f = std::move(closed_handler_);
  if (f) {
    f();
  }
}

}  // namespace bthost
