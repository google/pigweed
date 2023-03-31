// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_

#include <functional>
#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/bounded_inspect_list_node.h"
#include "src/connectivity/bluetooth/core/bt-host/common/expiring_set.h"
#include "src/connectivity/bluetooth/core/bt-host/common/metrics.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/bredr_connection_request.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/service_discoverer.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt {

namespace hci {
class SequentialCommandRunner;
}  // namespace hci

namespace gap {

class PairingDelegate;
class PeerCache;

enum class DisconnectReason : uint8_t {
  // A FIDL method explicitly requested this disconnect
  kApiRequest,
  // The interrogation procedure for this peer failed
  kInterrogationFailed,
  // The connection encountered an error during Pairing
  kPairingFailed,
  // An error was encountered on the ACL link
  kAclLinkError,
  // Peer Disconnected
  kPeerDisconnection,
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
// called by the underlying hci::Connection object and the connection is cleaned up and removed
// from the internal |connections_| map.
class BrEdrConnectionManager final {
 public:
  BrEdrConnectionManager(hci::Transport::WeakPtr hci, PeerCache* peer_cache,
                         DeviceAddress local_address, l2cap::ChannelManager* l2cap,
                         bool use_interlaced_scan);
  ~BrEdrConnectionManager();

  // Set whether this host is connectable
  void SetConnectable(bool connectable, hci::ResultFunction<> status_cb);

  // Returns the PairingDelegate currently assigned to this connection manager.
  const PairingDelegate::WeakPtr& pairing_delegate() const { return pairing_delegate_; }

  // Assigns a new PairingDelegate to handle BR/EDR authentication challenges.
  // Replacing an existing pairing delegate cancels all ongoing pairing
  // procedures. If a delegate is not set then all pairing requests will be
  // rejected.
  void SetPairingDelegate(PairingDelegate::WeakPtr delegate);

  // Retrieves the peer id that is connected to the connection |handle|.
  // Returns kInvalidPeerId if no such peer exists.
  PeerId GetPeerId(hci_spec::ConnectionHandle handle) const;

  // Opens a new L2CAP channel to service |psm| on |peer_id| using the preferred parameters
  // |params|. If the current connection doesn't meet |security_requirements|, attempt to upgrade
  // the link key and report an error via |cb| if the upgrade fails.
  //
  // |cb| will be called with the channel created to the peer, or nullptr if the channel creation
  // resulted in an error.
  void OpenL2capChannel(PeerId peer_id, l2cap::PSM psm,
                        BrEdrSecurityRequirements security_requirements,
                        l2cap::ChannelParameters params, l2cap::ChannelCallback cb);

  // See ScoConnectionManager for documentation. If a BR/EDR connection with
  // the peer does not exist, returns nullopt.
  using ScoRequestHandle = BrEdrConnection::ScoRequestHandle;
  std::optional<ScoRequestHandle> OpenScoConnection(
      PeerId peer_id,
      bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter> parameters,
      sco::ScoConnectionManager::OpenConnectionCallback callback);
  std::optional<ScoRequestHandle> AcceptScoConnection(
      PeerId peer_id,
      std::vector<bt::StaticPacket<pw::bluetooth::emboss::SynchronousConnectionParametersWriter>>
          parameters,
      sco::ScoConnectionManager::AcceptConnectionCallback callback);

  // Add a service search to be performed on new connected remote peers.
  // This search will happen on every peer connection.
  // |callback| will be called with the |attributes| that exist in the service entry on the peer's
  // SDP server. If |attributes| is empty, all attributes on the server will be returned. Returns a
  // SearchId which can be used to remove the search later. Identical searches will perform the same
  // search for each search added. Results of added service searches will be added to each Peer's
  // BrEdrData.
  // TODO(fxbug.dev/1378): Make identical searches just search once
  using SearchCallback = sdp::ServiceDiscoverer::ResultCallback;
  using SearchId = sdp::ServiceDiscoverer::SearchId;
  SearchId AddServiceSearch(const UUID& uuid, std::unordered_set<sdp::AttributeId> attributes,
                            SearchCallback callback);

  // Remove a search previously added with AddServiceSearch()
  // Returns true if a search was removed.
  // This function is idempotent.
  bool RemoveServiceSearch(SearchId id);

