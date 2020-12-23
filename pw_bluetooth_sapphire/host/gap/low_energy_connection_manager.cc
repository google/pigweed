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
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/security_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/strings/string_printf.h"

using bt::sm::BondableMode;

namespace bt::gap {

namespace {

static const hci::LEPreferredConnectionParameters kDefaultPreferredConnectionParameters(
    hci::defaults::kLEConnectionIntervalMin, hci::defaults::kLEConnectionIntervalMax,
    /*max_latency=*/0, hci::defaults::kLESupervisionTimeout);

// Maximum number of times to retry connections that fail with a kConnectionFailedToBeEstablished
// error.
constexpr int kMaxConnectionAttempts = 3;

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
      connector_(connector),
      local_address_delegate_(addr_delegate),
      interrogator_(peer_cache, hci_, dispatcher_),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(peer_cache_);
  ZX_DEBUG_ASSERT(l2cap_);
  ZX_DEBUG_ASSERT(gatt_);
  ZX_DEBUG_ASSERT(hci_);
  ZX_DEBUG_ASSERT(connector_);
  ZX_DEBUG_ASSERT(local_address_delegate_);

  auto self = weak_ptr_factory_.GetWeakPtr();

  conn_update_cmpl_handler_id_ = hci_->command_channel()->AddLEMetaEventHandler(
      hci::kLEConnectionUpdateCompleteSubeventCode, [self](const auto& event) {
        if (self) {
          return self->OnLEConnectionUpdateComplete(event);
        }
        return hci::CommandChannel::EventCallbackResult::kRemove;
      });
}

LowEnergyConnectionManager::~LowEnergyConnectionManager() {
  hci_->command_channel()->RemoveEventHandler(conn_update_cmpl_handler_id_);

  bt_log(DEBUG, "gap-le", "connection manager shutting down");

  weak_ptr_factory_.InvalidateWeakPtrs();

  // This will cancel any pending request.
  if (connector_->request_pending()) {
    connector_->Cancel();
  }

  // Clear |pending_requests_| and notify failure.
  for (auto& iter : pending_requests_) {
    iter.second.NotifyCallbacks(fit::error(HostError::kFailed));
  }
  pending_requests_.clear();

  // Clean up all connections.
  for (auto& iter : connections_) {
    CleanUpConnection(std::move(iter.second));
  }

  connections_.clear();
}

void LowEnergyConnectionManager::Connect(PeerId peer_id, ConnectionResultCallback callback,
                                         LowEnergyConnectionOptions connection_options) {
  if (!connector_) {
    bt_log(WARN, "gap-le", "connect called during shutdown!");
    callback(fit::error(HostError::kFailed));
    return;
  }

  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(WARN, "gap-le", "peer not found (id: %s)", bt_str(peer_id));
    callback(fit::error(HostError::kNotFound));
    return;
  }

  if (peer->technology() == TechnologyType::kClassic) {
    bt_log(ERROR, "gap-le", "peer does not support LE: %s", peer->ToString().c_str());
    callback(fit::error(HostError::kNotFound));
    return;
  }

  if (!peer->connectable()) {
    bt_log(ERROR, "gap-le", "peer not connectable: %s", peer->ToString().c_str());
    callback(fit::error(HostError::kNotFound));
    return;
  }

  // If we are already waiting to connect to |peer_id| then we store
  // |callback| to be processed after the connection attempt completes (in
  // either success of failure).
  auto pending_iter = pending_requests_.find(peer_id);
  if (pending_iter != pending_requests_.end()) {
    ZX_ASSERT(connections_.find(peer_id) == connections_.end());
    ZX_ASSERT(connector_->request_pending() || scanning_);
    // TODO(fxbug.dev/65592): Merge connection_options with the options of the pending request.
    pending_iter->second.AddCallback(std::move(callback));
    return;
  }

  // If there is already an active connection then we add a callback to be called after
  // interrogation completes.
  auto conn_iter = connections_.find(peer_id);
  if (conn_iter != connections_.end()) {
    // TODO(fxbug.dev/65592): Handle connection_options that conflict with the existing connection.
    conn_iter->second->AddRequestCallback(std::move(callback));
    return;
  }

