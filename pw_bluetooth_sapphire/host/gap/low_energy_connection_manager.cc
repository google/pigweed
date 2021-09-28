// Copyright 2017 The Fuchsia Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_connection_manager.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <lib/async/time.h>
#include <zircon/assert.h>
#include <zircon/syscalls.h>

#include <optional>
#include <vector>

#include "low_energy_connection.h"
#include "pairing_delegate.h"
#include "peer.h"
#include "peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/generic_access_client.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/lib/fxl/strings/string_printf.h"

using bt::sm::BondableMode;

namespace bt::gap {

namespace {

// If an auto-connect attempt fails with any of the following error codes, we will stop auto-
// connecting to the peer until the next successful connection. We have only observed this issue
// with the 0x3e "kConnectionFailedToBeEstablished" error in the field, but have included these
// other errors based on their descriptions in v5.2 Vol. 1 Part F Section 2.
bool ShouldStopAlwaysAutoConnecting(hci_spec::StatusCode err) {
  switch (err) {
    case hci_spec::StatusCode::kConnectionTimeout:
    case hci_spec::StatusCode::kConnectionRejectedSecurity:
    case hci_spec::StatusCode::kConnectionAcceptTimeoutExceeded:
    case hci_spec::StatusCode::kConnectionTerminatedByLocalHost:
    case hci_spec::StatusCode::kConnectionFailedToBeEstablished:
      return true;
    default:
      return false;
  }
}

// During the initial connection to a peripheral we use the initial high
// duty-cycle parameters to ensure that initiating procedures (bonding,
// encryption setup, service discovery) are completed quickly. Once these
// procedures are complete, we will change the connection interval to the
// peripheral's preferred connection parameters (see v5.0, Vol 3, Part C,
// Section 9.3.12).
static const hci_spec::LEPreferredConnectionParameters kInitialConnectionParameters(
    kLEInitialConnIntervalMin, kLEInitialConnIntervalMax, /*max_latency=*/0,
    hci_spec::defaults::kLESupervisionTimeout);

const char* kInspectRequestsNodeName = "pending_requests";
const char* kInspectRequestNodeNamePrefix = "pending_request_";
const char* kInspectConnectionsNodeName = "connections";
const char* kInspectConnectionNodePrefix = "connection_";
const char* kInspectOutboundConnectorNodeName = "outbound_connector";
const char* kInspectConnectionFailuresPropertyName = "recent_connection_failures";

}  // namespace

LowEnergyConnectionManager::LowEnergyConnectionManager(
    fxl::WeakPtr<hci::Transport> hci, hci::LocalAddressDelegate* addr_delegate,
    hci::LowEnergyConnector* connector, PeerCache* peer_cache, fbl::RefPtr<l2cap::L2cap> l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager,
    sm::SecurityManagerFactory sm_creator)
    : hci_(std::move(hci)),
      security_mode_(LeSecurityMode::Mode1),
      sm_factory_func_(std::move(sm_creator)),
      request_timeout_(kLECreateConnectionTimeout),
      dispatcher_(async_get_default_dispatcher()),
      peer_cache_(peer_cache),
      l2cap_(l2cap),
      gatt_(gatt),
      discovery_manager_(discovery_manager),
      hci_connector_(connector),
      local_address_delegate_(addr_delegate),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(peer_cache_);
  ZX_DEBUG_ASSERT(l2cap_);
  ZX_DEBUG_ASSERT(gatt_);
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(hci_connector_);
  ZX_DEBUG_ASSERT(local_address_delegate_);
}

LowEnergyConnectionManager::~LowEnergyConnectionManager() {
  bt_log(INFO, "gap-le", "LowEnergyConnectionManager shutting down");

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Clear |pending_requests_| and notify failure.
  for (auto& iter : pending_requests_) {
    iter.second.NotifyCallbacks(fpromise::error(HostError::kFailed));
  }
  pending_requests_.clear();

  current_request_.reset();

  remote_connectors_.clear();

  // Clean up all connections.
  for (auto& iter : connections_) {
    CleanUpConnection(std::move(iter.second));
  }

  connections_.clear();
}

