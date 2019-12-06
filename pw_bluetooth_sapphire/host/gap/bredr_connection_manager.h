// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_

#include <functional>
#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
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

// Represents a connection that is currently open with the controller (i.e. after receiving a
// Connection Complete and before either user disconnection or Disconnection Complete).
class BrEdrConnection final {
 public:
  using Request = ConnectionRequest<BrEdrConnection*>;
  BrEdrConnection(BrEdrConnectionManager* connection_manager, PeerId peer_id,
                  std::unique_ptr<hci::Connection> link, std::optional<Request> request);

  ~BrEdrConnection();

  BrEdrConnection(BrEdrConnection&&) = default;
  BrEdrConnection& operator=(BrEdrConnection&&) = default;

  // Called after interrogation completes to mark this connection as available for upper layers,
  // i.e. L2CAP on |domain|. Also signals any requesters with a successful status and this
  // connection. If not called and this connection is deleted (e.g. by disconnection), requesters
  // will be signaled with |HostError::kNotSupported| (to indicate interrogation error).
  void Start(data::Domain& domain);

  // If |Start| has been called, opens an L2CAP channel on the Domain provided. Otherwise, calls
  // |cb| with a |ZX_HANDLE_INVALID| socket on |dispatcher|.
  void OpenL2capChannel(l2cap::PSM psm, data::Domain::SocketCallback cb,
                        async_dispatcher_t* dispatcher);

  const hci::Connection& link() const { return *link_; }
  hci::Connection& link() { return *link_; }
  PeerId peer_id() const { return peer_id_; }
  PairingState& pairing_state() { return pairing_state_; }

 private:
  PeerId peer_id_;
  std::unique_ptr<hci::Connection> link_;
  PairingState pairing_state_;
  std::optional<Request> request_;
  std::optional<std::reference_wrapper<data::Domain>> domain_;  // clear until Start is called

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnection);
};

// Manages all activity related to connections in the BR/EDR section of the
// controller, including whether the peer can be connected to, incoming
// connections, and initiating connections.
//
// There are two flows for destroying connections: explicit local disconnections, and peer
// disconnections. When the connection is disconnected explicitly with |Disconnect()|, the
// connection is immediately cleaned up and removed from the internal |connections_| map and owned
// by itself until the HCI Disconnection Complete event is received by the underlying
// hci::Connection object. When the peer disconnects, the |OnPeerDisconnect()| callback is
// called by the underlying hci::Connection object and the connection is cleaned up and removed from
// the internal |connections_| map.
class BrEdrConnectionManager final {
 public:
  BrEdrConnectionManager(fxl::RefPtr<hci::Transport> hci, PeerCache* peer_cache,
                         DeviceAddress local_address, fbl::RefPtr<data::Domain> data_domain,
                         bool use_interlaced_scan);
  ~BrEdrConnectionManager();

  // Set whether this host is connectable
  void SetConnectable(bool connectable, hci::StatusCallback status_cb);

  // Returns the PairingDelegate currently assigned to this connection manager.
  PairingDelegate* pairing_delegate() const { return pairing_delegate_.get(); }

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
  SearchId AddServiceSearch(const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
                            SearchCallback callback);

  // Remove a search previously added with AddServiceSearch()
  // Returns true if a search was removed.
  // This function is idempotent.
  bool RemoveServiceSearch(SearchId id);

  using ConnectResultCallback = fit::function<void(hci::Status, BrEdrConnection*)>;

  // Initiates an outgoing Create Connection Request to attempt to connect to
  // the peer identified by |peer_id|. Returns false if the connection
  // request was invalid, otherwise returns true and |callback| will be called
  // with the result of the procedure, whether successful or not
  // TODO(BT-820) - implement a timeout
  [[nodiscard]] bool Connect(PeerId peer_id, ConnectResultCallback callback);

  // Called when the controller can not begin a new connection.
  void OnConnectFailure(hci::Status status, PeerId peer_id);

  // Called to cancel an outgoing connection request
  void SendCreateConnectionCancelCommand(DeviceAddress addr);

  // Disconnects any existing BR/EDR connection to |peer_id|. Returns true if
  // the peer is disconnected, false if the peer can not be disconnected.
  bool Disconnect(PeerId peer_id);

  // Callback for hci::Connection. Called when the peer disconnects.
  void OnPeerDisconnect(const hci::Connection* connection);

 private:
  // Reads the controller page scan settings.
  void ReadPageScanSettings();

  // Writes page scan parameters to the controller.
  // If |interlaced| is true, and the controller does not support interlaced
  // page scan mode, standard mode is used.
  void WritePageScanSettings(uint16_t interval, uint16_t window, bool interlaced,
                             hci::StatusCallback cb);

