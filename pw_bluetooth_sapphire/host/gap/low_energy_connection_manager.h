// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace hci {
class LocalAddressDelegate;
class Transport;
}  // namespace hci

namespace gap {

namespace internal {
class LowEnergyConnection;
}  // namespace internal

// TODO(armansito): Document the usage pattern.

class LowEnergyConnectionManager;
class PairingDelegate;
class Peer;
class PeerCache;

class LowEnergyConnectionRef final {
 public:
  // Destroying this object releases its reference to the underlying connection.
  ~LowEnergyConnectionRef();

  // Releases this object's reference to the underlying connection.
  void Release();

  // Returns true if the underlying connection is still active.
  bool active() const { return active_; }

  // Sets a callback to be called when the underlying connection is closed.
  void set_closed_callback(fit::closure callback) { closed_cb_ = std::move(callback); }

  PeerId peer_identifier() const { return peer_id_; }
  hci::ConnectionHandle handle() const { return handle_; }

 private:
  friend class LowEnergyConnectionManager;
  friend class internal::LowEnergyConnection;

  LowEnergyConnectionRef(PeerId peer_id, hci::ConnectionHandle handle,
                         fxl::WeakPtr<LowEnergyConnectionManager> manager);

  // Called by LowEnergyConnectionManager when the underlying connection is
  // closed. Notifies |closed_cb_|.
  void MarkClosed();

  bool active_;
  PeerId peer_id_;
  hci::ConnectionHandle handle_;
  fxl::WeakPtr<LowEnergyConnectionManager> manager_;
  fit::closure closed_cb_;
  fxl::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionRef);
};

using LowEnergyConnectionRefPtr = std::unique_ptr<LowEnergyConnectionRef>;

class LowEnergyConnectionManager final {
 public:
  // |hci|: The HCI transport used to track link layer connection events from
  //        the controller.
  // |addr_delegate|: Used to obtain local identity information during pairing
  //                  procedures.
  // |connector|: Adapter object for initiating link layer connections. This
  //              object abstracts the legacy and extended HCI command sets.
  // |peer_cache|: The cache that stores peer peer data. The connection
  //                 manager stores and retrieves pairing data and connection
  //                 parameters to/from the cache. It also updates the
  //                 connection and bonding state of a peer via the cache.
  // |data_domain|: Used to interact with the L2CAP layer.
  // |gatt|: Used to interact with the GATT profile layer.
  LowEnergyConnectionManager(fxl::RefPtr<hci::Transport> hci,
                             hci::LocalAddressDelegate* addr_delegate,
                             hci::LowEnergyConnector* connector, PeerCache* peer_cache,
                             fbl::RefPtr<data::Domain> data_domain, fbl::RefPtr<gatt::GATT> gatt);
  ~LowEnergyConnectionManager();

  // Allows a caller to claim shared ownership over a connection to the
  // requested remote LE peer identified by |peer_id|. Returns
  // false, if |peer_id| is not recognized, otherwise:
  //
  //   * If the requested peer is already connected, this method
  //     asynchronously returns a LowEnergyConnectionRef without sending any
  //     requests to the controller. This is done for both local and remote
  //     initiated connections (i.e. the local adapter can either be in the LE
  //     central or peripheral roles). |callback| always succeeds.
  //
  //   * If the requested peer is NOT connected, then this method initiates a
  //     connection to the requested peer using one of the GAP central role
  //     connection establishment procedures described in Core Spec v5.0, Vol 3,
  //     Part C, Section 9.3. A LowEnergyConnectionRef is asynchronously
  //     returned to the caller once the connection has been set up.
  //
  //     The status of the procedure is reported in |callback| in the case of an
  //     error.
  //
  // |callback| is posted on the creation thread's dispatcher.
  using ConnectionResultCallback = fit::function<void(hci::Status, LowEnergyConnectionRefPtr)>;
  bool Connect(PeerId peer_id, ConnectionResultCallback callback);

  PeerCache* peer_cache() { return peer_cache_; }
  hci::LocalAddressDelegate* local_address_delegate() const { return local_address_delegate_; }

  // Disconnects any existing LE connection to |peer_id|, invalidating all
  // active LowEnergyConnectionRefs. Returns false if the peer can not be
  // disconnected.
  bool Disconnect(PeerId peer_id);

  // Initializes a new connection over the given |link| and returns a connection
  // reference. Returns nullptr if the connection was rejected.
  //
  // |link| must be the result of a remote initiated connection.
  //
  // TODO(armansito): Add an |own_address| parameter for the locally advertised
  // address that was connected to.
  //
  // A link with the given handle should not have been previously registered.
  LowEnergyConnectionRefPtr RegisterRemoteInitiatedLink(hci::ConnectionPtr link);