  using ConnectResultCallback = fit::function<void(hci::Result<>, BrEdrConnection*)>;

  // Initiates an outgoing Create Connection Request to attempt to connect to
  // the peer identified by |peer_id|. Returns false if the connection
  // request was invalid, otherwise returns true and |callback| will be called
  // with the result of the procedure, whether successful or not
  // TODO(fxbug.dev/1413) - implement a timeout
  [[nodiscard]] bool Connect(PeerId peer_id, ConnectResultCallback callback);

  // Initiate pairing to the peer with |peer_id| using the bondable preference. Pairing will only be
  // initiated if the current link key does not meet the |security| requirements. |callback| will be
  // called with the result of the procedure, successful or not.
  void Pair(PeerId peer_id, BrEdrSecurityRequirements security, hci::ResultFunction<> callback);

  // Disconnects any existing BR/EDR connection to |peer_id|. Returns true if
  // the peer is disconnected, false if the peer can not be disconnected.
  bool Disconnect(PeerId peer_id, DisconnectReason reason);

  // If `reason` is DisconnectReason::kApiRequest, then incoming connections from `peer_id` are
  // rejected for kLocalDisconnectCooldownDuration
  static constexpr zx::duration kLocalDisconnectCooldownDuration = zx::sec(30);

  // Attach Inspect node as child of |parent| named |name|.
  // Only connections established after a call to AttachInspect are tracked.
  void AttachInspect(inspect::Node& parent, std::string name);

 private:
  // TODO(fxbug.dev/58020) - eventually replace these with auto-generated implementations
  // An event signifying that a connection was completed by the controller
  struct ConnectionComplete {
    DeviceAddress addr;
    hci_spec::ConnectionHandle handle;
    pw::bluetooth::emboss::StatusCode status_code;
    pw::bluetooth::emboss::LinkType link_type;

    // Create from an hci ConnectionComplete event
    // It is the duty of the caller to ensure it is called with a packet that contains a
    // ConnectionComplete event; otherwise it will assert
    explicit ConnectionComplete(const hci::EmbossEventPacket& event);

    hci::Result<> ToResult() { return bt::ToResult(status_code); }
  };

  // TODO(fxbug.dev/58020) - eventually replace these with auto-generated implementations
  // An event signifying that an incoming connection is being requested by a peer
  struct ConnectionRequestEvent {
    DeviceAddress addr;
    pw::bluetooth::emboss::LinkType link_type;
    DeviceClass class_of_device;

    // Create from an hci ConnectionRequest event
    // It is the duty of the caller to ensure it is called with a packet that contains a
    // ConnectionRequest event; otherwise it will assert
    explicit ConnectionRequestEvent(const hci::EmbossEventPacket& event);
  };

  // Callback for hci::Connection. Called when the peer disconnects.
  void OnPeerDisconnect(const hci::Connection* connection);

  // Called to cancel an outgoing connection request
  void SendCreateConnectionCancelCommand(DeviceAddress addr);

  // Attempt to complete the active connection request for the peer |peer_id| with status |status|
  void CompleteRequest(PeerId peer_id, DeviceAddress address, hci::Result<> status,
                       hci_spec::ConnectionHandle handle);

  // Is there a current incoming connection request in progress for the given address
  bool ExistsIncomingRequest(PeerId id);

  // Writes page timeout duration to the controller.
  // |page_timeout| must be in the range [PageTimeout::MIN (0x0001), PageTimeout::MAX (0xFFFF)]
  // |cb| will be called with the resulting return parameter status.
  void WritePageTimeout(zx::duration page_timeout, hci::ResultFunction<> cb);

  // Reads the controller page scan settings.
  void ReadPageScanSettings();

  // Writes page scan parameters to the controller.
  // If |interlaced| is true, and the controller does not support interlaced
  // page scan mode, standard mode is used.
  void WritePageScanSettings(uint16_t interval, uint16_t window, bool interlaced,
                             hci::ResultFunction<> cb);

  // Helper to register an event handler to run.
  hci::CommandChannel::EventHandlerId AddEventHandler(const hci_spec::EventCode& code,
                                                      hci::CommandChannel::EventCallbackVariant cb);

