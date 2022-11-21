// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connector.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"

namespace bt::gap::internal {

namespace {

// During the initial connection to a peripheral we use the initial high
// duty-cycle parameters to ensure that initiating procedures (bonding,
// encryption setup, service discovery) are completed quickly. Once these
// procedures are complete, we will change the connection interval to the
// peripheral's preferred connection parameters (see v5.0, Vol 3, Part C,
// Section 9.3.12).
static const hci_spec::LEPreferredConnectionParameters kInitialConnectionParameters(
    kLEInitialConnIntervalMin, kLEInitialConnIntervalMax, /*max_latency=*/0,
    hci_spec::defaults::kLESupervisionTimeout);

constexpr int kMaxConnectionAttempts = 3;
constexpr int kRetryExponentialBackoffBase = 2;

constexpr const char* kInspectPeerIdPropertyName = "peer_id";
constexpr const char* kInspectConnectionAttemptPropertyName = "connection_attempt";
constexpr const char* kInspectStatePropertyName = "state";
constexpr const char* kInspectIsOutboundPropertyName = "is_outbound";

}  // namespace

std::unique_ptr<LowEnergyConnector> LowEnergyConnector::CreateOutboundConnector(
    PeerId peer_id, LowEnergyConnectionOptions options, hci::LowEnergyConnector* connector,
    zx::duration request_timeout, fxl::WeakPtr<hci::Transport> transport, PeerCache* peer_cache,
    fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager,
    fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr, l2cap::ChannelManager* l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb) {
  return std::unique_ptr<LowEnergyConnector>(new LowEnergyConnector(
      /*outbound=*/true, peer_id, /*connection=*/nullptr, options, connector, request_timeout,
      transport, peer_cache, conn_mgr, discovery_manager, l2cap, gatt, std::move(cb)));
}

std::unique_ptr<LowEnergyConnector> LowEnergyConnector::CreateInboundConnector(
    PeerId peer_id, std::unique_ptr<hci::LowEnergyConnection> connection,
    LowEnergyConnectionOptions options, fxl::WeakPtr<hci::Transport> transport,
    PeerCache* peer_cache, fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
    l2cap::ChannelManager* l2cap, fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb) {
  return std::unique_ptr<LowEnergyConnector>(new LowEnergyConnector(
      /*outbound=*/false, peer_id, std::move(connection), options, /*connector=*/nullptr,
      /*request_timeout=*/zx::duration(0), transport, peer_cache, conn_mgr,
      /*discovery_manager=*/nullptr, l2cap, gatt, std::move(cb)));
}

LowEnergyConnector::LowEnergyConnector(
    bool outbound, PeerId peer_id, std::unique_ptr<hci::LowEnergyConnection> connection,
    LowEnergyConnectionOptions options, hci::LowEnergyConnector* connector,
    zx::duration request_timeout, fxl::WeakPtr<hci::Transport> transport, PeerCache* peer_cache,
    fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
    fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager, l2cap::ChannelManager* l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb)
    : state_(State::kIdle, /*convert=*/[](auto s) { return StateToString(s); }),
      peer_id_(peer_id),
      peer_cache_(peer_cache),
      l2cap_(l2cap),
      gatt_(gatt),
      is_outbound_(outbound),
      hci_request_timeout_(request_timeout),
      options_(options),
      result_cb_(std::move(cb)),
      hci_connector_(connector),
      connection_attempt_(0),
      discovery_manager_(std::move(discovery_manager)),
      transport_(transport),
      le_connection_manager_(conn_mgr),
      weak_ptr_factory_(this) {
  BT_ASSERT(le_connection_manager_);
  BT_ASSERT(transport_);
  BT_ASSERT(peer_cache_);

  auto peer = peer_cache_->FindById(peer_id_);
  BT_ASSERT(peer);
  peer_address_ = peer->address();

  if (is_outbound_) {
    BT_ASSERT(discovery_manager_);
    BT_ASSERT(!connection);
    BT_ASSERT(hci_connector_);
    BT_ASSERT(hci_request_timeout_.get() != 0);

    if (options.auto_connect) {
      RequestCreateConnection();
    } else {
      StartScanningForPeer();
    }
  } else {
    BT_ASSERT(connection);
    // Connection address should resolve to same peer as the given peer ID.
    Peer* conn_peer = peer_cache_->FindByAddress(connection->peer_address());
    BT_ASSERT(conn_peer);
    BT_ASSERT_MSG(peer_id_ == conn_peer->identifier(), "peer_id_ (%s) != connection peer (%s)",
                  bt_str(peer_id_), bt_str(conn_peer->identifier()));

    InitializeConnection(std::move(connection));
    StartInterrogation();
  }
}

LowEnergyConnector::~LowEnergyConnector() {
  if (*state_ != State::kComplete && *state_ != State::kFailed) {
    bt_log(WARN, "gap-le", "destroying LowEnergyConnector before procedure completed (peer: %s)",
           bt_str(peer_id_));
    NotifyFailure(ToResult(HostError::kCanceled));
  }

  if (hci_connector_ && hci_connector_->request_pending()) {
    // NOTE: LowEnergyConnector will be unable to wait for the connection to be canceled. The
    // hci::LowEnergyConnector may still be waiting to cancel the connection when a later
    // gap::internal::LowEnergyConnector is created.
    hci_connector_->Cancel();
  }
}

void LowEnergyConnector::Cancel() {
  bt_log(INFO, "gap-le", "canceling connector (peer: %s, state: %s)", bt_str(peer_id_),
         StateToString(*state_));

  switch (*state_) {
    case State::kStartingScanning:
      discovery_session_.reset();
      NotifyFailure(ToResult(HostError::kCanceled));
      break;
    case State::kScanning:
      discovery_session_.reset();
      scan_timeout_task_.reset();
      NotifyFailure(ToResult(HostError::kCanceled));
      break;
    case State::kConnecting:
      // The connector will call the result callback with a cancelled result.
      hci_connector_->Cancel();
      break;
    case State::kInterrogating:
      // The interrogator will call the result callback with a cancelled result.
      interrogator_->Cancel();
      break;
    case State::kPauseBeforeConnectionRetry:
      request_create_connection_task_.Cancel();
      NotifyFailure(ToResult(HostError::kCanceled));
      break;
    case State::kAwaitingConnectionFailedToBeEstablishedDisconnect:
      // Waiting for disconnect complete, nothing to do.
    case State::kComplete:
    case State::kFailed:
      // Cancelling completed/failed connector is a no-op.
      break;
    case State::kIdle:
      // It should not be possible to cancel during kIdle as the state is immediately changed.
      BT_PANIC("Cancel called during kIdle");
  }
}

void LowEnergyConnector::AttachInspect(inspect::Node& parent, std::string name) {
  inspect_node_ = parent.CreateChild(name);
  inspect_properties_.peer_id =
      inspect_node_.CreateString(kInspectPeerIdPropertyName, peer_id_.ToString());
  connection_attempt_.AttachInspect(inspect_node_, kInspectConnectionAttemptPropertyName);
  state_.AttachInspect(inspect_node_, kInspectStatePropertyName);
  inspect_properties_.is_outbound =
      inspect_node_.CreateBool(kInspectIsOutboundPropertyName, is_outbound_);
}

const char* LowEnergyConnector::StateToString(State state) {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kStartingScanning:
      return "StartingScanning";
    case State::kScanning:
      return "Scanning";
    case State::kConnecting:
      return "Connecting";
    case State::kInterrogating:
      return "Interrogating";
    case State::kAwaitingConnectionFailedToBeEstablishedDisconnect:
      return "AwaitingConnectionFailedToBeEstablishedDisconnect";
    case State::kPauseBeforeConnectionRetry:
      return "PauseBeforeConnectionRetry";
    case State::kComplete:
      return "Complete";
    case State::kFailed:
      return "Failed";
  }
}

