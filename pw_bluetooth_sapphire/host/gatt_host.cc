// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_host.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

#include "fidl/gatt_client_server.h"
#include "fidl/gatt_server_server.h"

using namespace btlib;

namespace bthost {

// static
fbl::RefPtr<GattHost> GattHost::Create(std::string thrd_name) {
  return AdoptRef(new GattHost(std::move(thrd_name)));
}

GattHost::GattHost(std::string thrd_name)
    : btlib::common::TaskDomain<GattHost>(this, std::move(thrd_name)),
      weak_ptr_factory_(this) {
  // Initialize the profile to operate on our task runner.
  gatt_ = gatt::GATT::Create(dispatcher());
  ZX_DEBUG_ASSERT(gatt_);
}

GattHost::~GattHost() {}

void GattHost::Initialize() {
  // Initialize the profile.
  gatt_->Initialize();
  gatt_->RegisterRemoteServiceWatcher(
      [self = fbl::WrapRefPtr(this)](auto peer_id, auto service) {
        std::lock_guard<std::mutex> lock(self->mtx_);
        if (self->alive() && self->remote_service_watcher_) {
          self->remote_service_watcher_(peer_id, service);
        }
      });
}

void GattHost::CloseServers() {
  PostMessage([this] { CloseServersInternal(); });
}

void GattHost::ShutDown() {
  // Stop processing further GATT profile requests.
  gatt_->ShutDown();

  // Clear the remote device callback to prevent further notifications after
  // this call.
  {
    std::lock_guard<std::mutex> lock(mtx_);
    remote_service_watcher_ = {};
  }

  btlib::common::TaskDomain<GattHost>::ScheduleCleanUp();
}

void GattHost::CleanUp() {
  AssertOnDispatcherThread();

  CloseServersInternal();
}

void GattHost::BindGattServer(
    fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> request) {
  PostMessage([this, request = std::move(request)]() mutable {
    auto self = weak_ptr_factory_.GetWeakPtr();
    auto server = std::make_unique<GattServerServer>(gatt_, std::move(request));
    server->set_error_handler(
        [self, server = server.get()](zx_status_t status) {
          if (self) {
            bt_log(TRACE, "bt-host", "GATT server disconnected");
            self->server_servers_.erase(server);
          }
        });
    server_servers_[server.get()] = std::move(server);
  });
}

void GattHost::BindGattClient(
    Token token, btlib::gatt::DeviceId peer_id,
    fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Client> request) {
  PostMessage([this, token, peer_id, request = std::move(request)]() mutable {
    if (client_servers_.find(token) != client_servers_.end()) {
      bt_log(WARN, "bt-host", "duplicate Client FIDL server tokens!");

      // The handle owned by |request| will be closed.
      return;
    }

    auto self = weak_ptr_factory_.GetWeakPtr();
    auto server =
        std::make_unique<GattClientServer>(peer_id, gatt_, std::move(request));
    server->set_error_handler([self, token](zx_status_t status) {
      if (self) {
        bt_log(TRACE, "bt-host", "GATT client disconnected");
        self->client_servers_.erase(token);
      }
    });
    client_servers_[token] = std::move(server);
  });
}

void GattHost::UnbindGattClient(Token token) {
  PostMessage([this, token] { client_servers_.erase(token); });
}

void GattHost::SetRemoteServiceWatcher(
    btlib::gatt::GATT::RemoteServiceWatcher callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  remote_service_watcher_ = std::move(callback);
}

void GattHost::CloseServersInternal() {
  AssertOnDispatcherThread();

  // This closes all remaining FIDL channels.
  client_servers_.clear();
  server_servers_.clear();
}

}  // namespace bthost