void LowEnergyConnectionManager::Connect(PeerId peer_id, ConnectionResultCallback callback,
                                         LowEnergyConnectionOptions connection_options) {
  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(WARN, "gap-le", "peer not found (id: %s)", bt_str(peer_id));
    callback(fpromise::error(HostError::kNotFound));
    return;
  }

  if (peer->technology() == TechnologyType::kClassic) {
    bt_log(ERROR, "gap-le", "peer does not support LE: %s", peer->ToString().c_str());
    callback(fpromise::error(HostError::kNotFound));
    return;
  }

  if (!peer->connectable()) {
    bt_log(ERROR, "gap-le", "peer not connectable: %s", peer->ToString().c_str());
    callback(fpromise::error(HostError::kNotFound));
    return;
  }

  // If we are already waiting to connect to |peer_id| then we store
  // |callback| to be processed after the connection attempt completes (in
  // either success of failure).
  auto pending_iter = pending_requests_.find(peer_id);
  if (pending_iter != pending_requests_.end()) {
    if (!current_request_) {
      bt_log(WARN, "gap-le",
             "Connect called for peer with pending request while no current_request_ exists (peer: "
             "%s)",
             bt_str(peer_id));
    }
    // TODO(fxbug.dev/65592): Merge connection_options with the options of the pending request.
    pending_iter->second.AddCallback(std::move(callback));
    // TODO(fxbug.dev/69621): Try to create this connection.
    return;
  }

  // Add callback to connecting request if |peer_id| matches.
  if (current_request_ && current_request_->request.peer_id() == peer_id) {
    // TODO(fxbug.dev/65592): Merge connection_options with the options of the current request.
    current_request_->request.AddCallback(std::move(callback));
    return;
  }

  auto conn_iter = connections_.find(peer_id);
  if (conn_iter != connections_.end()) {
    // TODO(fxbug.dev/65592): Handle connection_options that conflict with the existing connection.
    callback(fpromise::ok(conn_iter->second->AddRef()));
    return;
  }

  internal::LowEnergyConnectionRequest request(peer_id, std::move(callback), connection_options,
                                               peer->MutLe().RegisterInitializingConnection());
  request.AttachInspect(inspect_pending_requests_node_,
                        inspect_pending_requests_node_.UniqueName(kInspectRequestNodeNamePrefix));
  pending_requests_.emplace(peer_id, std::move(request));

  TryCreateNextConnection();
}

bool LowEnergyConnectionManager::Disconnect(PeerId peer_id) {
  auto remote_connector_iter = remote_connectors_.find(peer_id);
  if (remote_connector_iter != remote_connectors_.end()) {
    // Result callback will clean up connector.
    remote_connector_iter->second.connector->Cancel();
  }

  auto request_iter = pending_requests_.find(peer_id);
  if (request_iter != pending_requests_.end()) {
    ZX_ASSERT(current_request_->request.peer_id() != peer_id);
    request_iter->second.NotifyCallbacks(fpromise::error(HostError::kCanceled));
    pending_requests_.erase(request_iter);
  }

  if (current_request_ && current_request_->request.peer_id() == peer_id) {
    // Connector will call result callback to clean up connection.
    current_request_->connector->Cancel();
  }

  // Ignore Disconnect for peer that is not pending or connected:
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(INFO, "gap-le", "Disconnect called for unconnected peer (peer: %s)", bt_str(peer_id));
    return true;
  }

  // Handle peer that is already connected:

  // Remove the connection state from the internal map right away.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  // Since this was an intentional disconnect, update the auto-connection behavior
  // appropriately.
  peer_cache_->SetAutoConnectBehaviorForIntentionalDisconnect(peer_id);

  bt_log(INFO, "gap-le", "disconnecting (peer: %s, link: %s)", bt_str(conn->peer_id()),
         bt_str(*conn->link()));
  CleanUpConnection(std::move(conn));
  return true;
}

void LowEnergyConnectionManager::Pair(PeerId peer_id, sm::SecurityLevel pairing_level,
                                      sm::BondableMode bondable_mode, sm::StatusCallback cb) {
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "cannot pair: peer not connected (peer: %s)", bt_str(peer_id));
    cb(bt::sm::Status(bt::HostError::kNotFound));
    return;
  }
  bt_log(INFO, "gap-le", "pairing with security level: %d (peer: %s)", pairing_level,
         bt_str(peer_id));
  iter->second->UpgradeSecurity(pairing_level, bondable_mode, std::move(cb));
}

void LowEnergyConnectionManager::SetSecurityMode(LeSecurityMode mode) {
  security_mode_ = mode;
  if (mode == LeSecurityMode::SecureConnectionsOnly) {
    // `Disconnect`ing the peer must not be done while iterating through `connections_` as it
    // removes the connection from `connections_`, hence the helper vector.
    std::vector<PeerId> insufficiently_secure_peers;
    for (auto& [peer_id, connection] : connections_) {
      if (connection->security().level() != sm::SecurityLevel::kSecureAuthenticated &&
          connection->security().level() != sm::SecurityLevel::kNoSecurity) {
        insufficiently_secure_peers.push_back(peer_id);
      }
    }
    for (PeerId id : insufficiently_secure_peers) {
      Disconnect(id);
    }
  }
  for (auto& iter : connections_) {
    iter.second->set_security_mode(mode);
  }
}