void LowEnergyConnector::StartScanningForPeer() {
  auto self = weak_ptr_factory_.GetWeakPtr();

  state_.Set(State::kStartingScanning);

  discovery_manager_->StartDiscovery(/*active=*/false, [self](auto session) {
    if (self) {
      self->OnScanStart(std::move(session));
    }
  });
}

void LowEnergyConnector::OnScanStart(LowEnergyDiscoverySessionPtr session) {
  if (*state_ == State::kFailed) {
    return;
  }
  BT_ASSERT(*state_ == State::kStartingScanning);

  // Failed to start scan, abort connection procedure.
  if (!session) {
    bt_log(INFO, "gap-le", "failed to start scan (peer: %s)", bt_str(peer_id_));
    NotifyFailure(ToResult(HostError::kFailed));
    return;
  }

  bt_log(INFO, "gap-le", "started scanning for pending connection (peer: %s)", bt_str(peer_id_));
  state_.Set(State::kScanning);

  auto self = weak_ptr_factory_.GetWeakPtr();
  scan_timeout_task_.emplace([this] {
    BT_ASSERT(*state_ == State::kScanning);
    bt_log(INFO, "gap-le", "scan for pending connection timed out (peer: %s)", bt_str(peer_id_));
    NotifyFailure(ToResult(HostError::kTimedOut));
  });
  // The scan timeout may include time during which scanning is paused.
  scan_timeout_task_->PostDelayed(async_get_default_dispatcher(), kLEGeneralCepScanTimeout);

  discovery_session_ = std::move(session);
  discovery_session_->filter()->set_connectable(true);

  // The error callback must be set before the result callback in case the result callback is called
  // synchronously.
  discovery_session_->set_error_callback([self] {
    BT_ASSERT(self->state_.value() == State::kScanning);
    bt_log(INFO, "gap-le", "discovery error while scanning for peer (peer: %s)",
           bt_str(self->peer_id_));
    self->scan_timeout_task_.reset();
    self->NotifyFailure(ToResult(HostError::kFailed));
  });

  discovery_session_->SetResultCallback([self](auto& peer) {
    BT_ASSERT(self->state_.value() == State::kScanning);

    if (peer.identifier() != self->peer_id_) {
      return;
    }

    bt_log(INFO, "gap-le", "discovered peer for pending connection (peer: %s)",
           bt_str(self->peer_id_));

    self->scan_timeout_task_.reset();
    self->discovery_session_->Stop();

    self->RequestCreateConnection();
  });
}

