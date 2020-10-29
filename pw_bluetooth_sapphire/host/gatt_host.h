// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_

#include <fuchsia/bluetooth/gatt/cpp/fidl.h>

#include <fbl/macros.h>
#include <fbl/ref_ptr.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

class GattClientServer;
class GattServerServer;

// GattHost bridges the GATT profile to the outside world.
//
// GattHost is responsible for:
//   * The GATT profile implementation over L2CAP;
//   * Creation of child GATT bt-gatt-svc devices and relaying of their
//     messages;
//   * FIDL message processing
class GattHost final {
 public:
  // Type that can be used as a token in some of the functions below. Pointers
  // are allowed to be used as tokens.
  using Token = uintptr_t;

  // Construct with a default GATT implementation.
  GattHost();

  // Construct with a specified GATT implementation (e.g. a fake).
  explicit GattHost(std::unique_ptr<bt::gatt::GATT> gatt);

  ~GattHost();

  // Closes all open FIDL interface handles.
  void CloseServers();

  // Binds the given GATT server request to a FIDL server.
  void BindGattServer(fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> request);

  // Binds the given GATT client request to a FIDL server. The binding will be
  // associated with the given |token|. The same token can be
  // be passed to UnbindGattClient to disconnect a client.
  //
  // The handle associated with |request| will be closed if |token| is already
  // bound to another handle.
  void BindGattClient(Token token, bt::gatt::PeerId peer_id,
                      fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Client> request);

  // Unbinds a previously bound GATT client server associated with |token| and
  // |peer_id|. Unbinds all GATT client servers associated with |token| if
  // |peer_id| is std::nullopt.
  void UnbindGattClient(Token token, std::optional<bt::gatt::PeerId> peer_id);

  // Returns the GATT profile implementation.
  fxl::WeakPtr<bt::gatt::GATT> profile() const { return gatt_->AsWeakPtr(); }

  // Sets a remote service handler to be notified when remote GATT services are
  // discovered. These are used by HostDevice to publish bt-gatt-svc devices.
  // This method is thread-safe.
  void SetRemoteServiceWatcher(bt::gatt::GATT::RemoteServiceWatcher callback);

  fxl::WeakPtr<GattHost> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  std::mutex mtx_;
  bt::gatt::GATT::RemoteServiceWatcher remote_service_watcher_ __TA_GUARDED(mtx_);

  // NOTE: All members below must be accessed on the main thread.

  // The GATT profile.
  std::unique_ptr<bt::gatt::GATT> gatt_;

  // All currently active FIDL connections.
  std::unordered_map<GattServerServer*, std::unique_ptr<GattServerServer>> server_servers_;

  // Mapping from tokens GattClient pointers. The ID is provided by the caller.
  using ClientMap = std::unordered_map<bt::PeerId, std::unique_ptr<GattClientServer>>;
  std::unordered_map<Token, ClientMap> client_servers_;

  fxl::WeakPtrFactory<GattHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattHost);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_