void LowEnergyConnectionManager::AttachInspect(inspect::Node& parent) {
  inspect_node_ = parent.CreateChild(kInspectNodeName);
  inspect_properties_.recent_connection_failures.AttachInspect(
      inspect_node_, kInspectConnectionFailuresPropertyName);
  inspect_pending_requests_node_ = inspect_node_.CreateChild(kInspectRequestsNodeName);
  inspect_connections_node_ = inspect_node_.CreateChild(kInspectConnectionsNodeName);
  for (auto& request : pending_requests_) {
    request.second.AttachInspect(
        inspect_pending_requests_node_,
        inspect_pending_requests_node_.UniqueName(kInspectRequestNodeNamePrefix));
  }
  for (auto& conn : connections_) {
    conn.second->AttachInspect(inspect_connections_node_,
                               inspect_connections_node_.UniqueName(kInspectConnectionNodePrefix));
  }
  if (current_request_) {
    current_request_->connector->AttachInspect(inspect_node_, kInspectOutboundConnectorNodeName);
  }
}

void LowEnergyConnectionManager::RegisterRemoteInitiatedLink(hci::ConnectionPtr link,
                                                             sm::BondableMode bondable_mode,
                                                             ConnectionResultCallback callback) {
  ZX_ASSERT(link);

  Peer* peer = UpdatePeerWithLink(*link);
  auto peer_id = peer->identifier();

  bt_log(INFO, "gap-le", "new remote-initiated link (peer: %s, local addr: %s, link: %s)",
         bt_str(peer_id), bt_str(link->local_address()), bt_str(*link));

  // TODO(fxbug.dev/653): Use own address when storing the connection.
  // Currently this will refuse the connection and disconnect the link if |peer|
  // is already connected to us by a different local address.
  if (connections_.find(peer_id) != connections_.end()) {
    bt_log(INFO, "gap-le",
           "multiple links from peer; remote-initiated connection refused (peer: %s)",
           bt_str(peer_id));
    callback(fpromise::error(HostError::kFailed));
    return;
  }

  if (remote_connectors_.find(peer_id) != remote_connectors_.end()) {
    bt_log(INFO, "gap-le",
           "remote connector for peer already exists; connection refused (peer: %s)",
           bt_str(peer_id));
    callback(fpromise::error(HostError::kFailed));
    return;
  }

  LowEnergyConnectionOptions connection_options{.bondable_mode = bondable_mode};
  internal::LowEnergyConnectionRequest request(peer_id, std::move(callback), connection_options,
                                               peer->MutLe().RegisterInitializingConnection());

  auto result_cb = std::bind(&LowEnergyConnectionManager::OnRemoteInitiatedConnectResult, this,
                             peer_id, std::placeholders::_1);
  auto connector = internal::LowEnergyConnector::CreateInboundConnector(
      peer_id, std::move(link), connection_options, hci_, peer_cache_,
      weak_ptr_factory_.GetWeakPtr(), l2cap_, gatt_, result_cb);
  remote_connectors_.emplace(peer_id,
                             RequestAndConnector{std::move(request), std::move(connector)});
}

void LowEnergyConnectionManager::SetPairingDelegate(fxl::WeakPtr<PairingDelegate> delegate) {
  // TODO(armansito): Add a test case for this once fxbug.dev/886 is done.
  pairing_delegate_ = delegate;

  // Tell existing connections to abort ongoing pairing procedures. The new
  // delegate will receive calls to PairingDelegate::CompletePairing, unless it
  // is null.
  for (auto& iter : connections_) {
    iter.second->ResetSecurityManager(delegate ? delegate->io_capability()
                                               : sm::IOCapability::kNoInputNoOutput);
  }
}

void LowEnergyConnectionManager::SetDisconnectCallbackForTesting(DisconnectCallback callback) {
  test_disconn_cb_ = std::move(callback);
}

void LowEnergyConnectionManager::ReleaseReference(LowEnergyConnectionHandle* conn_ref) {
  ZX_DEBUG_ASSERT(conn_ref);

  auto iter = connections_.find(conn_ref->peer_identifier());
  ZX_DEBUG_ASSERT(iter != connections_.end());

  iter->second->DropRef(conn_ref);
  if (iter->second->ref_count() != 0u)
    return;

  // Move the connection object before erasing the entry.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  bt_log(INFO, "gap-le", "all refs dropped on connection (link: %s, peer: %s)",
         bt_str(*conn->link()), bt_str(conn->peer_id()));
  CleanUpConnection(std::move(conn));
}