  // Find the handle for a connection to |peer_id|. Returns nullopt if no BR/EDR
  // |peer_id| is connected.
  std::optional<std::pair<hci_spec::ConnectionHandle, BrEdrConnection*>> FindConnectionById(
      PeerId peer_id);

  // Find the handle for a connection to |bd_addr|. Returns nullopt if no BR/EDR
  // |bd_addr| is connected.
  std::optional<std::pair<hci_spec::ConnectionHandle, BrEdrConnection*>> FindConnectionByAddress(
      const DeviceAddressBytes& bd_addr);

  // Find a peer with |addr| or create one if not found.
  Peer* FindOrInitPeer(DeviceAddress addr);

  // Initialize ACL connection state from |connection_handle| obtained from the controller and begin
  // interrogation. The connection will be initialized with the role |role|.
  void InitializeConnection(DeviceAddress addr, hci_spec::ConnectionHandle connection_handle,
                            pw::bluetooth::emboss::ConnectionRole role);

  // Called once interrogation completes to make connection identified by |handle| available to
  // upper layers and begin new connection procedures.
  void CompleteConnectionSetup(Peer::WeakPtr peer, hci_spec::ConnectionHandle handle);

  // Callbacks for registered events
  hci::CommandChannel::EventCallbackResult OnAuthenticationComplete(const hci::EventPacket& event);
  void OnConnectionRequest(ConnectionRequestEvent request);
  void OnConnectionComplete(ConnectionComplete event);
  hci::CommandChannel::EventCallbackResult OnIoCapabilityRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnIoCapabilityResponse(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnLinkKeyRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnLinkKeyNotification(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnSimplePairingComplete(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserConfirmationRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserPasskeyRequest(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnUserPasskeyNotification(const hci::EventPacket& event);
  hci::CommandChannel::EventCallbackResult OnRoleChange(const hci::EventPacket& event);

  // Called when we complete a pending request. Initiates a new connection
  // attempt for the next peer in the pending list, if any.
  void TryCreateNextConnection();

  // Collective parameters needed to begin a CreateConnection procedure
  struct CreateConnectionParams {
    PeerId peer_id;
    DeviceAddress addr;
    std::optional<uint16_t> clock_offset;
    std::optional<pw::bluetooth::emboss::PageScanRepetitionMode> page_scan_repetition_mode;
  };

  // Find the next valid Request that is available to begin connecting
  std::optional<CreateConnectionParams> NextCreateConnectionParams();

  // Begin a CreateConnection procedure for a given Request
  void InitiatePendingConnection(CreateConnectionParams request);

  // Called when a request times out waiting for a connection complete packet,
  // *after* the command status was received. This is responsible for canceling
  // the request and initiating the next one in the queue
  void OnRequestTimeout();

  // Clean up |conn| after it has been deliberately disconnected or after its
  // link closed. Unregisters the connection from the data domain and marks the
  // peer's BR/EDR cache state as disconnected. Takes ownership of |conn| and
  // destroys it.
  void CleanUpConnection(hci_spec::ConnectionHandle handle, BrEdrConnection conn,
                         DisconnectReason reason);

  // Helpers for sending commands on the command channel for this controller.
  // All callbacks will run on |dispatcher_|.
  void SendAuthenticationRequested(hci_spec::ConnectionHandle handle, hci::ResultFunction<> cb);
  void SendIoCapabilityRequestReply(
      DeviceAddressBytes bd_addr, pw::bluetooth::emboss::IoCapability io_capability,
      pw::bluetooth::emboss::OobDataPresent oob_data_present,
      pw::bluetooth::emboss::AuthenticationRequirements auth_requirements,
      hci::ResultFunction<> cb = nullptr);
  void SendIoCapabilityRequestNegativeReply(DeviceAddressBytes bd_addr,
                                            pw::bluetooth::emboss::StatusCode reason,
                                            hci::ResultFunction<> cb = nullptr);
  void SendUserConfirmationRequestReply(DeviceAddressBytes bd_addr,
                                        hci::ResultFunction<> cb = nullptr);
  void SendUserConfirmationRequestNegativeReply(DeviceAddressBytes bd_addr,
                                                hci::ResultFunction<> cb = nullptr);
  void SendUserPasskeyRequestReply(DeviceAddressBytes bd_addr, uint32_t numeric_value,
                                   hci::ResultFunction<> cb = nullptr);
  void SendUserPasskeyRequestNegativeReply(DeviceAddressBytes bd_addr,
                                           hci::ResultFunction<> cb = nullptr);
  void SendLinkKeyRequestNegativeReply(DeviceAddressBytes bd_addr,
                                       hci::ResultFunction<> cb = nullptr);
  void SendLinkKeyRequestReply(DeviceAddressBytes bd_addr, hci_spec::LinkKey link_key,
                               hci::ResultFunction<> cb = nullptr);
  void SendAcceptConnectionRequest(DeviceAddressBytes addr, hci::ResultFunction<> cb = nullptr);
  void SendRejectConnectionRequest(DeviceAddress addr, pw::bluetooth::emboss::StatusCode reason,
                                   hci::ResultFunction<> cb = nullptr);
  void SendRejectSynchronousRequest(DeviceAddress addr, pw::bluetooth::emboss::StatusCode reason,
                                    hci::ResultFunction<> cb = nullptr);

  // Send the HCI command encoded in |command_packet|. If |cb| is not nullptr, the event returned
  // will be decoded for its status, which is passed to |cb|.
  template <typename T>
  void SendCommandWithStatusCallback(T command_packet, hci::ResultFunction<> cb);

  // Record a disconnection in Inspect's list of disconnections.
  void RecordDisconnectInspect(const BrEdrConnection& conn, DisconnectReason reason);

  using ConnectionMap = std::unordered_map<hci_spec::ConnectionHandle, BrEdrConnection>;

  hci::Transport::WeakPtr hci_;
  std::unique_ptr<hci::SequentialCommandRunner> hci_cmd_runner_;

  // The pairing delegate used for authentication challenges. If nullptr, all
  // pairing requests will be rejected.
  PairingDelegate::WeakPtr pairing_delegate_;

  // Peer cache is used to look up parameters for connecting to peers and
  // update the state of connected peers as well as introduce unknown peers.
  // This object must outlive this instance.
  PeerCache* cache_;

  const DeviceAddress local_address_;

  l2cap::ChannelManager* l2cap_;

  // Discoverer for SDP services
  sdp::ServiceDiscoverer discoverer_;

  // Holds the connections that are active.
  ConnectionMap connections_;

  // Holds a denylist with cooldowns for locally requested disconnects.
  ExpiringSet<DeviceAddress> deny_incoming_;

  // Handler IDs for registered events
  std::vector<hci::CommandChannel::EventHandlerId> event_handler_ids_;

  // The current page scan parameters of the controller.
  // Set to 0 when non-connectable.
  uint16_t page_scan_interval_;
  uint16_t page_scan_window_;
  pw::bluetooth::emboss::PageScanType page_scan_type_;
  bool use_interlaced_scan_;

  // Outstanding connection requests based on remote peer ID.
  std::unordered_map<PeerId, BrEdrConnectionRequest> connection_requests_;

  std::optional<hci::BrEdrConnectionRequest> pending_request_;

  // Time after which a connection attempt is considered to have timed out.
  zx::duration request_timeout_;

  struct InspectProperties {
    BoundedInspectListNode last_disconnected_list = BoundedInspectListNode(/*capacity=*/5);
    inspect::Node connections_node_;
    inspect::Node requests_node_;
    struct DirectionalCounts {
      inspect::Node node_;
      UintMetricCounter connection_attempts_;
      UintMetricCounter successful_connections_;
      UintMetricCounter failed_connections_;
    };

    // These stats only represent HCI connections.
    // Connections may be closed if we fail to initialize, interrogate, or pair
    DirectionalCounts outgoing_;
    DirectionalCounts incoming_;

    // Successfully initialized connections
    UintMetricCounter interrogation_complete_count_;

    UintMetricCounter disconnect_local_api_request_count_;
    UintMetricCounter disconnect_interrogation_failed_count_;
    UintMetricCounter disconnect_pairing_failed_count_;
    UintMetricCounter disconnect_acl_link_error_count_;
    UintMetricCounter disconnect_peer_disconnection_count_;
  };
  InspectProperties inspect_properties_;
  inspect::Node inspect_node_;

  // The dispatcher that all commands are queued on.
  async_dispatcher_t* dispatcher_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  WeakSelf<BrEdrConnectionManager> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnectionManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_CONNECTION_MANAGER_H_