void LowEnergyConnector::RequestCreateConnection() {
  // Scanning may be skipped. When the peer disconnects during/after interrogation, a retry may be
  // initiated by calling this method.
  BT_ASSERT(*state_ == State::kIdle || *state_ == State::kScanning ||
            *state_ == State::kPauseBeforeConnectionRetry);

  // Pause discovery until connection complete.
  auto pause_token = discovery_manager_->PauseDiscovery();

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self, pause = std::move(pause_token)](hci::Result<> status, auto link) {
    if (self) {
      self->OnConnectResult(status, std::move(link));
    }
  };

  state_.Set(State::kConnecting);

  // TODO(fxbug.dev/70199): Use slow interval & window for auto connections during background scan.
  BT_ASSERT(hci_connector_->CreateConnection(
      /*use_accept_list=*/false, peer_address_, kLEScanFastInterval, kLEScanFastWindow,
      kInitialConnectionParameters, std::move(status_cb), hci_request_timeout_));
}

void LowEnergyConnector::OnConnectResult(hci::Result<> status,
                                         std::unique_ptr<hci::LowEnergyConnection> link) {
  if (status.is_error()) {
    bt_log(INFO, "gap-le", "failed to connect to peer (id: %s, status: %s)", bt_str(peer_id_),
           bt_str(status));

    NotifyFailure(status);
    return;
  }
  BT_ASSERT(link);

  bt_log(INFO, "gap-le", "connection request successful (peer: %s)", bt_str(peer_id_));

  if (InitializeConnection(std::move(link))) {
    StartInterrogation();
  }
}

bool LowEnergyConnector::InitializeConnection(std::unique_ptr<hci::LowEnergyConnection> link) {
  BT_ASSERT(link);

  auto peer_disconnect_cb = fit::bind_member<&LowEnergyConnector::OnPeerDisconnect>(this);
  auto error_cb = [this]() { NotifyFailure(); };

  Peer* peer = peer_cache_->FindById(peer_id_);
  BT_ASSERT(peer);
  auto connection =
      LowEnergyConnection::Create(peer->GetWeakPtr(), std::move(link), options_, peer_disconnect_cb,
                                  error_cb, le_connection_manager_, l2cap_, gatt_, transport_);
  if (!connection) {
    bt_log(WARN, "gap-le", "connection initialization failed (peer: %s)", bt_str(peer_id_));
    NotifyFailure();
    return false;
  }

  connection_ = std::move(connection);
  return true;
}

void LowEnergyConnector::StartInterrogation() {
  BT_ASSERT((is_outbound_ && *state_ == State::kConnecting) ||
            (!is_outbound_ && *state_ == State::kIdle));
  BT_ASSERT(connection_);

  state_.Set(State::kInterrogating);
  auto peer = peer_cache_->FindById(peer_id_);
  BT_ASSERT(peer);
  interrogator_.emplace(peer->GetWeakPtr(), connection_->handle(), transport_);
  interrogator_->Start(fit::bind_member<&LowEnergyConnector::OnInterrogationComplete>(this));
}

