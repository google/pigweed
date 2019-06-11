// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_H_

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include "lib/fidl/cpp/vector.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/types.h"
#include "src/lib/fxl/memory/ref_ptr.h"

namespace bt {

namespace l2cap {
class Channel;
}  // namespace l2cap

namespace gatt {

// This is the root object of the GATT layer. This object owns:
//
//   * A single local attribute database
//   * All client and server data bearers
//   * L2CAP ATT fixed channels
//
// GATT requires an dispatcher on initialization which will be used to
// serialize all internal GATT tasks.
//
// All public functions are asynchronous and thread-safe.
class GATT : public fbl::RefCounted<GATT> {
 public:
  // Constructs a GATT object.
  static fbl::RefPtr<GATT> Create(async_dispatcher_t* gatt_dispatcher);

  // Initialize/ShutDown the GATT profile. It is safe for the caller to drop its
  // reference after ShutDown.
  //
  // The owner MUST call ShutDown() to properly clean up the object.
  using InitializeCallback = fit::function<void()>;
  virtual void Initialize(InitializeCallback callback) = 0;
  virtual void ShutDown() = 0;

  // Registers the given connection with the GATT profile without initiating
  // service discovery. Once a connection is registered with GATT, the peer can
  // access local services and clients can call the "Remote Service" methods
  // below using |peer_id|.
  //
  // |peer_id|: The identifier for the peer device that the link belongs to.
  //            This is used to identify the peer while handling certain events.
  // |att_chan|: The ATT fixed channel over which the ATT protocol bearer will
  //             operate. The bearer will be associated with the link that
  //             underlies this channel.
  virtual void AddConnection(PeerId peer_id,
                             fbl::RefPtr<l2cap::Channel> att_chan) = 0;

  // Unregisters the GATT profile connection to the peer with Id |peer_id|.
  virtual void RemoveConnection(PeerId peer_id) = 0;

  // ==============
  // Local Services
  // ==============
  //
  // The methods below are for managing local GATT services that are available
  // to data bearers in the server role.

  // Registers the GATT service hierarchy represented by |service| with the
  // local attribute database. Once successfully registered, the service will
  // be available to remote clients.
  //
  // Objects under |service| must have unique identifiers to aid in value
  // request handling. These identifiers will be passed to |read_handler| and
  // |write_handler|.
  //
  // The provided handlers will be called to handle remote initiated
  // transactions targeting the service. These handlers will be run on the
  // on the GATT dispatcher.
  //
  // This method returns an opaque identifier on successful registration,
  // which can be used by the caller to refer to the service in the future. This
  // ID will be returned via |callback| which run on the GATT dispatcher.
  //
  // Returns |kInvalidId| on failure. Registration can fail if the attribute
  // database has run out of handles or if the hierarchy contains
  // characteristics or descriptors with repeated IDs.
  using ServiceIdCallback = fit::function<void(IdType)>;
  virtual void RegisterService(ServicePtr service, ServiceIdCallback callback,
                               ReadHandler read_handler,
                               WriteHandler write_handler,
                               ClientConfigCallback ccc_callback) = 0;

  // Unregisters the GATT service hierarchy identified by |service_id|. Has no
  // effect if |service_id| is not a registered id.
  virtual void UnregisterService(IdType service_id) = 0;

  // Sends a characteristic handle-value notification to a peer that has
  // configured the characteristic for notifications or indications. Does
  // nothing if the given peer has not configured the characteristic.
  //
  // |service_id|: The GATT service that the characteristic belongs to.
  // |chrc_id|: The GATT characteristic that will be notified.
  // |peer_id|: ID of the peer that the notification/indication will be sent to.
  // |value|: The attribute value that will be included in the notification.
  // |indicate|: If true, an indication will be sent.
  //
  // This method can only be called on the GATT task runner.
  //
  // TODO(armansito): Revise this API to involve fewer lookups (NET-483).
  // TODO(armansito): Fix this to notify all registered peers when |peer_id| is
  // empty (NET-589).
  virtual void SendNotification(IdType service_id, IdType chrc_id,
                                PeerId peer_id, ::std::vector<uint8_t> value,
                                bool indicate) = 0;

  // ===============
  // Remote Services
  // ===============
  //
  // The methods below are for interacting with remote GATT services. These
  // methods operate asynchronously.

  // Perform service discovery and initialize remote services for the peer with
  // the given |peer_id|.
  virtual void DiscoverServices(PeerId peer_id) = 0;

  // Register a handler that will be notified when a remote service gets
  // discovered on a connected peer.
  //
  // |watcher| will be posted on an dispatcher if one is provided.
  // Otherwise, it will run on an internal thread and the client is responsible
  // for synchronization.
  using RemoteServiceWatcher =
      fit::function<void(PeerId peer_id, fbl::RefPtr<RemoteService> service)>;
  virtual void RegisterRemoteServiceWatcher(
      RemoteServiceWatcher watcher,
      async_dispatcher_t* dispatcher = nullptr) = 0;

  // Returns the list of remote services that were found on the device with
  // |peer_id|. If |peer_id| was registered but DiscoverServices() has not been
  // called yet, this request will be buffered until remote services have been
  // discovered. If the connection is removed without discovery services,
  // |callback| will be called with an error status. |callback| will always run
  // on the GATT loop.
  virtual void ListServices(PeerId peer_id, std::vector<UUID> uuids,
                            ServiceListCallback callback) = 0;

  // Connects the RemoteService with the given identifier found on the
  // device with |peer_id|. Returns nullptr if the service is not found.
  // |callback| will be run on the given task runner.
  //
  // TODO(armansito): Change this to ConnectToService().
  virtual void FindService(PeerId peer_id, IdType service_id,
                           RemoteServiceCallback callback) = 0;

 protected:
  friend class fbl::RefPtr<GATT>;
  GATT() = default;
  virtual ~GATT() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GATT);
};

}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_H_
