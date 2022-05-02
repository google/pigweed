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

#include "lib/fitx/result.h"
#include "low_energy_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/common/windowed_inspect_numeric_property.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_discovery_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connector.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
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

enum class LowEnergyDisconnectReason : uint8_t {
  // Explicit disconnect request
  kApiRequest,
  // An internal error was encountered
  kError,
};

// LowEnergyConnectionManager is responsible for connecting and initializing new connections,
// interrogating connections, intiating pairing, and disconnecting connections.
class LowEnergyConnectionManager final {
 public:
  // Duration after which connection failures are removed from Inspect.
  static constexpr zx::duration kInspectRecentConnectionFailuresExpiryDuration = zx::min(10);

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
                             l2cap::ChannelManager* l2cap, fxl::WeakPtr<gatt::GATT> gatt,
                             fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager,
                             sm::SecurityManagerFactory sm_creator);
  ~LowEnergyConnectionManager();

  // Allows a caller to claim shared ownership over a connection to the requested remote LE peer
  // identified by |peer_id|.
  //   * If |peer_id| is not recognized, |callback| is called with an error.
  //
  //   * If the requested peer is already connected, |callback| is called with a
  //     LowEnergyConnectionHandle immediately.
  //     This is done for both local and remote initiated connections (i.e. the local adapter
  //     can either be in the LE central or peripheral roles).
  //
  //   * If the requested peer is NOT connected, then this method initiates a
  //     connection to the requested peer using the internal::LowEnergyConnector. See that class's
  //     documentation for a more detailed overview of the Connection process. A
  //     LowEnergyConnectionHandle is asynchronously returned to the caller once the connection has
  //     been set up.
  //
  // The status of the procedure is reported in |callback| in the case of an
  // error.
  using ConnectionResult = fitx::result<HostError, std::unique_ptr<LowEnergyConnectionHandle>>;
  using ConnectionResultCallback = fit::function<void(ConnectionResult)>;
  void Connect(PeerId peer_id, ConnectionResultCallback callback,
               LowEnergyConnectionOptions connection_options);

  hci::LocalAddressDelegate* local_address_delegate() const { return local_address_delegate_; }

  // Disconnects any existing or pending LE connection to |peer_id|, invalidating all
  // active LowEnergyConnectionHandles. Returns false if the peer can not be
  // disconnected.
  bool Disconnect(PeerId peer_id,
                  LowEnergyDisconnectReason reason = LowEnergyDisconnectReason::kApiRequest);

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
  void RegisterRemoteInitiatedLink(std::unique_ptr<hci::LowEnergyConnection> link,
                                   sm::BondableMode bondable_mode,
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
  using DisconnectCallback = fit::function<void(hci_spec::ConnectionHandle)>;
  void SetDisconnectCallbackForTesting(DisconnectCallback callback);

  // Sets the timeout interval to be used on future connect requests. The
  // default value is kLECreateConnectionTimeout.
  void set_request_timeout_for_testing(zx::duration value) { request_timeout_ = value; }

  // Callback for hci::Connection, called when the peer disconnects.
  // |reason| is used to control retry logic.
  void OnPeerDisconnect(const hci::Connection* connection, hci_spec::StatusCode reason);

  // Initiates the pairing process. Expected to only be called during higher-level testing.
  //   |peer_id|: the peer to pair to - if the peer is not connected, |cb| is called with an error.
  //   |pairing_level|: determines the security level of the pairing. **Note**: If the security
  //                    level of the link is already >= |pairing level|, no pairing takes place.
  //   |bondable_mode|: sets the bonding mode of this connection. A device in bondable mode forms a
  //                    bond to the peer upon pairing, assuming the peer is also in bondable mode.
  //                    A device in non-bondable mode will not allow pairing that forms a bond.
  //   |cb|: callback called upon completion of this function, whether pairing takes place or not.
  void Pair(PeerId peer_id, sm::SecurityLevel pairing_level, sm::BondableMode bondable_mode,
            sm::ResultFunction<> cb);

  // Sets the LE security mode of the local device (see v5.2 Vol. 3 Part C Section 10.2). If set to
  // SecureConnectionsOnly, any currently encrypted links not meeting the requirements of Security
  // Mode 1 Level 4 will be disconnected.
  void SetSecurityMode(LESecurityMode mode);

  // Attach manager inspect node as a child node of |parent|.
  void AttachInspect(inspect::Node& parent, std::string name);

  LESecurityMode security_mode() const { return security_mode_; }
  sm::SecurityManagerFactory sm_factory_func() const { return sm_factory_func_; }

 private:
  friend class internal::LowEnergyConnection;

  // Mapping from peer identifiers to open LE connections.
  using ConnectionMap = std::unordered_map<PeerId, std::unique_ptr<internal::LowEnergyConnection>>;

  // Called by LowEnergyConnectionHandle::Release().
  void ReleaseReference(LowEnergyConnectionHandle* handle);

  // Initiates a new connection attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Called by internal::LowEnergyConnector to indicate the result of a local connect request.
  void OnLocalInitiatedConnectResult(
      hci::Result<std::unique_ptr<internal::LowEnergyConnection>> result);

  // Called by internal::LowEnergyConnector to indicate the result of a remote connect request.
  void OnRemoteInitiatedConnectResult(
      PeerId peer_id, hci::Result<std::unique_ptr<internal::LowEnergyConnection>> result);

  // Either report an error to clients or initialize the connection and report success to clients.
  void ProcessConnectResult(hci::Result<std::unique_ptr<internal::LowEnergyConnection>> result,
                            internal::LowEnergyConnectionRequest request);

  // Finish setting up connection, adding to |connections_| map, and notifying clients.
  bool InitializeConnection(std::unique_ptr<internal::LowEnergyConnection> connection,
                            internal::LowEnergyConnectionRequest request);

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
  Peer* UpdatePeerWithLink(const hci::LowEnergyConnection& link);

  // Called when the peer disconnects with a "Connection Failed to be Established" error.
  // Cleans up the existing connection and adds the connection request back to the queue for a
  // retry.
  void CleanUpAndRetryConnection(std::unique_ptr<internal::LowEnergyConnection> connection);

  // Returns an iterator into |connections_| if a connection is found that
  // matches the given logical link |handle|. Otherwise, returns an iterator
  // that is equal to |connections_.end()|.
  //
  // The general rules of validity around std::unordered_map::iterator apply to
  // the returned value.
  ConnectionMap::iterator FindConnection(hci_spec::ConnectionHandle handle);

  fxl::WeakPtr<hci::Transport> hci_;

  // The pairing delegate used for authentication challenges. If nullptr, all
  // pairing requests will be rejected.
  fxl::WeakPtr<PairingDelegate> pairing_delegate_;

  // The GAP LE security mode of the device (v5.2 Vol. 3 Part C 10.2).
  LESecurityMode security_mode_;

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

  // The reference to L2CAP, used to interact with the L2CAP layer to
  // manage LE logical links, fixed channels, and LE-specific L2CAP signaling
  // events (e.g. connection parameter update).
  l2cap::ChannelManager* l2cap_;

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

  struct RequestAndConnector {
    internal::LowEnergyConnectionRequest request;
    std::unique_ptr<internal::LowEnergyConnector> connector;
  };
  // The in-progress locally initiated connection request, if any.
  std::optional<RequestAndConnector> current_request_;

  // Active connectors for remote connection requests.
  std::unordered_map<PeerId, RequestAndConnector> remote_connectors_;

  // For passing to internal::LowEnergyConnector. |hci_connector_| must
  // out-live this connection manager.
  hci::LowEnergyConnector* hci_connector_;  // weak

  // Address manager is used to obtain local identity information during pairing
  // procedures. Expected to outlive this instance.
  hci::LocalAddressDelegate* local_address_delegate_;  // weak

  // True if the connection manager is performing a scan for a peer before connecting.
  bool scanning_ = false;

  struct InspectProperties {
    // Count of connection failures in the past 10 minutes.
    WindowedInspectIntProperty recent_connection_failures{
        kInspectRecentConnectionFailuresExpiryDuration};

    UintMetricCounter outgoing_connection_success_count_;
    UintMetricCounter outgoing_connection_failure_count_;
    UintMetricCounter incoming_connection_success_count_;
    UintMetricCounter incoming_connection_failure_count_;

    UintMetricCounter disconnect_explicit_disconnect_count_;
    UintMetricCounter disconnect_link_error_count_;
    UintMetricCounter disconnect_zero_ref_count_;
    UintMetricCounter disconnect_remote_disconnection_count_;
  };
  InspectProperties inspect_properties_;
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