void LowEnergyConnector::OnInterrogationComplete(hci::Result<> status) {
  // If a disconnect event is received before interrogation completes, state_ will be either kFailed
  // or kPauseBeforeConnectionRetry depending on the status of the disconnect.
  BT_ASSERT(*state_ == State::kInterrogating || *state_ == State::kFailed ||
            *state_ == State::kPauseBeforeConnectionRetry);
  if (*state_ == State::kFailed || *state_ == State::kPauseBeforeConnectionRetry) {
    return;
  }

  BT_ASSERT(connection_);

  // If the controller responds to an interrogation command with the 0x3e
  // "kConnectionFailedToBeEstablished" error, it will send a Disconnection Complete event soon
  // after. Wait for this event before initiating a retry.
  if (status == ToResult(hci_spec::StatusCode::CONNECTION_FAILED_TO_BE_ESTABLISHED)) {
    bt_log(INFO, "gap-le",
           "Received kConnectionFailedToBeEstablished during interrogation. Waiting for Disconnect "
           "Complete. (peer: %s)",
           bt_str(peer_id_));
    state_.Set(State::kAwaitingConnectionFailedToBeEstablishedDisconnect);
    return;
  }

  if (status.is_error()) {
    bt_log(INFO, "gap-le", "interrogation failed with %s (peer: %s)", bt_str(status),
           bt_str(peer_id_));
    NotifyFailure();
    return;
  }

  connection_->OnInterrogationComplete();
  NotifySuccess();
}

void LowEnergyConnector::OnPeerDisconnect(hci_spec::StatusCode status_code) {
  // The peer can't disconnect while scanning or connecting, and we unregister from
  // disconnects after kFailed & kComplete.
  BT_ASSERT_MSG(*state_ == State::kInterrogating ||
                    *state_ == State::kAwaitingConnectionFailedToBeEstablishedDisconnect,
                "Received peer disconnect during invalid state (state: %s, status: %s)",
                StateToString(*state_), bt_str(ToResult(status_code)));
  if (*state_ == State::kInterrogating &&
      status_code != hci_spec::StatusCode::CONNECTION_FAILED_TO_BE_ESTABLISHED) {
    NotifyFailure(ToResult(status_code));
    return;
  }

  // state_ is kAwaitingConnectionFailedToBeEstablished or kInterrogating with a 0x3e error, so
  // retry connection
  if (!MaybeRetryConnection()) {
    NotifyFailure(ToResult(status_code));
  }
}

bool LowEnergyConnector::MaybeRetryConnection() {
  // Only retry outbound connections.
  if (is_outbound_ && *connection_attempt_ < kMaxConnectionAttempts - 1) {
    connection_.reset();
    state_.Set(State::kPauseBeforeConnectionRetry);

    // Exponential backoff (2s, 4s, 8s, ...)
    zx::duration retry_delay = zx::sec(kRetryExponentialBackoffBase << *connection_attempt_);

    connection_attempt_.Set(*connection_attempt_ + 1);
    bt_log(INFO, "gap-le", "Retrying connection in %lds (peer: %s, attempt: %d)",
           retry_delay.to_secs(), bt_str(peer_id_), *connection_attempt_);
    request_create_connection_task_.PostDelayed(async_get_default_dispatcher(), retry_delay);
    return true;
  }
  return false;
}

void LowEnergyConnector::NotifySuccess() {
  BT_ASSERT(*state_ == State::kInterrogating);
  BT_ASSERT(connection_);
  BT_ASSERT(result_cb_);

  state_.Set(State::kComplete);

  // LowEnergyConnectionManager should immediately set handlers to replace these ones.
  connection_->set_peer_disconnect_callback([peer_id = peer_id_](auto) {
    BT_PANIC("Peer disconnected without handler set (peer: %s)", bt_str(peer_id));
  });

  connection_->set_error_callback([peer_id = peer_id_]() {
    BT_PANIC("connection error without handler set (peer: %s)", bt_str(peer_id));
  });

  result_cb_(fit::ok(std::move(connection_)));
}

void LowEnergyConnector::NotifyFailure(hci::Result<> status) {
  state_.Set(State::kFailed);
  // The result callback must only be called once, so extraneous failures should be ignored.
  if (result_cb_) {
    result_cb_(fit::error(status.take_error()));
  }
}

}  // namespace bt::gap::internal
