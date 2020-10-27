// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection.h"

#include <zircon/assert.h>

#include "client.h"
#include "server.h"
#include "src/connectivity/bluetooth/core/bt-host/att/database.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::gatt::internal {

Connection::Connection(PeerId peer_id, fxl::RefPtr<att::Bearer> att_bearer,
                       fxl::RefPtr<att::Database> local_db, RemoteServiceWatcher svc_watcher,
                       async_dispatcher_t* gatt_dispatcher)
    : att_(att_bearer) {
  ZX_DEBUG_ASSERT(att_bearer);
  ZX_DEBUG_ASSERT(local_db);
  ZX_DEBUG_ASSERT(svc_watcher);
  ZX_DEBUG_ASSERT(gatt_dispatcher);

  server_ = std::make_unique<gatt::Server>(peer_id, local_db, att_);
  remote_service_manager_ =
      std::make_unique<RemoteServiceManager>(gatt::Client::Create(att_), gatt_dispatcher);
  remote_service_manager_->set_service_watcher(std::move(svc_watcher));
}

void Connection::Initialize(std::optional<UUID> optional_service_uuid) {
  ZX_DEBUG_ASSERT(remote_service_manager_);
  remote_service_manager_->Initialize(
      [att = att_, optional_service_uuid](att::Status status) {
        if (bt_is_error(status, ERROR, "gatt", "client setup failed")) {
          // Signal a link error.
          att->ShutDown();
        } else if (optional_service_uuid) {
          bt_log(DEBUG, "gatt", "primary service discovery complete for services with UUID %s",
                 optional_service_uuid.value().ToString().c_str());
        } else {
          bt_log(DEBUG, "gatt", "primary service discovery complete");
        }
      },
      optional_service_uuid);
}

}  // namespace bt::gatt::internal