  peer->MutLe().SetConnectionState(Peer::ConnectionState::kInitializing);
  pending_requests_[peer_id] = internal::LowEnergyConnectionRequest(
      peer->address(), std::move(callback), connection_options);

  TryCreateNextConnection();
}

bool LowEnergyConnectionManager::Disconnect(PeerId peer_id) {
  // Handle a request that is still pending by canceling scanning/connecting:
  auto request_iter = pending_requests_.find(peer_id);
  if (request_iter != pending_requests_.end()) {
    CancelPendingRequest(peer_id);
    return true;
  }

  // Ignore Disconnect for peer that is not pending or connected:
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "Disconnect called for unconnected peer (peer: %s)", bt_str(peer_id));
    return true;
  }

  // Handle peer that is being interrogated or is already connected:

  // Remove the connection state from the internal map right away.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  // Since this was an intentional disconnect, update the auto-connection behavior
  // appropriately.
  peer_cache_->SetAutoConnectBehaviorForIntentionalDisconnect(peer_id);

  bt_log(INFO, "gap-le", "disconnecting link: %s", bt_str(*conn->link()));
  CleanUpConnection(std::move(conn));
  return true;
}

void LowEnergyConnectionManager::Pair(PeerId peer_id, sm::SecurityLevel pairing_level,
                                      sm::BondableMode bondable_mode, sm::StatusCallback cb) {
  auto iter = connections_.find(peer_id);
  if (iter == connections_.end()) {
    bt_log(WARN, "gap-le", "cannot pair: peer not connected (id: %s)", bt_str(peer_id));
    cb(bt::sm::Status(bt::HostError::kNotFound));
    return;
  }
  bt_log(DEBUG, "gap-le", "pairing with security level: %d", pairing_level);
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

void LowEnergyConnectionManager::RegisterRemoteInitiatedLink(hci::ConnectionPtr link,
                                                             sm::BondableMode bondable_mode,
                                                             ConnectionResultCallback callback) {
  ZX_DEBUG_ASSERT(link);
  bt_log(INFO, "gap-le", "new remote-initiated link (local addr: %s): %s",
         bt_str(link->local_address()), bt_str(*link));

  Peer* peer = UpdatePeerWithLink(*link);
  auto peer_id = peer->identifier();

  LowEnergyConnectionOptions connection_options{.bondable_mode = bondable_mode};
  internal::LowEnergyConnectionRequest request(peer->address(), std::move(callback),
                                               connection_options);

  // TODO(armansito): Use own address when storing the connection (fxbug.dev/653).
  // Currently this will refuse the connection and disconnect the link if |peer|
  // is already connected to us by a different local address.
  InitializeConnection(peer_id, std::move(link), std::move(request));
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

void LowEnergyConnectionManager::SetConnectionParametersCallbackForTesting(
    ConnectionParametersCallback callback) {
  test_conn_params_cb_ = std::move(callback);
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

  bt_log(INFO, "gap-le", "all refs dropped on connection: %s", conn->link()->ToString().c_str());
  CleanUpConnection(std::move(conn));
}

void LowEnergyConnectionManager::TryCreateNextConnection() {
  // There can only be one outstanding LE Create Connection request at a time.
  if (connector_->request_pending()) {
    bt_log(DEBUG, "gap-le", "%s: HCI_LE_Create_Connection command pending", __FUNCTION__);
    return;
  }

  if (scanning_) {
    bt_log(DEBUG, "gap-le", "%s: connection request scan pending", __FUNCTION__);
    return;
  }

  if (pending_requests_.empty()) {
    bt_log(TRACE, "gap-le", "%s: no pending requests remaining", __FUNCTION__);
    return;
  }

  for (auto& iter : pending_requests_) {
    const auto& next_peer_addr = iter.second.address();
    Peer* peer = peer_cache_->FindByAddress(next_peer_addr);
    if (peer) {
      iter.second.add_connection_attempt();

      if (iter.second.connection_attempts() != 1) {
        // Skip scanning if this is a connection retry, as a scan was performed before the initial
        // attempt.
        bt_log(INFO, "gap-le", "retrying connection (attempt: %d, peer: %s)",
               iter.second.connection_attempts(), bt_str(peer->identifier()));
        RequestCreateConnection(peer);
      } else if (iter.second.connection_options().auto_connect) {
        // If this connection is being established in response to a directed advertisement, there is
        // no need to scan again.
        bt_log(TRACE, "gap-le", "auto connecting (peer: %s)", bt_str(peer->identifier()));
        RequestCreateConnection(peer);
      } else {
        StartScanningForPeer(peer);
      }
      break;
    }

    bt_log(DEBUG, "gap-le", "deferring connection attempt for peer: %s",
           next_peer_addr.ToString().c_str());

    // TODO(fxbug.dev/908): For now the requests for this peer won't complete
    // until the next peer discovery. This will no longer be an issue when we
    // use background scanning.
  }
}

void LowEnergyConnectionManager::StartScanningForPeer(Peer* peer) {
  ZX_ASSERT(peer);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto peer_id = peer->identifier();

  scanning_ = true;

  discovery_manager_->StartDiscovery(/*active=*/false, [self, peer_id](auto session) {
    if (self) {
      self->OnScanStart(peer_id, std::move(session));
    }
  });
}

void LowEnergyConnectionManager::OnScanStart(PeerId peer_id, LowEnergyDiscoverySessionPtr session) {
  auto request_iter = pending_requests_.find(peer_id);
  if (request_iter == pending_requests_.end()) {
    // Request was canceled while scan was starting.
    return;
  }

  // Starting scanning failed, abort connection procedure.
  if (!session) {
    scanning_ = false;
    OnConnectResult(peer_id, hci::Status(HostError::kFailed), nullptr);
    return;
  }

  bt_log(DEBUG, "gap-le", "started scanning for pending connection (peer: %s)", bt_str(peer_id));

  auto self = weak_ptr_factory_.GetWeakPtr();
  scan_timeout_task_.emplace([self, peer_id] {
    bt_log(INFO, "gap-le", "scan for pending connection timed out (peer: %s)", bt_str(peer_id));
    self->OnConnectResult(peer_id, hci::Status(HostError::kTimedOut), nullptr);
  });
  // The scan timeout may include time during which scanning is paused.
  scan_timeout_task_->PostDelayed(self->dispatcher_, kLEGeneralCepScanTimeout);

  session->filter()->set_connectable(true);
  request_iter->second.set_discovery_session(std::move(session));

  // Set the result callback after adding the session to the request in case it is called
  // synchronously (e.g. when there is an ongoing active scan and the peer is cached).
  request_iter->second.discovery_session()->SetResultCallback([self, peer_id](auto& peer) {
    if (!self || peer.identifier() != peer_id) {
      return;
    }

    bt_log(DEBUG, "gap-le", "discovered peer for pending connection (peer: %s)",
           bt_str(peer.identifier()));

    self->scan_timeout_task_.reset();
    ZX_ASSERT(self->scanning_);
    self->scanning_ = false;

    // Stopping the discovery session will unregister this result handler.
    auto iter = self->pending_requests_.find(peer_id);
    ZX_ASSERT(iter != self->pending_requests_.end());
    ZX_ASSERT(iter->second.discovery_session());
    iter->second.discovery_session()->Stop();

    Peer* peer_ptr = self->peer_cache_->FindById(peer_id);
    ZX_ASSERT(peer_ptr);
    self->RequestCreateConnection(peer_ptr);
  });

  request_iter->second.discovery_session()->set_error_callback([self, peer_id] {
    ZX_ASSERT(self->scanning_);
    bt_log(INFO, "gap-le", "discovery error while scanning for peer (peer: %s)", bt_str(peer_id));
    self->scanning_ = false;
    self->OnConnectResult(peer_id, hci::Status(HostError::kFailed), nullptr);
  });
}

void LowEnergyConnectionManager::RequestCreateConnection(Peer* peer) {
  ZX_ASSERT(peer);

  // Pause discovery until connection complete.
  auto pause = discovery_manager_->PauseDiscovery();

  // During the initial connection to a peripheral we use the initial high
  // duty-cycle parameters to ensure that initiating procedures (bonding,
  // encryption setup, service discovery) are completed quickly. Once these
  // procedures are complete, we will change the connection interval to the
  // peripheral's preferred connection parameters (see v5.0, Vol 3, Part C,
  // Section 9.3.12).

  // TODO(armansito): Initiate the connection using the cached preferred
  // connection parameters if we are bonded.
  hci::LEPreferredConnectionParameters initial_params(kLEInitialConnIntervalMin,
                                                      kLEInitialConnIntervalMax, 0,
                                                      hci::defaults::kLESupervisionTimeout);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self, peer_id = peer->identifier(), pause = std::optional(std::move(pause))](
                       hci::Status status, auto link) mutable {
    if (self) {
      pause.reset();
      self->OnConnectResult(peer_id, status, std::move(link));
    }
  };

  // We set the scan window and interval to the same value for continuous scanning.
  connector_->CreateConnection(/*use_whitelist=*/false, peer->address(), kLEScanFastInterval,
                               kLEScanFastInterval, initial_params, std::move(status_cb),
                               self->request_timeout_);
}

bool LowEnergyConnectionManager::InitializeConnection(
    PeerId peer_id, std::unique_ptr<hci::Connection> link,
    internal::LowEnergyConnectionRequest request) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(link->ll_type() == hci::Connection::LinkType::kLE);

  auto handle = link->handle();
  auto role = link->role();

  // TODO(armansito): For now reject having more than one link with the same
  // peer. This should change once this has more context on the local
  // destination for remote initiated connections (see fxbug.dev/653).
  if (connections_.find(peer_id) != connections_.end()) {
    bt_log(DEBUG, "gap-le", "multiple links from peer; connection refused");
    // Notify request that duplicate connection could not be initialized.
    request.NotifyCallbacks(fit::error(HostError::kFailed));
    return false;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto conn_param_update_cb = [self, handle, peer_id](const auto& params) {
    if (self) {
      self->OnNewLEConnectionParams(peer_id, handle, params);
    }
  };

  auto link_error_cb = [self, peer_id] {
    bt_log(DEBUG, "gap", "link error received from L2CAP");
    if (self) {
      self->Disconnect(peer_id);
    }
  };

  // Initialize connection.
  auto conn_options = request.connection_options();
  auto conn = std::make_unique<internal::LowEnergyConnection>(
      peer_id, std::move(link), dispatcher_, self, l2cap_, gatt_, std::move(request));
  conn->InitializeFixedChannels(std::move(conn_param_update_cb), std::move(link_error_cb),
                                conn_options);
  conn->StartConnectionPausePeripheralTimeout();

  connections_[peer_id] = std::move(conn);

  // TODO(armansito): Should complete a few more things before returning the
  // connection:
  //    1. If this is the first time we connected to this peer:
  //      a. If master, obtain Peripheral Preferred Connection Parameters via
  //         GATT if available
  //      b. Initiate name discovery over GATT if complete name is unknown
  //      c. If master, allow slave to initiate procedures (service discovery,
  //         encryption setup, etc) for kLEConnectionPauseCentral before
  //         updating the connection parameters to the slave's preferred values.

  if (role == hci::Connection::Role::kMaster) {
    // After the Central device has no further pending actions to perform and the
    // Peripheral device has not initiated any other actions within
    // kLEConnectionPauseCentral, then the Central device should update the connection parameters
    // to either the Peripheral Preferred Connection Parameters or self-determined values (Core
    // Spec v5.2, Vol 3, Part C, Sec 9.3.12).
    //
    // TODO(fxbug.dev/60830): Use the preferred connection parameters from the GAP characteristic.
    // (Core Spec v5.2, Vol 3, Part C, Sec 12.3)
    connections_[peer_id]->PostCentralPauseTimeoutCallback([this, handle]() {
      UpdateConnectionParams(handle, kDefaultPreferredConnectionParameters);
    });
  }

  // TODO(fxbug.dev/66356): Start the interrogator owned by connections_[peer_id] instead of passing
  // a WeakPtr here.
  auto conn_weak = connections_[peer_id]->GetWeakPtr();
  interrogator_.Start(peer_id, handle, [peer_id, self, conn_weak](auto status) mutable {
    // If the connection was destroyed (!conn_weak), it was cancelled and the connection process
    // should be aborted.
    if (self && conn_weak) {
      self->OnInterrogationComplete(peer_id, status);
    }
  });

  return true;
}

void LowEnergyConnectionManager::OnInterrogationComplete(PeerId peer_id, hci::Status status) {
  auto iter = connections_.find(peer_id);
  ZX_ASSERT(iter != connections_.end());

  // If the controller responds to an interrogation command with the 0x3e
  // "kConnectionFailedToBeEstablished" error, it will send a Disconnection Complete event soon
  // after. Do not create a connection ref in order to ensure the connection stays alive until the
  // event is received. This is the simplest way of handling incoming connection requests during
  // this time window and waiting to initiate a connection retry when the event is received.
  if (status.is_protocol_error() &&
      status.protocol_error() == hci::kConnectionFailedToBeEstablished) {
    bt_log(INFO, "gap-le",
           "Received kConnectionFailedToBeEstablished during interrogation. Waiting for Disconnect "
           "Complete. (peer: %s)",
           bt_str(peer_id));
    return;
  }

  // Create first ref to ensure that connection is cleaned up in early returns or if first request
  // callback does not retain a ref.
  auto first_ref = iter->second->AddRef();

  if (!status.is_success()) {
    bt_log(INFO, "gap-le", "interrogation failed with %s, releasing ref (peer: %s)", bt_str(status),
           bt_str(peer_id));
    // Releasing first_ref will disconnect and notify request callbacks of failure.
    return;
  }

  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(INFO, "gap", "OnInterrogationComplete called for unknown peer");
    // Releasing first_ref will disconnect and notify request callbacks of failure.
    return;
  }

  auto it = connections_.find(peer_id);
  if (it == connections_.end()) {
    bt_log(INFO, "gap", "OnInterrogationComplete called for removed connection");
    // Releasing first_ref will disconnect and notify request callbacks of failure.
    return;
  }
  auto& conn = it->second;

  if (conn->link()->role() == hci::Connection::Role::kSlave) {
    // "The peripheral device should not perform a connection parameter update procedure within
    // kLEConnectionPausePeripheral after establishing a connection." (Core Spec v5.2, Vol 3, Part
    // C, Sec 9.3.12).
    conn->on_peripheral_pause_timeout([peer_id, this](auto conn) {
      RequestConnectionParameterUpdate(peer_id, *conn, kDefaultPreferredConnectionParameters);
    });
  }

  peer->MutLe().SetConnectionState(Peer::ConnectionState::kConnected);

  // Distribute refs to requesters.
  conn->NotifyRequestCallbacks();
}