  // Helper to register an event handler to run.
  hci::CommandChannel::EventHandlerId AddEventHandler(const hci::EventCode& code,
                                                      hci::CommandChannel::EventCallback cb);

  // Find the handle for a connection to |peer_id|. Returns nullopt if no BR/EDR
  // |peer_id| is connected.
  std::optional<std::pair<hci::ConnectionHandle, BrEdrConnection*>> FindConnectionById(
      PeerId peer_id);

  // Find the handle for a connection to |bd_addr|. Returns nullopt if no BR/EDR
  // |bd_addr| is connected.
  std::optional<std::pair<hci::ConnectionHandle, BrEdrConnection*>> FindConnectionByAddress(
      const DeviceAddressBytes& bd_addr);

  // Find a peer with |addr| or create one if not found.
  Peer* FindOrInitPeer(DeviceAddress addr);

  // Initialize ACL connection state from |connection_handle| obtained from the controller and begin
  // interrogation.
  void InitializeConnection(DeviceAddress addr, hci::ConnectionHandle connection_handle);

  // Called once interrogation completes to make connection identified by |handle| available to
  // upper layers and begin new connection procedures.
  void CompleteConnectionSetup(Peer* peer, hci::ConnectionHandle handle);

  // Callbacks for registered events
  hci::CommandChannel::EventCallbackResult OnAuthenticationComplete(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnConnectionRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnConnectionComplete(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnIoCapabilityRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnIoCapabilityResponse(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnLinkKeyRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnLinkKeyNotification(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnSimplePairingComplete(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserConfirmationRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserPasskeyRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserPasskeyNotification(const hci::EventPacket& event);

  // Called when we complete a pending request. Initiates a new connection
  // attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Called when a request times out waiting for a connection complete packet,
  // *after* the command status was received. This is responsible for canceling
  // the request and initiating the next one in the queue
  void OnRequestTimeout();

  // Clean up |conn| after it has been deliberately disconnected or after its
  // link closed. Unregisters the connection from the data domain and marks the
  // peer's BR/EDR cache state as disconnected. Takes ownership of |conn| and
  // destroys it.
  void CleanUpConnection(hci::ConnectionHandle handle, BrEdrConnection conn);

  // Helpers for sending commands on the command channel for this controller.
  // All callbacks will run on |dispatcher_|.
  void SendIoCapabilityRequestReply(DeviceAddressBytes bd_addr, hci::IOCapability io_capability,
                                    uint8_t oob_data_present,
                                    hci::AuthRequirements auth_requirements,
                                    hci::StatusCallback cb = nullptr);
  void SendIoCapabilityRequestNegativeReply(DeviceAddressBytes bd_addr, hci::StatusCode reason,
                                            hci::StatusCallback cb = nullptr);
  void SendUserConfirmationRequestReply(DeviceAddressBytes bd_addr,
                                        hci::StatusCallback cb = nullptr);
  void SendUserConfirmationRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                hci::StatusCallback cb = nullptr);
  void SendUserPasskeyRequestReply(DeviceAddressBytes bd_addr, uint32_t numeric_value,
                                   hci::StatusCallback cb = nullptr);
  void SendUserPasskeyRequestNegativeReply(DeviceAddressBytes bd_addr,
                                           hci::StatusCallback cb = nullptr);

  // Send the HCI command encoded in |command_packet|. If |cb| is not nullptr, the event returned
  // will be decoded for its status, which is passed to |cb|.
  void SendCommandWithStatusCallback(std::unique_ptr<hci::CommandPacket> command_packet,
                                     hci::StatusCallback cb);

  using ConnectionMap = std::unordered_map<hci::ConnectionHandle, BrEdrConnection>;

  fxl::RefPtr<hci::Transport> hci_;
  std::unique_ptr<hci::SequentialCommandRunner> hci_cmd_runner_;

  // The pairing delegate used for authentication challenges. If nullptr, all
  // pairing requests will be rejected.
  fxl::WeakPtr<PairingDelegate> pairing_delegate_;

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

  // Handler IDs for registered events
  std::vector<hci::CommandChannel::EventHandlerId> event_handler_ids_;

  // The current page scan parameters of the controller.
  // Set to 0 when non-connectable.
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;
  hci::PageScanType page_scan_type_;
  bool use_interlaced_scan_;

  // Outstanding connection requests based on remote peer ID.
  std::unordered_map<PeerId, ConnectionRequest<BrEdrConnection*>> connection_requests_;

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
