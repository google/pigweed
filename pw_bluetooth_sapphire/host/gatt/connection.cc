// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include <numeric>

#include "client.h"
#include "server.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"

namespace bt::gatt::internal {

Connection::Connection(std::unique_ptr<Client> client, std::unique_ptr<Server> server,
                       RemoteServiceWatcher svc_watcher, async_dispatcher_t* gatt_dispatcher)
    : server_(std::move(server)), weak_ptr_factory_(this) {
  BT_ASSERT(svc_watcher);

  remote_service_manager_ =
      std::make_unique<RemoteServiceManager>(std::move(client), gatt_dispatcher);
  remote_service_manager_->set_service_watcher(std::move(svc_watcher));
}

void Connection::Initialize(std::vector<UUID> service_uuids, fit::callback<void(uint16_t)> mtu_cb) {
  BT_ASSERT(remote_service_manager_);

  auto uuids_count = service_uuids.size();
  // status_cb must not capture att_ in order to prevent reference cycle.
  auto status_cb = [self = weak_ptr_factory_.GetWeakPtr(), uuids_count](att::Result<> status) {
    if (!self) {
      return;
    }

    if (bt_is_error(status, ERROR, "gatt", "client setup failed")) {
      // Signal a link error.
      self->ShutDown();
    } else if (uuids_count > 0) {
      bt_log(DEBUG, "gatt", "primary service discovery complete for (%zu) service uuids",
             uuids_count);
    } else {
      bt_log(DEBUG, "gatt", "primary service discovery complete");
    }
  };

  remote_service_manager_->Initialize(std::move(status_cb), std::move(mtu_cb),
                                      std::move(service_uuids));
}

void Connection::ShutDown() {
  // We shut down the connection from the server not for any technical reason, but just because it
  // was simpler to expose the att::Bearer's ShutDown behavior from the server.
  server_->ShutDown();
}
}  // namespace bt::gatt::internal
