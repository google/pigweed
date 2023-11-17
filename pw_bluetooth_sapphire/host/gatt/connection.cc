// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/gatt/connection.h"

#include <numeric>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/client.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/local_service_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/server.h"

namespace bt::gatt::internal {

Connection::Connection(std::unique_ptr<Client> client,
                       std::unique_ptr<Server> server,
                       RemoteServiceWatcher svc_watcher)
    : server_(std::move(server)), weak_self_(this) {
  BT_ASSERT(svc_watcher);

  remote_service_manager_ =
      std::make_unique<RemoteServiceManager>(std::move(client));
  remote_service_manager_->set_service_watcher(std::move(svc_watcher));
}

void Connection::Initialize(std::vector<UUID> service_uuids,
                            fit::callback<void(uint16_t)> mtu_cb) {
  BT_ASSERT(remote_service_manager_);

  auto uuids_count = service_uuids.size();
  // status_cb must not capture att_ in order to prevent reference cycle.
  auto status_cb = [self = weak_self_.GetWeakPtr(),
                    uuids_count](att::Result<> status) {
    if (!self.is_alive()) {
      return;
    }

    if (bt_is_error(status, ERROR, "gatt", "client setup failed")) {
      // Signal a link error.
      self->ShutDown();
    } else if (uuids_count > 0) {
      bt_log(DEBUG,
             "gatt",
             "primary service discovery complete for (%zu) service uuids",
             uuids_count);
    } else {
      bt_log(DEBUG, "gatt", "primary service discovery complete");
    }
  };

  remote_service_manager_->Initialize(
      std::move(status_cb), std::move(mtu_cb), std::move(service_uuids));
}

void Connection::ShutDown() {
  // We shut down the connection from the server not for any technical reason,
  // but just because it was simpler to expose the att::Bearer's ShutDown
  // behavior from the server.
  server_->ShutDown();
}
}  // namespace bt::gatt::internal
