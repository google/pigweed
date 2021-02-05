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

#include "low_energy_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
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

// LowEnergyConnectionManager is responsible for connecting and initializing new connections,
// interrogating connections, intiating pairing, and disconnecting connections.
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
  // |l2cap|: Used to interact with the L2CAP layer.
  // |gatt|: Used to interact with the GATT profile layer.
  LowEnergyConnectionManager(fxl::WeakPtr<hci::Transport> hci,
                             hci::LocalAddressDelegate* addr_delegate,
                             hci::LowEnergyConnector* connector, PeerCache* peer_cache,
                             fbl::RefPtr<l2cap::L2cap> l2cap, fxl::WeakPtr<gatt::GATT> gatt,
                             fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager,
                             sm::SecurityManagerFactory sm_creator);
  ~LowEnergyConnectionManager();

  // Options for the |Connect| method.

  // Allows a caller to claim shared ownership over a connection to the requested remote LE peer
  // identified by |peer_id|.
  //   * If |peer_id| is not recognized, |callback| is called with an error.
  //
  //   * If the requested peer is already connected, |callback| is called with a
  //     LowEnergyConnectionHandle after interrogation (if necessary).
  //     This is done for both local and remote initiated connections (i.e. the local adapter
  //     can either be in the LE central or peripheral roles).
  //
  //   * If the requested peer is NOT connected, then this method initiates a
  //     connection to the requested peer using one of the GAP central role
  //     connection establishment procedures described in Core Spec v5.0, Vol 3,
  //     Part C, Section 9.3. The peer is then interrogated. A LowEnergyConnectionHandle is
  //     asynchronously returned to the caller once the connection has been set up.
  //
  // The status of the procedure is reported in |callback| in the case of an
  // error.
  using ConnectionResult = fit::result<std::unique_ptr<LowEnergyConnectionHandle>, HostError>;
  using ConnectionResultCallback = fit::function<void(ConnectionResult)>;
  void Connect(PeerId peer_id, ConnectionResultCallback callback,
               LowEnergyConnectionOptions connection_options);

  PeerCache* peer_cache() { return peer_cache_; }
  hci::LocalAddressDelegate* local_address_delegate() const { return local_address_delegate_; }

  // Disconnects any existing or pending LE connection to |peer_id|, invalidating all
  // active LowEnergyConnectionHandles. Returns false if the peer can not be
  // disconnected.
  bool Disconnect(PeerId peer_id);

  // Initializes a new connection over the given |link| and asynchronously returns a connection
  // reference.
  //
  // |link| must be the result of a remote initiated connection.
  //
  // |callback| will be called with a connection status and connection reference. The connection
  // reference will be nullptr if the connection was rejected (as indicated by a failure status).
  //
  // TODO(armansito): Add an |own_address| parameter for the locally advertised
  // address that was connected to.
  //
  // A link with the given handle should not have been previously registered.
  void RegisterRemoteInitiatedLink(hci::ConnectionPtr link, sm::BondableMode bondable_mode,
                                   ConnectionResultCallback callback);

  // Returns the PairingDelegate currently assigned to this connection manager.
  PairingDelegate* pairing_delegate() const { return pairing_delegate_.get(); }

  // Assigns a new PairingDelegate to handle LE authentication challenges.
  // Replacing an existing pairing delegate cancels all ongoing pairing
  // procedures. If a delegate is not set then all pairing requests will be
  // rejected.
  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate);

  // TODO(armansito): Add a PeerCache::Observer interface and move these
  // callbacks there.

  // Called when a link with the given handle gets disconnected. This event is
  // guaranteed to be called before invalidating connection references.
  // |callback| is run on the creation thread.
  //
  // NOTE: This is intended ONLY for unit tests. Clients should watch for
  // disconnection events using LowEnergyConnectionHandle::set_closed_callback()
  // instead. DO NOT use outside of tests.
  using DisconnectCallback = fit::function<void(hci::ConnectionHandle)>;
  void SetDisconnectCallbackForTesting(DisconnectCallback callback);

  // Sets the timeout interval to be used on future connect requests. The
  // default value is kLECreateConnectionTimeout.
  void set_request_timeout_for_testing(zx::duration value) { request_timeout_ = value; }

  // Callback for hci::Connection, called when the peer disconnects.
  // |reason| is used to control retry logic.
  void OnPeerDisconnect(const hci::Connection* connection, hci::StatusCode reason);

  // Initiates the pairing process. Expected to only be called during higher-level testing.
  //   |peer_id|: the peer to pair to - if the peer is not connected, |cb| is called with an error.
  //   |pairing_level|: determines the security level of the pairing. **Note**: If the security
  //                    level of the link is already >= |pairing level|, no pairing takes place.
  //   |bondable_mode|: sets the bonding mode of this connection. A device in bondable mode forms a
  //                    bond to the peer upon pairing, assuming the peer is also in bondable mode.
  //                    A device in non-bondable mode will not allow pairing that forms a bond.
  //   |cb|: callback called upon completion of this function, whether pairing takes place or not.
  void Pair(PeerId peer_id, sm::SecurityLevel pairing_level, sm::BondableMode bondable_mode,
            sm::StatusCallback cb);

  // Sets the LE security mode of the local device (see v5.2 Vol. 3 Part C Section 10.2). If set to
  // SecureConnectionsOnly, any currently encrypted links not meeting the requirements of Security
  // Mode 1 Level 4 will be disconnected.
  void SetSecurityMode(LeSecurityMode mode);

  // Attach manager inspect node as a child node of |parent|.
  static constexpr const char* kInspectNodeName = "low_energy_connection_manager";
  void AttachInspect(inspect::Node& parent);

  LeSecurityMode security_mode() const { return security_mode_; }
  sm::SecurityManagerFactory sm_factory_func() const { return sm_factory_func_; }

 private:
  friend class LowEnergyConnectionHandle;

  // Mapping from peer identifiers to open LE connections.
  using ConnectionMap = std::unordered_map<PeerId, std::unique_ptr<internal::LowEnergyConnection>>;

  // Called by LowEnergyConnectionHandle::Release().
  void ReleaseReference(LowEnergyConnectionHandle* conn_ref);

  // Called when |connector_| completes a pending request. Initiates a new
  // connection attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Starts scanning for peer to connect to. When the peer is found, initiates a connection attempt.
  void StartScanningForPeer(Peer* peer);

  // Called when scanning for the pending peer with id |peer_id| successfully starts.
  // |session| must not be nullptr.
  //
  // Starts a scan timeout and filters scan results for one that matches |peer_id|. When a matching
  // result is found, initiates a connection attempt.
  void OnScanStart(PeerId peer_id, LowEnergyDiscoverySessionPtr session);

  // Initiates a connection attempt to |peer|.
  void RequestCreateConnection(Peer* peer);

  // Initializes the connection to the peer with the given identifier and starts interrogation.
  // |request| will be notified when interrogation completes.
  // This method is responsible for setting up all data bearers.
  // Returns true on success (a LowEnergyConnection was created and interrogation started), or false
  // otherwise.
  bool InitializeConnection(PeerId peer_id, hci::ConnectionPtr link,
                            internal::LowEnergyConnectionRequest request);

  // Called upon interrogation completion for a new connection.
  // Notifies connection request callbacks.
  void OnInterrogationComplete(PeerId peer_id, hci::Status status);

  // Cleans up a connection state. This results in a HCI_Disconnect command if the connection has
  // not already been disconnected, and notifies any referenced LowEnergyConnectionHandles of the
  // disconnection. Marks the corresponding PeerCache entry as disconnected and cleans up all data
  // bearers.
  //
  // |conn_state| will have been removed from the underlying map at the time of
  // a call. Its ownership is passed to the method for disposal.
  //
  // This is also responsible for unregistering the link from managed subsystems
  // (e.g. L2CAP).
  void CleanUpConnection(std::unique_ptr<internal::LowEnergyConnection> conn);

  // Called by |OnConnectResult()| when a new locally initiated LE connection has been
  // created. Notifies pending connection request callbacks when connection initialization
  // completes.
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

  // Called when the peer disconnects with a "Connection Failed to be Established" error.
  // Cleans up the existing connection and adds the connection request back to the queue for a
  // retry.
  void CleanUpAndRetryConnection(std::unique_ptr<internal::LowEnergyConnection> connection);

  // Cancel the request corresponding to |peer_id|, notify callbacks, and try to create the next
  // connection.
  void CancelPendingRequest(PeerId peer_id);

  // Returns an iterator into |connections_| if a connection is found that
  // matches the given logical link |handle|. Otherwise, returns an iterator
  // that is equal to |connections_.end()|.
  //
  // The general rules of validity around std::unordered_map::iterator apply to
  // the returned value.
  ConnectionMap::iterator FindConnection(hci::ConnectionHandle handle);

  fxl::WeakPtr<hci::Transport> hci_;

  // The pairing delegate used for authentication challenges. If nullptr, all
  // pairing requests will be rejected.
  fxl::WeakPtr<PairingDelegate> pairing_delegate_;

  // The GAP LE security mode of the device (v5.2 Vol. 3 Part C 10.2).
  LeSecurityMode security_mode_;

  // The function used to create each channel's SecurityManager implementation.
  sm::SecurityManagerFactory sm_factory_func_;

  // Time after which a connection attempt is considered to have timed out. This
  // is configurable to allow unit tests to set a shorter value.
  zx::duration request_timeout_;

  // Task called after a peer scan attempt is times out.
  std::optional<async::TaskClosure> scan_timeout_task_;

  // The dispatcher for all asynchronous tasks.
  async_dispatcher_t* dispatcher_;

  // The peer cache is used to look up and persist remote peer data that is
  // relevant during connection establishment (such as the address, preferred
  // connection parameters, etc). Expected to outlive this instance.
  PeerCache* peer_cache_;  // weak

  // The reference to the data domain, used to interact with the L2CAP layer to
  // manage LE logical links, fixed channels, and LE-specific L2CAP signaling
  // events (e.g. connection parameter update).
  fbl::RefPtr<l2cap::L2cap> l2cap_;

  // The GATT layer reference, used to add and remove ATT data bearers and
  // service discovery.
  fxl::WeakPtr<gatt::GATT> gatt_;

  // Local GATT service registry.
  std::unique_ptr<gatt::LocalServiceManager> gatt_registry_;

  fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager_;

  // Callbacks used by unit tests to observe connection state events.
  DisconnectCallback test_disconn_cb_;

  // Outstanding connection requests based on remote peer ID.
  std::unordered_map<PeerId, internal::LowEnergyConnectionRequest> pending_requests_;

  // Mapping from peer identifiers to currently open LE connections.
  ConnectionMap connections_;

  // Performs the Direct Connection Establishment procedure. |connector_| must
  // out-live this connection manager.
  hci::LowEnergyConnector* connector_;  // weak

  // Address manager is used to obtain local identity information during pairing
  // procedures. Expected to outlive this instance.
  hci::LocalAddressDelegate* local_address_delegate_;  // weak

  // Sends HCI commands that request version and feature support information from peer
  // controllers.
  LowEnergyInterrogator interrogator_;

  // True if the connection manager is performing a scan for a peer before connecting.
  bool scanning_ = false;

  inspect::Node inspect_node_;
  // Container node for pending request nodes.
  inspect::Node inspect_pending_requests_node_;
  // container node for connection nodes.
  inspect::Node inspect_connections_node_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnectionManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_CONNECTION_MANAGER_H_