void LowEnergyConnectionManager::TryCreateNextConnection() {
  if (current_request_.has_value()) {
    bt_log(DEBUG, "gap-le", "%s: request already in progress", __FUNCTION__);
    return;
  }

  if (pending_requests_.empty()) {
    bt_log(TRACE, "gap-le", "%s: no pending requests remaining", __FUNCTION__);
    return;
  }

  for (auto& iter : pending_requests_) {
    auto peer_id = iter.first;
    Peer* peer = peer_cache_->FindById(peer_id);
    if (peer) {
      auto request_pair = pending_requests_.extract(peer_id);
      internal::LowEnergyConnectionRequest request = std::move(request_pair.mapped());

      auto result_cb =
          fit::bind_member(this, &LowEnergyConnectionManager::OnLocalInitiatedConnectResult);
      std::unique_ptr<internal::LowEnergyConnector> connector =
          internal::LowEnergyConnector::CreateOutboundConnector(
              peer_id, request.connection_options(), hci_connector_, request_timeout_, hci_,
              peer_cache_, discovery_manager_, weak_ptr_factory_.GetWeakPtr(), l2cap_, gatt_,
              std::move(result_cb));
      connector->AttachInspect(inspect_node_, kInspectOutboundConnectorNodeName);

      current_request_ = RequestAndConnector{std::move(request), std::move(connector)};
      return;
    }

    bt_log(WARN, "gap-le", "deferring connection attempt (peer: %s)", bt_str(peer_id));

    // TODO(fxbug.dev/908): For now the requests for this peer won't complete
    // until the next peer discovery. This will no longer be an issue when we
    // use background scanning.
  }
}

void LowEnergyConnectionManager::OnLocalInitiatedConnectResult(
    fpromise::result<std::unique_ptr<internal::LowEnergyConnection>, hci::Status> result) {
  ZX_ASSERT(current_request_.has_value());

  internal::LowEnergyConnectionRequest request = std::move(current_request_->request);
  current_request_.reset();

  if (result.is_error()) {
    bt_log(INFO, "gap-le", "failed to connect to peer (peer: %s, status: %s)",
           bt_str(request.peer_id()), bt_str(result.error()));
  } else {
    bt_log(INFO, "gap-le", "connection request successful (peer: %s)", bt_str(request.peer_id()));
  }

  ProcessConnectResult(std::move(result), std::move(request));
  TryCreateNextConnection();
}

void LowEnergyConnectionManager::OnRemoteInitiatedConnectResult(
    PeerId peer_id,
    fpromise::result<std::unique_ptr<internal::LowEnergyConnection>, hci::Status> result) {
  auto remote_connector_node = remote_connectors_.extract(peer_id);
  ZX_ASSERT(!remote_connector_node.empty());

  internal::LowEnergyConnectionRequest request = std::move(remote_connector_node.mapped().request);

  if (result.is_error()) {
    bt_log(INFO, "gap-le",
           "failed to complete remote initated connection with peer (peer: %s, status: %s)",
           bt_str(peer_id), bt_str(result.error()));
  } else {
    bt_log(INFO, "gap-le", "remote initiated connection successful (peer: %s)", bt_str(peer_id));
  }

  ProcessConnectResult(std::move(result), std::move(request));
}

void LowEnergyConnectionManager::ProcessConnectResult(
    fpromise::result<std::unique_ptr<internal::LowEnergyConnection>, hci::Status> result,
    internal::LowEnergyConnectionRequest request) {
  PeerId peer_id = request.peer_id();
  if (result.is_error()) {
    hci::Status err = result.error();
    Peer* peer = peer_cache_->FindById(peer_id);
    // Peer may have been forgotten (causing this error).
    // A separate connection may have been established in the other direction while this connection
    // was connecting, in which case the peer state should not be updated.
    if (peer && connections_.find(peer->identifier()) == connections_.end()) {
      if (request.connection_options().auto_connect && err.is_protocol_error() &&
          ShouldStopAlwaysAutoConnecting(err.protocol_error())) {
        // We may see a peer's connectable advertisements, but fail to establish a connection to the
        // peer (e.g. due to asymmetrical radio TX power). Unsetting the AutoConnect flag here
        // prevents a loop of "see peer device, attempt auto-connect, fail to establish connection".
        peer->MutLe().set_auto_connect_behavior(
            Peer::AutoConnectBehavior::kSkipUntilNextConnection);
      }
    }

    HostError host_error = err.is_protocol_error() ? HostError::kFailed : err.error();
    request.NotifyCallbacks(fpromise::error(host_error));

    inspect_properties_.recent_connection_failures.Add(1);

    return;
  }

  auto connection = result.take_value();
  InitializeConnection(std::move(connection), std::move(request));
}

