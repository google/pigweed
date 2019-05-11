// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_discoverer.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace hci {
class SequentialCommandRunner;
class Transport;
}  // namespace hci

namespace gap {

class PairingDelegate;
class PeerCache;
class BrEdrConnectionManager;

// Represents an established Br/Edr connection, after we have performed
// interrogation
class BrEdrConnection final {
 public:
  BrEdrConnection(PeerId peer_id, std::unique_ptr<hci::Connection> link)
      : link_(std::move(link)), peer_id_(peer_id) {}

  const hci::Connection& link() const { return *link_; }
  hci::Connection& link() { return *link_; }

  PeerId peer_id() const { return peer_id_; }

 private:
  std::unique_ptr<hci::Connection> link_;
  PeerId peer_id_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnection);
};

// Manages all activity related to connections in the BR/EDR section of the
// controller, including whether the peer can be connected to, incoming
// connections, and initiating connections.
class BrEdrConnectionManager final {
 public:
  BrEdrConnectionManager(fxl::RefPtr<hci::Transport> hci, PeerCache* peer_cache,
                         DeviceAddress local_address,
                         fbl::RefPtr<data::Domain> data_domain,
                         bool use_interlaced_scan);
  ~BrEdrConnectionManager();

  // Set whether this host is connectable
  void SetConnectable(bool connectable, hci::StatusCallback status_cb);

