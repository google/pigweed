// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_server.h"

namespace bthost {

namespace fbg = fuchsia::bluetooth::gatt2;

LowEnergyConnectionServer::LowEnergyConnectionServer(
    fxl::WeakPtr<bt::gatt::GATT> gatt,
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection, zx::channel handle,
    fit::callback<void()> closed_cb)
    : ServerBase(this, std::move(handle)),
      conn_(std::move(connection)),
      closed_handler_(std::move(closed_cb)),
      peer_id_(conn_->peer_identifier()),
      gatt_(std::move(gatt)) {
  BT_DEBUG_ASSERT(conn_);

  set_error_handler([this](zx_status_t) { OnClosed(); });
  conn_->set_closed_callback(fit::bind_member<&LowEnergyConnectionServer::OnClosed>(this));
}

void LowEnergyConnectionServer::OnClosed() {
  if (closed_handler_) {
    binding()->Close(ZX_ERR_CONNECTION_RESET);
    closed_handler_();
  }
}

void LowEnergyConnectionServer::RequestGattClient(fidl::InterfaceRequest<fbg::Client> client) {
  if (gatt_client_server_.has_value()) {
    bt_log(INFO, "fidl", "%s: gatt client server already bound (peer: %s)", __FUNCTION__,
           bt_str(peer_id_));
    client.Close(ZX_ERR_ALREADY_BOUND);
    return;
  }

  fit::callback<void()> server_error_cb = [this] {
    bt_log(TRACE, "fidl", "gatt client server error (peer: %s)", bt_str(peer_id_));
    gatt_client_server_.reset();
  };
  gatt_client_server_.emplace(peer_id_, gatt_, std::move(client), std::move(server_error_cb));
}

}  // namespace bthost