void LowEnergyConnectionManager::CleanUpConnection(
    std::unique_ptr<internal::LowEnergyConnection> conn) {
  ZX_ASSERT(conn);

  // Mark the peer peer as no longer connected.
  Peer* peer = peer_cache_->FindById(conn->peer_id());
  ZX_ASSERT_MSG(peer, "A connection was active for an unknown peer! (id: %s)",
                bt_str(conn->peer_id()));
  peer->MutLe().SetConnectionState(Peer::ConnectionState::kNotConnected);

  conn.reset();
}

void LowEnergyConnectionManager::RegisterLocalInitiatedLink(std::unique_ptr<hci::Connection> link) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(link->ll_type() == hci::Connection::LinkType::kLE);
  bt_log(INFO, "gap-le", "new local-initiated link %s", bt_str(*link));

  Peer* peer = UpdatePeerWithLink(*link);
  auto peer_id = peer->identifier();

  auto request_iter = pending_requests_.find(peer_id);
  ZX_ASSERT(request_iter != pending_requests_.end());
  auto request = std::move(request_iter->second);
  pending_requests_.erase(request_iter);

  InitializeConnection(peer_id, std::move(link), std::move(request));
  // If interrogation completes synchronously and the client does not retain a connection ref from
  // its callback,  the connection may already have been removed from connections_.

  ZX_ASSERT(!connector_->request_pending());
  TryCreateNextConnection();
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

