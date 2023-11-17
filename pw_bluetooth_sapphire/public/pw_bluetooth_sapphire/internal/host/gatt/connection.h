// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/remote_service_manager.h"

namespace bt {

namespace l2cap {
class Channel;
}  // namespace l2cap

namespace att {
class Bearer;
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
  // |client| is the GATT client for this connection, which uses |att_bearer| in
  // production. |server| is the GATT server for this connection, which uses
  // |att_bearer| in production. |svc_watcher| communicates updates about the
  // peer's GATT services to the Connection's owner.
  Connection(std::unique_ptr<Client> client,
             std::unique_ptr<Server> server,
             RemoteServiceWatcher svc_watcher);
  ~Connection() = default;

  Server* server() const { return server_.get(); }
  RemoteServiceManager* remote_service_manager() const {
    return remote_service_manager_.get();
  }

  // Performs MTU exchange, then primary service discovery. Shuts down the
  // connection on failure. If |service_uuids| is non-empty, discovery is only
  // performed for services with the indicated UUIDs. Returns the agreed-upon
  // MTU via |mtu_cb|.
  void Initialize(std::vector<UUID> service_uuids,
                  fit::callback<void(uint16_t)> mtu_cb);

  // Closes the ATT bearer on which the connection operates.
  void ShutDown();

 private:
  std::unique_ptr<Server> server_;
  std::unique_ptr<RemoteServiceManager> remote_service_manager_;

  WeakSelf<Connection> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Connection);
};

}  // namespace internal
}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_CONNECTION_H_
