// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_

#include <lib/async/dispatcher.h>

#include <memory>

#include <fbl/macros.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service_manager.h"

namespace bt {

namespace l2cap {
class Channel;
}  // namespace l2cap

namespace att {
class Bearer;
class Database;
}  // namespace att

namespace gatt {

class Server;

namespace internal {

// Represents the GATT data channel between the local adapter and a single
// remote peer. A Connection supports simultaneous GATT client and server
// functionality. An instance of Connection should exist on each ACL logical
// link.
class Connection final {
 public:
  // |peer_id| identifies the peer device.
  // |local_db| is the local attribute database that the GATT server will
  // operate on.
  // |att_bearer| is the ATT protocol data transport for this connection.
  // |client| is the ATT client for this connection, which uses |att_bearer| in production.
  Connection(PeerId peer_id, fbl::RefPtr<att::Bearer> att_bearer, std::unique_ptr<Client> client,
             fbl::RefPtr<att::Database> local_db, RemoteServiceWatcher svc_watcher,
             async_dispatcher_t* gatt_dispatcher);
  ~Connection() = default;

  Server* server() const { return server_.get(); }
  RemoteServiceManager* remote_service_manager() const { return remote_service_manager_.get(); }

  // Initiate MTU exchange followed by primary service discovery. On failure,
  // signals a link error through the ATT channel (which is expected to
  // disconnect the link).
  // If |service_uuids| is non-empty, discovery is only performed for services with the indicated
  // UUIDs.
  void Initialize(std::vector<UUID> service_uuids);

 private:
  fbl::RefPtr<att::Bearer> att_;
  std::unique_ptr<Server> server_;
  std::unique_ptr<RemoteServiceManager> remote_service_manager_;

  fxl::WeakPtrFactory<Connection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Connection);
};

}  // namespace internal
}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_