void LowEnergyConnectionManager::OnConnectResult(PeerId peer_id, hci::Status status,
                                                 hci::ConnectionPtr link) {
  ZX_ASSERT(connections_.find(peer_id) == connections_.end());

  if (status) {
    bt_log(TRACE, "gap-le", "connection request successful (peer: %s)", bt_str(peer_id));
    RegisterLocalInitiatedLink(std::move(link));
    return;
  }

  // The request failed or timed out.
  bt_log(INFO, "gap-le", "failed to connect to peer (id: %s, status: %s)", bt_str(peer_id),
         bt_str(status));
  Peer* peer = peer_cache_->FindById(peer_id);
  ZX_ASSERT(peer);
  peer->MutLe().SetConnectionState(Peer::ConnectionState::kNotConnected);

  // Notify the matching pending callbacks about the failure.
  auto iter = pending_requests_.find(peer_id);
  ZX_ASSERT(iter != pending_requests_.end());

  if (scanning_) {
    bt_log(DEBUG, "gap-le", "canceling scanning (peer: %s)", bt_str(peer_id));
    scanning_ = false;
  }

  // Remove the entry from |pending_requests_| before notifying callbacks.
  auto pending_req_data = std::move(iter->second);
  pending_requests_.erase(iter);
  auto error = status.is_protocol_error() ? HostError::kFailed : status.error();
  pending_req_data.NotifyCallbacks(fit::error(error));

  // Process the next pending attempt.
  ZX_ASSERT(!connector_->request_pending());
  TryCreateNextConnection();
}

