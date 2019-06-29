// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <fuchsia/bluetooth/gatt/cpp/fidl.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/common/task_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

class GattClientServer;
class GattServerServer;

// This object is responsible for bridging the GATT profile to the outside
// world.
//
// GattHost represents the GATT TaskDomain. It spawns and manages a thread on
// which all GATT tasks are serialized in an asynchronous event loop.
//
// This domain is responsible for:
//
//   * The GATT profile implementation over L2CAP;
//   * Creation of child GATT bt-gatt-svc devices and relaying of their
//     messages;
//   * FIDL message processing
//
// All functions on this object are thread safe. ShutDown() must be called
// at least once to properly clean up this object before destruction (this is
// asserted).
class GattHost final : public fbl::RefCounted<GattHost>, public bt::TaskDomain<GattHost> {
 public:
  // Type that can be used as a token in some of the functions below. Pointers
  // are allowed to be used as tokens.
  using Token = uintptr_t;

  // Creates a production GattHost that spawns a dispatcher with a dedicated
  // thread and constructs a GATT domain attached to that dispatcher. This is
  // intended for production use.
  static fbl::RefPtr<GattHost> Create(std::string thread_name);

  // Creates a GattHost that is intended for testing. This takes the GATT domain
  // and the dispatcher as a dependency rather than managing an internal thread.
  static fbl::RefPtr<GattHost> CreateForTesting(async_dispatcher_t* dispatcher,
                                                fbl::RefPtr<bt::gatt::GATT> gatt);

  // Initialize.
  void Initialize();

  // This MUST be called to cleanly destroy this object. This method is
  // thread-safe.
  void ShutDown();

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

  // Unbinds a previously bound GATT client server associated with |token|.
  void UnbindGattClient(Token token);

  // Returns the GATT profile implementation.
  fbl::RefPtr<bt::gatt::GATT> profile() const { return gatt_; }

  // Sets a remote service handler to be notified when remote GATT services are
  // discovered. These are used by HostDevice to publish bt-gatt-svc devices.
  // This method is thread-safe. |callback| will not be called after ShutDown().
  void SetRemoteServiceWatcher(bt::gatt::GATT::RemoteServiceWatcher callback);

 private:
  BT_FRIEND_TASK_DOMAIN(GattHost);
  friend class fbl::RefPtr<GattHost>;

  // Constructor used in production. This instantiates a real GATT domain object
  // and spawns an event loop with a dedicated thread.
  explicit GattHost(std::string thread_name);

  // Constructor used for testing. This does NOT spawn a thread but takes a
  // GATT domain and dispatcher as a dependency.
  GattHost(async_dispatcher_t* dispatcher, fbl::RefPtr<bt::gatt::GATT> gatt);
  ~GattHost() override;

  // Called by TaskDomain during shutdown. This must be called on the GATT task
  // runner.
  void CleanUp();

  // Closes the active FIDL servers. This must be called on the GATT thread.
  void CloseServersInternal();

  std::mutex mtx_;
  bt::gatt::GATT::RemoteServiceWatcher remote_service_watcher_ __TA_GUARDED(mtx_);

  // NOTE: All members below must be accessed on the GATT thread

  // The GATT profile.
  fbl::RefPtr<bt::gatt::GATT> gatt_;

  // All currently active FIDL connections. These objects are thread hostile and
  // must be accessed only via the TaskDomain dispatcher.
  std::unordered_map<GattServerServer*, std::unique_ptr<GattServerServer>> server_servers_;

  // Mapping from tokens GattClient pointers. The ID is provided by the caller.
  std::unordered_map<Token, std::unique_ptr<GattClientServer>> client_servers_;

  fxl::WeakPtrFactory<GattHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattHost);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_HOST_H_