  // Assigns a new PairingDelegate to handle BR/EDR authentication challenges.
  // Replacing an existing pairing delegate cancels all ongoing pairing
  // procedures. If a delegate is not set then all pairing requests will be
  // rejected.
  void SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate);

  // Retrieves the peer id that is connected to the connection |handle|.
  // Returns kInvalidPeerId if no such peer exists.
  PeerId GetPeerId(hci::ConnectionHandle handle) const;

  // Opens a new L2CAP channel to service |psm| on |peer_id|. Returns false if
  // the peer is not already connected.
  using SocketCallback = fit::function<void(zx::socket)>;
  bool OpenL2capChannel(PeerId peer_id, l2cap::PSM psm, SocketCallback cb,
                        async_dispatcher_t* dispatcher);

  // Add a service search to be performed on new connected remote peers.
  // This search will happen on every peer connection.
  // |callback| will be called with the |attributes| that exist in the service
  // entry on the remote SDP server.  This callback is called in the same
  // dispatcher as BrEdrConnectionManager was created.
  // If |attributes| is empty, all attributes on the server will be returned.
  // Returns a SearchId which can be used to remove the search later.
  // Identical searches will perform the same search for each search added.
  // TODO(BT-785): Make identifcal searches just search once
  using SearchCallback = sdp::ServiceDiscoverer::ResultCallback;
  using SearchId = sdp::ServiceDiscoverer::SearchId;
  SearchId AddServiceSearch(const UUID& uuid,
                            std::unordered_set<sdp::AttributeId> attributes,
                            SearchCallback callback);

  // Remove a search previously added with AddServiceSearch()
  // Returns true if a search was removed.
  // This function is idempotent.
  bool RemoveServiceSearch(SearchId id);

  using ConnectResultCallback =
      fit::function<void(hci::Status, BrEdrConnection*)>;

  // Initiates an outgoing Create Connection Request to attempt to connect to
  // the peer identified by |peer_id|. Returns false if the connection
  // request was invalid, otherwise returns true and |callback| will be called
  // with the result of the procedure, whether successful or not
  // TODO(BT-820) - implement a timeout
  [[nodiscard]] bool Connect(PeerId peer_id, ConnectResultCallback callback);

  // Intialize a GAP-level ACL connection from the hci connection_handle
  void InitializeConnection(DeviceAddress addr,
                            hci::ConnectionHandle connection_handle);

  // Called when an outgoing connection fails to establish
  void OnConnectFailure(hci::Status status, PeerId peer_id);

  // Called to cancel an outgoing connection request
  void SendCreateConnectionCancelCommand(DeviceAddress addr);

  // Disconnects any existing BR/EDR connection to |peer_id|. Returns false if
  // |peer_id| is not a recognized BR/EDR peer or the corresponding peer is
  // not connected.
  bool Disconnect(PeerId peer_id);

 private:
  // Reads the controller page scan settings.
  void ReadPageScanSettings();

  // Writes page scan parameters to the controller.
  // If |interlaced| is true, and the controller does not support interlaced
  // page scan mode, standard mode is used.
  void WritePageScanSettings(uint16_t interval, uint16_t window,
                             bool interlaced, hci::StatusCallback cb);

  // Helper to register an event handler to run.
  hci::CommandChannel::EventHandlerId AddEventHandler(
      const hci::EventCode& code, hci::CommandChannel::EventCallback cb);

  // Find the handle for a connection to |peer_id|. Returns nullopt if no BR/EDR
  // |peer_id| is connected.
  std::optional<std::pair<hci::ConnectionHandle, BrEdrConnection*>>
  FindConnectionById(PeerId peer_id);

  // Callbacks for registered events
  void OnConnectionRequest(const hci::EventPacket& event);
  void OnConnectionComplete(const hci::EventPacket& event);
  void OnDisconnectionComplete(const hci::EventPacket& event);
  void OnLinkKeyRequest(const hci::EventPacket& event);
  void OnLinkKeyNotification(const hci::EventPacket& event);
  void OnIOCapabilitiesRequest(const hci::EventPacket& event);
  void OnUserConfirmationRequest(const hci::EventPacket& event);

  // Called once interrogation is complete to establish a BrEdrConnection and,
  // if in response to an outgoing connection request, completes the request
  void EstablishConnection(Peer* peer, hci::Status status,
                           std::unique_ptr<hci::Connection> conn_ptr);

  Peer* FindOrInitPeer(DeviceAddress addr);

  // Called when we complete a pending request. Initiates a new connection
  // attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Called when a request times out waiting for a connection complete packet,
  // *after* the command status was received. This is responsible for canceling
  // the request and initiating the next one in the queue
  void OnRequestTimeout();

  // Cleanup a connection which has been deliberately disconnected, or had all
  // references to it dropped
  void CleanupConnection(hci::ConnectionHandle handle, BrEdrConnection& conn,
                         bool link_already_closed);

  using ConnectionMap =
      std::unordered_map<hci::ConnectionHandle, BrEdrConnection>;

  fxl::RefPtr<hci::Transport> hci_;
  std::unique_ptr<hci::SequentialCommandRunner> hci_cmd_runner_;

  // Peer cache is used to look up parameters for connecting to peers and
  // update the state of connected peers as well as introduce unknown peers.
  // This object must outlive this instance.
  PeerCache* cache_;

  const DeviceAddress local_address_;

  fbl::RefPtr<data::Domain> data_domain_;

  // Interregator for new connections to pass.
  BrEdrInterrogator interrogator_;

  // Discoverer for SDP services
  sdp::ServiceDiscoverer discoverer_;

  // Holds the connections that are active.
  ConnectionMap connections_;

  // Handler ID for connection events
  hci::CommandChannel::EventHandlerId conn_complete_handler_id_;
  hci::CommandChannel::EventHandlerId conn_request_handler_id_;
  hci::CommandChannel::EventHandlerId disconn_cmpl_handler_id_;

  // Handler IDs for pairing events
  hci::CommandChannel::EventHandlerId link_key_request_handler_id_;
  hci::CommandChannel::EventHandlerId link_key_notification_handler_id_;
  hci::CommandChannel::EventHandlerId io_cap_req_handler_id_;
  hci::CommandChannel::EventHandlerId user_conf_handler_id_;

  // The current page scan parameters of the controller.
  // Set to 0 when non-connectable.
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;
  hci::PageScanType page_scan_type_;
  bool use_interlaced_scan_;

  // Outstanding connection requests based on remote peer ID.
  std::unordered_map<PeerId, ConnectionRequest<BrEdrConnection*>>
      connection_requests_;

  std::optional<hci::BrEdrConnectionRequest> pending_request_;

  // Time after which a connection attempt is considered to have timed out.
  zx::duration request_timeout_;

  // The dispatcher that all commands are queued on.
  async_dispatcher_t* dispatcher_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrConnectionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnectionManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