  // Returns the PairingDelegate currently assigned to this connection manager.
  PairingDelegate* pairing_delegate() const { return pairing_delegate_.get(); }

  // Assigns a new PairingDelegate to handle LE authentication challenges.
  // Replacing an existing pairing delegate cancels all ongoing pairing
  // procedures. If a delegate is not set then all pairing requests will be
  // rejected.
  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate);

  // TODO(armansito): Add a PeerCache::Observer interface and move these
  // callbacks there.

  // Called when the connection parameters on a link have been updated.
  using ConnectionParametersCallback = fit::function<void(const Peer&)>;
  void SetConnectionParametersCallbackForTesting(ConnectionParametersCallback callback);

  // Called when a link with the given handle gets disconnected. This event is
  // guaranteed to be called before invalidating connection references.
  // |callback| is run on the creation thread.
  //
  // NOTE: This is intended ONLY for unit tests. Clients should watch for
  // disconnection events using LowEnergyConnectionRef::set_closed_callback()
  // instead. DO NOT use outside of tests.
  using DisconnectCallback = fit::function<void(hci::ConnectionHandle)>;
  void SetDisconnectCallbackForTesting(DisconnectCallback callback);

  // Sets the timeout interval to be used on future connect requests. The
  // default value is kLECreateConnectionTimeout.
  void set_request_timeout_for_testing(zx::duration value) { request_timeout_ = value; }

  // Callback for hci::Connection, called when the peer disconnects.
  void OnPeerDisconnect(const hci::Connection* connection);

  // Initiates the pairing process. This is expected to only be called during higher-level testing.
  // |peer_id| is the peer to pair to - if the peer is not connected, this function will return
  // false. |pairing_level| determines the security level of the resulting pairing. |cb| will be
  // called upon completion of the pairing. If the security level of the link is already >=
  // |pairing_level|, then nothing will be done, and |cb| will be called with a success status.
  void Pair(PeerId peer_id, sm::SecurityLevel pairing_level, sm::StatusCallback cb);

 private:
  friend class LowEnergyConnectionRef;

  // Mapping from peer identifiers to open LE connections.
  using ConnectionMap = std::unordered_map<PeerId, std::unique_ptr<internal::LowEnergyConnection>>;

  class PendingRequestData {
   public:
    PendingRequestData(const DeviceAddress& address, ConnectionResultCallback first_callback);
    PendingRequestData() = default;
    ~PendingRequestData() = default;

    PendingRequestData(PendingRequestData&&) = default;
    PendingRequestData& operator=(PendingRequestData&&) = default;

    void AddCallback(ConnectionResultCallback cb) { callbacks_.push_back(std::move(cb)); }

    // Notifies all elements in |callbacks| with |status| and the result of
    // |func|.
    using RefFunc = fit::function<LowEnergyConnectionRefPtr()>;
    void NotifyCallbacks(hci::Status status, const RefFunc& func);

    const DeviceAddress& address() const { return address_; }

   private:
    DeviceAddress address_;
    std::list<ConnectionResultCallback> callbacks_;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PendingRequestData);
  };

  // Called by LowEnergyConnectionRef::Release().
  void ReleaseReference(LowEnergyConnectionRef* conn_ref);

  // Called when |connector_| completes a pending request. Initiates a new
  // connection attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Initiates a connection attempt to |peer|.
  void RequestCreateConnection(Peer* peer);

  // Initializes the connection to the peer with the given identifier and
  // returns the initial reference to it. This method is responsible for setting
  // up all data bearers.
  LowEnergyConnectionRefPtr InitializeConnection(PeerId peer_id, hci::ConnectionPtr link);

  // Adds a new connection reference to an existing connection to the peer
  // with the ID |peer_id| and returns it. Returns nullptr if
  // |peer_id| is not recognized.
  LowEnergyConnectionRefPtr AddConnectionRef(PeerId peer_id);

  // Cleans up a connection state. This results in a HCI_Disconnect command if the connection has
  // not already been disconnected, and notifies any referenced LowEnergyConnectionRefs of the
  // disconnection. Marks the corresponding PeerCache entry as disconnected and cleans up all data
  // bearers.
  //
  // |conn_state| will have been removed from the underlying map at the time of
  // a call. Its ownership is passed to the method for disposal.
  //
  // This is also responsible for unregistering the link from managed subsystems
  // (e.g. L2CAP).
  void CleanUpConnection(std::unique_ptr<internal::LowEnergyConnection> conn);

  // Called by |connector_| when a new locally initiated LE connection has been
  // created.
  void RegisterLocalInitiatedLink(hci::ConnectionPtr link);

  // Updates |peer_cache_| with the given |link| and returns the corresponding
  // Peer.
  //
  // Creates a new Peer if |link| matches a peer that did not
  // previously exist in the cache. Otherwise this updates and returns an
  // existing Peer.
  //
  // The returned peer is marked as non-temporary and its connection
  // parameters are updated.
  //
  // Called by RegisterRemoteInitiatedLink() and RegisterLocalInitiatedLink().
  Peer* UpdatePeerWithLink(const hci::Connection& link);

  // Called by |connector_| to indicate the result of a connect request.
  void OnConnectResult(PeerId peer_id, hci::Status status, hci::ConnectionPtr link);

  // Event handler for the HCI LE Connection Update Complete event.
  hci::CommandChannel::EventCallbackResult OnLEConnectionUpdateComplete(
      const hci::EventPacket& event);

  // Called when the preferred connection parameters have been received for a LE
  // peripheral. This can happen in the form of:
  //
  //   1. <<Slave Connection Interval Range>> advertising data field
  //   2. "Peripheral Preferred Connection Parameters" GATT characteristic
  //      (under "GAP" service)
  //   3. HCI LE Remote Connection Parameter Request Event
  //   4. L2CAP Connection Parameter Update request
  //
  // TODO(armansito): Support #1, #2, and #3 above.
  //
  // This method caches |params| for later connection attempts and sends the
  // parameters to the controller if the initializing procedures are complete
  // (since we use more agressing initial parameters for pairing and service
  // discovery, as recommended by the specification in v5.0, Vol 3, Part C,
  // Section 9.3.12.1).
  //
  // |peer_id| uniquely identifies the peer. |handle| represents
  // the logical link that |params| should be applied to.
  void OnNewLEConnectionParams(PeerId peer_id, hci::ConnectionHandle handle,
                               const hci::LEPreferredConnectionParameters& params);

  // Tells the controller to use the given connection |params| on the given
  // logical link |handle|.
  void UpdateConnectionParams(hci::ConnectionHandle handle,
                              const hci::LEPreferredConnectionParameters& params);

  // Returns an iterator into |connections_| if a connection is found that
  // matches the given logical link |handle|. Otherwise, returns an iterator
  // that is equal to |connections_.end()|.
  //
  // The general rules of validity around std::unordered_map::iterator apply to
  // the returned value.
  ConnectionMap::iterator FindConnection(hci::ConnectionHandle handle);

  fxl::RefPtr<hci::Transport> hci_;

  // The pairing delegate used for authentication challenges. If nullptr, all
  // pairing requests will be rejected.
  fxl::WeakPtr<PairingDelegate> pairing_delegate_;

  // Time after which a connection attempt is considered to have timed out. This
  // is configurable to allow unit tests to set a shorter value.
  zx::duration request_timeout_;

  // The dispatcher for all asynchronous tasks.
  async_dispatcher_t* dispatcher_;

  // The peer cache is used to look up and persist remote peer data that is
  // relevant during connection establishment (such as the address, preferred
  // connection parameters, etc). Expected to outlive this instance.
  PeerCache* peer_cache_;  // weak

  // The reference to the data domain, used to interact with the L2CAP layer to
  // manage LE logical links, fixed channels, and LE-specific L2CAP signaling
  // events (e.g. connection parameter update).
  fbl::RefPtr<data::Domain> data_domain_;

  // The GATT layer reference, used to add and remove ATT data bearers and
  // service discovery.
  fbl::RefPtr<gatt::GATT> gatt_;

  // Local GATT service registry.
  std::unique_ptr<gatt::LocalServiceManager> gatt_registry_;

  // Event handler ID for the HCI LE Connection Update Complete event.
  hci::CommandChannel::EventHandlerId conn_update_cmpl_handler_id_;

  // Callbacks used by unit tests to observe connection state events.
  ConnectionParametersCallback test_conn_params_cb_;
  DisconnectCallback test_disconn_cb_;

  // Outstanding connection requests based on remote peer ID.
  std::unordered_map<PeerId, PendingRequestData> pending_requests_;

  // Mapping from peer identifiers to currently open LE connections.
  ConnectionMap connections_;

  // Performs the Direct Connection Establishment procedure. |connector_| must
  // out-live this connection manager.
  hci::LowEnergyConnector* connector_;  // weak

  // Address manager is used to obtain local identity information during pairing
  // procedures. Expected to outlive this instance.
  hci::LocalAddressDelegate* local_address_delegate_;  // weak

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_MANAGER_H_