void LowEnergyConnectionManager::OnPeerDisconnect(const hci::Connection* connection,
                                                  hci::StatusCode reason) {
  auto handle = connection->handle();
  if (test_disconn_cb_) {
    test_disconn_cb_(handle);
  }

  // See if we can find a connection with a matching handle by walking the
  // connections list.
  auto iter = FindConnection(handle);
  if (iter == connections_.end()) {
    bt_log(TRACE, "gap-le", "disconnect from unknown connection handle: %#.4x", handle);
    return;
  }

  // Found the connection. Remove the entry from |connections_| before notifying
  // the "closed" handlers.
  auto conn = std::move(iter->second);
  connections_.erase(iter);

  bt_log(INFO, "gap-le", "peer %s disconnected (handle: %#.4x)", bt_str(conn->peer_id()), handle);

  // Retry connections that failed to be established.
  if (reason == hci::kConnectionFailedToBeEstablished && conn->request() &&
      conn->request()->connection_attempts() < kMaxConnectionAttempts) {
    CleanUpAndRetryConnection(std::move(conn));
    return;
  }

  CleanUpConnection(std::move(conn));
}

hci::CommandChannel::EventCallbackResult LowEnergyConnectionManager::OnLEConnectionUpdateComplete(
    const hci::EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kLEMetaEventCode);
  ZX_DEBUG_ASSERT(event.params<hci::LEMetaEventParams>().subevent_code ==
                  hci::kLEConnectionUpdateCompleteSubeventCode);

  auto payload = event.le_event_params<hci::LEConnectionUpdateCompleteSubeventParams>();
  ZX_ASSERT(payload);
  hci::ConnectionHandle handle = le16toh(payload->connection_handle);

  // This event may be the result of the LE Connection Update command.
  if (le_conn_update_complete_command_callback_) {
    le_conn_update_complete_command_callback_(handle, payload->status);
  }

  if (payload->status != hci::StatusCode::kSuccess) {
    bt_log(WARN, "gap-le",
           "HCI LE Connection Update Complete event with error "
           "(status: %#.2x, handle: %#.4x)",
           payload->status, handle);

    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  auto iter = FindConnection(handle);
  if (iter == connections_.end()) {
    bt_log(DEBUG, "gap-le", "conn. parameters received for unknown link (handle: %#.4x)", handle);
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  const auto& conn = *iter->second;
  ZX_DEBUG_ASSERT(conn.handle() == handle);

  bt_log(INFO, "gap-le", "conn. parameters updated (id: %s, handle: %#.4x)", bt_str(conn.peer_id()),
         handle);
  hci::LEConnectionParameters params(le16toh(payload->conn_interval),
                                     le16toh(payload->conn_latency),
                                     le16toh(payload->supervision_timeout));
  conn.link()->set_low_energy_parameters(params);

  Peer* peer = peer_cache_->FindById(conn.peer_id());
  if (!peer) {
    bt_log(ERROR, "gap-le", "conn. parameters updated for unknown peer!");
    return hci::CommandChannel::EventCallbackResult::kContinue;
  }

  peer->MutLe().SetConnectionParameters(params);

  if (test_conn_params_cb_)
    test_conn_params_cb_(*peer);

  return hci::CommandChannel::EventCallbackResult::kContinue;
}

void LowEnergyConnectionManager::OnNewLEConnectionParams(
    PeerId peer_id, hci::ConnectionHandle handle,
    const hci::LEPreferredConnectionParameters& params) {
  bt_log(DEBUG, "gap-le", "conn. parameters received (handle: %#.4x)", handle);

  Peer* peer = peer_cache_->FindById(peer_id);
  if (!peer) {
    bt_log(ERROR, "gap-le", "conn. parameters received from unknown peer!");
    return;
  }

  peer->MutLe().SetPreferredConnectionParameters(params);

  // Use the new parameters if we're not performing service discovery or
  // bonding.
  if (peer->le()->connected()) {
    UpdateConnectionParams(handle, params);
  }
}

void LowEnergyConnectionManager::RequestConnectionParameterUpdate(
    PeerId peer_id, const internal::LowEnergyConnection& conn,
    const hci::LEPreferredConnectionParameters& params) {
  ZX_ASSERT_MSG(conn.link()->role() == hci::Connection::Role::kSlave,
                "tried to send connection parameter update request as master");

  Peer* peer = peer_cache_->FindById(peer_id);
  // Ensure interrogation has completed.
  ZX_ASSERT(peer->le()->features().has_value());

  // TODO(fxbug.dev/49714): check local controller support for LL Connection Parameters Request
  // procedure (mask is currently in Adapter le state, consider propagating down)
  bool ll_connection_parameters_req_supported =
      peer->le()->features()->le_features &
      static_cast<uint64_t>(hci::LESupportedFeature::kConnectionParametersRequestProcedure);

  bt_log(TRACE, "gap-le", "ll connection parameters req procedure supported: %s",
         ll_connection_parameters_req_supported ? "true" : "false");

  if (ll_connection_parameters_req_supported) {
    auto status_cb = [self = weak_ptr_factory_.GetWeakPtr(), peer_id, params](hci::Status status) {
      if (!self) {
        return;
      }

      auto it = self->connections_.find(peer_id);
      if (it == self->connections_.end()) {
        bt_log(TRACE, "gap-le",
               "connection update command status for non-connected peer (peer id: %s)",
               bt_str(peer_id));
        return;
      }
      auto& conn = it->second;

      // The next LE Connection Update complete event is for this command iff the command status
      // is success.
      if (status.is_success()) {
        self->le_conn_update_complete_command_callback_ = [self, params, peer_id,
                                                           expected_handle = conn->handle()](
                                                              hci::ConnectionHandle handle,
                                                              hci::StatusCode status) {
          if (!self) {
            return;
          }

          if (handle != expected_handle) {
            bt_log(WARN, "gap-le",
                   "handle in conn update complete command callback (%#.4x) does not match handle "
                   "in command (%#.4x)",
                   handle, expected_handle);
            return;
          }

          auto it = self->connections_.find(peer_id);
          if (it == self->connections_.end()) {
            bt_log(TRACE, "gap-le",
                   "connection update complete event for non-connected peer (peer id: %s)",
                   bt_str(peer_id));
            return;
          }
          auto& conn = it->second;

          // Retry connection parameter update with l2cap if the peer doesn't support LL
          // procedure.
          if (status == hci::StatusCode::kUnsupportedRemoteFeature) {
            bt_log(TRACE, "gap-le",
                   "peer does not support HCI LE Connection Update command, trying l2cap request");
            self->L2capRequestConnectionParameterUpdate(*conn, params);
          }
        };

      } else if (status.protocol_error() == hci::StatusCode::kUnsupportedRemoteFeature) {
        // Retry connection parameter update with l2cap if the peer doesn't support LL procedure.
        bt_log(TRACE, "gap-le",
               "peer does not support HCI LE Connection Update command, trying l2cap request");
        self->L2capRequestConnectionParameterUpdate(*conn, params);
      }
    };

    UpdateConnectionParams(conn.handle(), params, std::move(status_cb));
  } else {
    L2capRequestConnectionParameterUpdate(conn, params);
  }
}

void LowEnergyConnectionManager::UpdateConnectionParams(
    hci::ConnectionHandle handle, const hci::LEPreferredConnectionParameters& params,
    StatusCallback status_cb) {
  bt_log(DEBUG, "gap-le", "updating connection parameters (handle: %#.4x)", handle);
  auto command = hci::CommandPacket::New(hci::kLEConnectionUpdate,
                                         sizeof(hci::LEConnectionUpdateCommandParams));
  auto event_params = command->mutable_payload<hci::LEConnectionUpdateCommandParams>();

  event_params->connection_handle = htole16(handle);
  event_params->conn_interval_min = htole16(params.min_interval());
  event_params->conn_interval_max = htole16(params.max_interval());
  event_params->conn_latency = htole16(params.max_latency());
  event_params->supervision_timeout = htole16(params.supervision_timeout());
  event_params->minimum_ce_length = 0x0000;
  event_params->maximum_ce_length = 0x0000;

  auto status_cb_wrapper = [handle, cb = std::move(status_cb)](
                               auto id, const hci::EventPacket& event) mutable {
    ZX_ASSERT(event.event_code() == hci::kCommandStatusEventCode);
    hci_is_error(event, TRACE, "gap-le",
                 "controller rejected connection parameters (handle: %#.4x)", handle);
    if (cb) {
      cb(event.ToStatus());
    }
  };

  hci_->command_channel()->SendCommand(std::move(command), std::move(status_cb_wrapper),
                                       hci::kCommandStatusEventCode);
}

void LowEnergyConnectionManager::L2capRequestConnectionParameterUpdate(
    const internal::LowEnergyConnection& conn, const hci::LEPreferredConnectionParameters& params) {
  ZX_ASSERT_MSG(conn.link()->role() == hci::Connection::Role::kSlave,
                "tried to send l2cap connection parameter update request as master");

  bt_log(DEBUG, "gap-le", "sending l2cap connection parameter update request");

  auto handle = conn.handle();
  auto response_cb = [handle](bool accepted) {
    bt_log(DEBUG, "gap-le", "peer %s l2cap connection parameter update request (handle: %#.4x)",
           accepted ? "accepted" : "rejected", handle);
  };

  // TODO(fxbug.dev/49717): don't send request until after kLEConnectionParameterTimeout of an
  // l2cap conn parameter update response being received (Core Spec v5.2, Vol 3, Part C,
  // Sec 9.3.9).
  l2cap_->RequestConnectionParameterUpdate(handle, params, std::move(response_cb));
}

void LowEnergyConnectionManager::CleanUpAndRetryConnection(
    std::unique_ptr<internal::LowEnergyConnection> connection) {
  auto peer_id = connection->peer_id();
  auto request = connection->take_request();

  CleanUpConnection(std::move(connection));

  Peer* peer = peer_cache_->FindById(peer_id);
  ZX_ASSERT(peer);
  peer->MutLe().SetConnectionState(Peer::ConnectionState::kInitializing);

  auto [_, inserted] = pending_requests_.emplace(peer_id, std::move(request.value()));
  ZX_ASSERT(inserted);

  TryCreateNextConnection();
}

LowEnergyConnectionManager::ConnectionMap::iterator LowEnergyConnectionManager::FindConnection(
    hci::ConnectionHandle handle) {
  auto iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    const auto& conn = *iter->second;
    if (conn.handle() == handle)
      break;
  }
  return iter;
}

void LowEnergyConnectionManager::CancelPendingRequest(PeerId peer_id) {
  auto request_iter = pending_requests_.find(peer_id);
  ZX_ASSERT(request_iter != pending_requests_.end());

  bt_log(INFO, "gap-le", "canceling pending connection request (peer: %s)", bt_str(peer_id));

  // Only cancel the connector if it is pending for this peer request. Otherwise, the request must
  // be pending scan start or in the scanning state.
  auto address = request_iter->second.address();
  if (connector_->pending_peer_address() == std::optional(address)) {
    // Connector will call OnConnectResult to notify callbacks and try next connection.
    connector_->Cancel();
  } else {
    // Cancel scanning by removing pending request. OnScanStart will detect that the request was
    // removed and abort.
    OnConnectResult(peer_id, hci::Status(HostError::kCanceled), nullptr);
  }
}

}  // namespace bt::gap