bool LowEnergyConnectionManager::InitializeConnection(
    std::unique_ptr<internal::LowEnergyConnection> connection,
    internal::LowEnergyConnectionRequest request) {
  ZX_ASSERT(connection);

  auto peer_id = connection->peer_id();

  // TODO(fxbug.dev/653): For now reject having more than one link with the same
  // peer. This should change once this has more context on the local
  // destination for remote initiated connections.
  if (connections_.find(peer_id) != connections_.end()) {
    bt_log(INFO, "gap-le",
           "cannot initialize multiple links to same peer; connection refused (peer: %s)",
           bt_str(peer_id));
    // Notify request that duplicate connection could not be initialized.
    request.NotifyCallbacks(fpromise::error(HostError::kFailed));
    // Do not update peer state, as there is another active LE connection in connections_ for this
    // peer.
    return false;
  }

  Peer* peer = peer_cache_->FindById(peer_id);
  ZX_ASSERT(peer);

  connection->AttachInspect(inspect_connections_node_,
                            inspect_connections_node_.UniqueName(kInspectConnectionNodePrefix));
  connection->set_peer_disconnect_callback(std::bind(&LowEnergyConnectionManager::OnPeerDisconnect,
                                                     this, connection->link(),
                                                     std::placeholders::_1));
  connection->set_error_callback([this, peer_id]() { Disconnect(peer_id); });

  auto [conn_iter, inserted] = connections_.try_emplace(peer_id, std::move(connection));
  ZX_ASSERT(inserted);

  conn_iter->second->set_peer_conn_token(peer->MutLe().RegisterConnection());

  // Create first ref to ensure that connection is cleaned up on early returns or if first request
  // callback does not retain a ref.
  auto first_ref = conn_iter->second->AddRef();

  UpdatePeerWithLink(*conn_iter->second->link());

  bt_log(TRACE, "gap-le", "notifying connection request callbacks (peer: %s)", bt_str(peer_id));

  request.NotifyCallbacks(
      fpromise::ok(std::bind(&internal::LowEnergyConnection::AddRef, conn_iter->second.get())));

  return true;
}

void LowEnergyConnectionManager::CleanUpConnection(
    std::unique_ptr<internal::LowEnergyConnection> conn) {
  ZX_ASSERT(conn);

  // Mark the peer peer as no longer connected.
  Peer* peer = peer_cache_->FindById(conn->peer_id());
  ZX_ASSERT_MSG(peer, "A connection was active for an unknown peer! (id: %s)",
                bt_str(conn->peer_id()));
  conn.reset();
}

Peer* LowEnergyConnectionManager::UpdatePeerWithLink(const hci::Connection& link) {
  Peer* peer = peer_cache_->FindByAddress(link.peer_address());
  if (!peer) {
    peer = peer_cache_->NewPeer(link.peer_address(), true /* connectable */);
  }
  peer->MutLe().SetConnectionParameters(link.low_energy_parameters());
  peer_cache_->SetAutoConnectBehaviorForSuccessfulConnection(peer->identifier());

  return peer;
}

void LowEnergyConnectionManager::OnPeerDisconnect(const hci::Connection* connection,
                                                  hci_spec::StatusCode reason) {
  auto handle = connection->handle();
  if (test_disconn_cb_) {
    test_disconn_cb_(handle);
  }

  // See if we can find a connection with a matching handle by walking the
  // connections list.
  auto iter = FindConnection(handle);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "disconnect from unknown connection handle: %#.4x", handle);
    return;
  }

  // Found the connection. Remove the entry from |connections_| before notifying
  // the "closed" handlers.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  bt_log(INFO, "gap-le", "peer disconnected (peer: %s, handle: %#.4x)", bt_str(conn->peer_id()),
         handle);

  CleanUpConnection(std::move(conn));
}

LowEnergyConnectionManager::ConnectionMap::iterator LowEnergyConnectionManager::FindConnection(
    hci_spec::ConnectionHandle handle) {
  auto iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    const auto& conn = *iter->second;
    if (conn.handle() == handle)
      break;
  }
  return iter;
}

}  // namespace bt::gap
