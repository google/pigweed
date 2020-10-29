// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_host.h"

#include "fidl/gatt_client_server.h"
#include "fidl/gatt_server_server.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

using namespace bt;

namespace bthost {

GattHost::GattHost() : GattHost::GattHost(gatt::GATT::Create()) {}

GattHost::GattHost(std::unique_ptr<gatt::GATT> gatt)
    : gatt_(std::move(gatt)), weak_ptr_factory_(this) {
  ZX_ASSERT(gatt_);

  gatt_->RegisterRemoteServiceWatcher(
      [self = weak_ptr_factory_.GetWeakPtr()](auto peer_id, auto service) {
        // Make sure we are not holding the lock while |watcher| executes to
        // prevent potential deadlocks.
        bt::gatt::GATT::RemoteServiceWatcher watcher;
        {
          std::lock_guard<std::mutex> lock(self->mtx_);
          if (self->remote_service_watcher_) {
            watcher = self->remote_service_watcher_.share();
          }
        }
        if (watcher) {
          watcher(peer_id, service);
        }
      });
}

GattHost::~GattHost() {
  // Stop processing further GATT profile requests.
  gatt_ = nullptr;

  // Clear the remote device callback to prevent further notifications after
  // this call.
  {
    std::lock_guard<std::mutex> lock(mtx_);
    remote_service_watcher_ = {};
  }

  CloseServers();
}

void GattHost::CloseServers() {
  // This closes all remaining FIDL channels.
  client_servers_.clear();
  server_servers_.clear();
}

void GattHost::BindGattServer(fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> request) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto server = std::make_unique<GattServerServer>(gatt_->AsWeakPtr(), std::move(request));
  server->set_error_handler([self, server = server.get()](zx_status_t status) {
    if (self) {
      bt_log(DEBUG, "bt-host", "GATT server disconnected");
      self->server_servers_.erase(server);
    }
  });
  server_servers_[server.get()] = std::move(server);
}

void GattHost::BindGattClient(Token token, bt::gatt::PeerId peer_id,
                              fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Client> request) {
  // Either initialize a new entry for |token| or obtain an iterator to the
  // existing entry. In either case the iterator will be valid.
  auto inner_iter = client_servers_.try_emplace(token, ClientMap()).first;
  if (inner_iter->second.find(peer_id) != inner_iter->second.end()) {
    bt_log(WARN, "bt-host", "only 1 gatt.Client FIDL handle allowed per peer (%s) per token (%lu)!",
           bt_str(peer_id), token);

    // The handle owned by |request| will be closed.
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto server = std::make_unique<GattClientServer>(peer_id, gatt_->AsWeakPtr(), std::move(request));
  server->set_error_handler([self, token, peer_id](zx_status_t status) {
    if (self) {
      bt_log(DEBUG, "bt-host", "GATT client disconnected");
      self->UnbindGattClient(token, {peer_id});
    }
  });
  inner_iter->second[peer_id] = std::move(server);
}

void GattHost::UnbindGattClient(Token token, std::optional<bt::gatt::PeerId> peer_id) {
  if (!peer_id.has_value()) {
    client_servers_.erase(token);
    return;
  }

  auto iter = client_servers_.find(token);
  if (iter != client_servers_.end()) {
    iter->second.erase(*peer_id);
    if (iter->second.empty()) {
      client_servers_.erase(iter);
    }
  }
}

void GattHost::SetRemoteServiceWatcher(bt::gatt::GATT::RemoteServiceWatcher callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  remote_service_watcher_ = std::move(callback);
}

}  // namespace bthost
