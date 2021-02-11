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
static const hci::LEPreferredConnectionParameters kInitialConnectionParameters(
    kLEInitialConnIntervalMin, kLEInitialConnIntervalMax, /*max_latency=*/0,
    hci::defaults::kLESupervisionTimeout);

constexpr int kMaxConnectionAttempts = 3;

}  // namespace

std::unique_ptr<LowEnergyConnector> LowEnergyConnector::CreateOutboundConnector(
    PeerId peer_id, LowEnergyConnectionOptions options, hci::LowEnergyConnector* connector,
    zx::duration request_timeout, fxl::WeakPtr<hci::Transport> transport, PeerCache* peer_cache,
    fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager,
    fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr, fbl::RefPtr<l2cap::L2cap> l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb) {
  return std::unique_ptr<LowEnergyConnector>(new LowEnergyConnector(
      /*outbound=*/true, peer_id, /*connection=*/nullptr, options, connector, request_timeout,
      transport, peer_cache, conn_mgr, discovery_manager, l2cap, gatt, std::move(cb)));
}

std::unique_ptr<LowEnergyConnector> LowEnergyConnector::CreateInboundConnector(
    PeerId peer_id, std::unique_ptr<hci::Connection> connection, LowEnergyConnectionOptions options,
    fxl::WeakPtr<hci::Transport> transport, PeerCache* peer_cache,
    fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr, fbl::RefPtr<l2cap::L2cap> l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb) {
  return std::unique_ptr<LowEnergyConnector>(new LowEnergyConnector(
      /*outbound=*/false, peer_id, std::move(connection), options, /*connector=*/nullptr,
      /*request_timeout=*/zx::duration(0), transport, peer_cache, conn_mgr,
      /*discovery_manager=*/nullptr, l2cap, gatt, std::move(cb)));
}

LowEnergyConnector::LowEnergyConnector(
    bool outbound, PeerId peer_id, std::unique_ptr<hci::Connection> connection,
    LowEnergyConnectionOptions options, hci::LowEnergyConnector* connector,
    zx::duration request_timeout, fxl::WeakPtr<hci::Transport> transport, PeerCache* peer_cache,
    fxl::WeakPtr<LowEnergyConnectionManager> conn_mgr,
    fxl::WeakPtr<LowEnergyDiscoveryManager> discovery_manager, fbl::RefPtr<l2cap::L2cap> l2cap,
    fxl::WeakPtr<gatt::GATT> gatt, ResultCallback cb)
    : state_(State::kIdle),
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
      interrogator_(peer_cache_, transport, async_get_default_dispatcher()),
      discovery_manager_(std::move(discovery_manager)),
      transport_(transport),
      le_connection_manager_(conn_mgr),
      weak_ptr_factory_(this) {
  ZX_ASSERT(le_connection_manager_);
  ZX_ASSERT(transport_);
  ZX_ASSERT(peer_cache_);

  auto peer = peer_cache_->FindById(peer_id_);
  ZX_ASSERT(peer);
  peer_address_ = peer->address();

  if (is_outbound_) {
    ZX_ASSERT(discovery_manager_);
    ZX_ASSERT(!connection);
    ZX_ASSERT(hci_connector_);
    ZX_ASSERT(hci_request_timeout_.get() != 0);

    if (options.auto_connect) {
      RequestCreateConnection();
    } else {
      StartScanningForPeer();
    }
  } else {
    ZX_ASSERT(connection);
    ZX_ASSERT(peer_address_.value() == connection->peer_address().value());

    InitializeConnection(std::move(connection));
    StartInterrogation();
  }
}

LowEnergyConnector::~LowEnergyConnector() {
  if (state_ != State::kComplete && state_ != State::kFailed) {
    bt_log(WARN, "gap-le", "destroying LowEnergyConnector before procedure completed (peer: %s)",
           bt_str(peer_id_));
    NotifyFailure(hci::Status(HostError::kCanceled));
  }

  if (hci_connector_ && hci_connector_->request_pending()) {
    // NOTE: LowEnergyConnector will be unable to wait for the connection to be canceled. The
    // hci::LowEnergyConnector may still be waiting to cancel the connection when a later
    // gap::internal::LowEnergyConnector is created.
    hci_connector_->Cancel();
  }

  interrogator_.Cancel(peer_id_);
}

void LowEnergyConnector::Cancel() {
  bt_log(INFO, "gap-le", "canceling connector (peer: %s, state: %s)", bt_str(peer_id_),
         StateToString(state_));

  switch (state_) {
    case State::kStartingScanning:
      discovery_session_.reset();
      NotifyFailure(hci::Status(HostError::kCanceled));
      break;
    case State::kScanning:
      discovery_session_.reset();
      scan_timeout_task_.reset();
      NotifyFailure(hci::Status(HostError::kCanceled));
      break;
    case State::kConnecting:
      // The connector will call the result callback with a cancelled result.
      hci_connector_->Cancel();
      break;
    case State::kInterrogating:
      // The interrogator will call the result callback with a cancelled result.
      interrogator_.Cancel(peer_id_);
      break;
    case State::kAwaitingConnectionFailedToBeEstablishedDisconnect:
      // Waiting for disconnect complete, nothing to do.
    case State::kComplete:
    case State::kFailed:
      // Cancelling completed/failed connector is a no-op.
      break;
    case State::kIdle:
      // It should not be possible to cancel during kIdle as the state is immediately changed.
      ZX_PANIC("Cancel called during kIdle");
  }
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
    case State::kComplete:
      return "Complete";
    case State::kFailed:
      return "Failed";
  }
}

void LowEnergyConnector::StartScanningForPeer() {
  auto self = weak_ptr_factory_.GetWeakPtr();

  state_ = State::kStartingScanning;

  discovery_manager_->StartDiscovery(/*active=*/false, [self](auto session) {
    if (self) {
      self->OnScanStart(std::move(session));
    }
  });
}

void LowEnergyConnector::OnScanStart(LowEnergyDiscoverySessionPtr session) {
  if (state_ == State::kFailed) {
    return;
  }
  ZX_ASSERT(state_ == State::kStartingScanning);

  // Failed to start scan, abort connection procedure.
  if (!session) {
    bt_log(INFO, "gap-le", "failed to start scan (peer: %s)", bt_str(peer_id_));
    NotifyFailure(hci::Status(HostError::kFailed));
    return;
  }

  bt_log(INFO, "gap-le", "started scanning for pending connection (peer: %s)", bt_str(peer_id_));
  state_ = State::kScanning;

  auto self = weak_ptr_factory_.GetWeakPtr();
  scan_timeout_task_.emplace([this] {
    ZX_ASSERT(state_ == State::kScanning);
    bt_log(INFO, "gap-le", "scan for pending connection timed out (peer: %s)", bt_str(peer_id_));
    NotifyFailure(hci::Status(HostError::kTimedOut));
  });
  // The scan timeout may include time during which scanning is paused.
  scan_timeout_task_->PostDelayed(async_get_default_dispatcher(), kLEGeneralCepScanTimeout);

  discovery_session_ = std::move(session);
  discovery_session_->filter()->set_connectable(true);

  // The error callback must be set before the result callback in case the result callback is called
  // synchronously.
  discovery_session_->set_error_callback([self] {
    ZX_ASSERT(self->state_ == State::kScanning);
    bt_log(INFO, "gap-le", "discovery error while scanning for peer (peer: %s)",
           bt_str(self->peer_id_));
    self->scan_timeout_task_.reset();
    self->NotifyFailure(hci::Status(HostError::kFailed));
  });

  discovery_session_->SetResultCallback([self](auto& peer) {
    ZX_ASSERT(self->state_ == State::kScanning);

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
  ZX_ASSERT(state_ == State::kIdle || state_ == State::kScanning ||
            state_ == State::kInterrogating ||
            state_ == State::kAwaitingConnectionFailedToBeEstablishedDisconnect);

  // Pause discovery until connection complete.
  auto pause_token = discovery_manager_->PauseDiscovery();

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self, pause = std::move(pause_token)](hci::Status status, auto link) {
    if (self) {
      self->OnConnectResult(status, std::move(link));
    }
  };

  state_ = State::kConnecting;

  // We set the scan window and interval to the same value for continuous
  // scanning.
  ZX_ASSERT(hci_connector_->CreateConnection(
      /*use_whitelist=*/false, peer_address_, kLEScanFastInterval, kLEScanFastInterval,
      kInitialConnectionParameters, std::move(status_cb), hci_request_timeout_));
}

void LowEnergyConnector::OnConnectResult(hci::Status status, hci::ConnectionPtr link) {
  if (!status) {
    bt_log(INFO, "gap-le", "failed to connect to peer (id: %s, status: %s)", bt_str(peer_id_),
           bt_str(status));

    NotifyFailure(status);
    return;
  }
  ZX_ASSERT(link);

  bt_log(INFO, "gap-le", "connection request successful (peer: %s)", bt_str(peer_id_));

  InitializeConnection(std::move(link));
  StartInterrogation();
}

void LowEnergyConnector::InitializeConnection(hci::ConnectionPtr link) {
  ZX_ASSERT(link);

  auto peer_disconnect_cb = fit::bind_member(this, &LowEnergyConnector::OnPeerDisconnect);
  auto error_cb = [this]() { NotifyFailure(); };

  connection_ = std::make_unique<LowEnergyConnection>(
      peer_id_, std::move(link), options_, peer_disconnect_cb, error_cb, le_connection_manager_,
      l2cap_, gatt_, transport_);
}

void LowEnergyConnector::StartInterrogation() {
  ZX_ASSERT((is_outbound_ && state_ == State::kConnecting) ||
            (!is_outbound_ && state_ == State::kIdle));
  ZX_ASSERT(connection_);

  state_ = State::kInterrogating;
  interrogator_.Start(peer_id_, connection_->handle(),
                      fit::bind_member(this, &LowEnergyConnector::OnInterrogationComplete));
}

void LowEnergyConnector::OnInterrogationComplete(hci::Status status) {
  if (state_ == State::kFailed) {
    return;
  }
  ZX_ASSERT(state_ == State::kInterrogating);
  ZX_ASSERT(connection_);

  // If the controller responds to an interrogation command with the 0x3e
  // "kConnectionFailedToBeEstablished" error, it will send a Disconnection Complete event soon
  // after. Wait for this event before initating a retry.
  if (status.is_protocol_error() &&
      status.protocol_error() == hci::kConnectionFailedToBeEstablished) {
    bt_log(INFO, "gap-le",
           "Received kConnectionFailedToBeEstablished during interrogation. Waiting for Disconnect "
           "Complete. (peer: %s)",
           bt_str(peer_id_));
    state_ = State::kAwaitingConnectionFailedToBeEstablishedDisconnect;
    return;
  }

  if (!status.is_success()) {
    bt_log(INFO, "gap-le", "interrogation failed with %s (peer: %s)", bt_str(status),
           bt_str(peer_id_));
    NotifyFailure();
    return;
  }

  NotifySuccess();
}

void LowEnergyConnector::OnPeerDisconnect(hci::StatusCode status) {
  // The peer can't disconnect while scanning or connecting, and we unregister from
  // disconnects after kFailed & kComplete.
  ZX_ASSERT_MSG(state_ == State::kInterrogating ||
                    state_ == State::kAwaitingConnectionFailedToBeEstablishedDisconnect,
                "Received peer disconnect during invalid state (state: %s, status: %s)",
                StateToString(state_), bt_str(hci::Status(status)));
  if (state_ == State::kInterrogating &&
      status != hci::StatusCode::kConnectionFailedToBeEstablished) {
    interrogator_.Cancel(peer_id_);
    NotifyFailure(hci::Status(status));
    return;
  }

  // state_ is kAwaitingConnectionFailedToBeEstablished or kInterrogating with a 0x3e error, so
  // retry connection
  if (!MaybeRetryConnection()) {
    NotifyFailure(hci::Status(status));
  }
}

bool LowEnergyConnector::MaybeRetryConnection() {
  // Only retry outbound connections.
  if (is_outbound_ && connection_attempt_ < kMaxConnectionAttempts - 1) {
    connection_.reset();
    connection_attempt_++;
    RequestCreateConnection();
    return true;
  }
  return false;
}

void LowEnergyConnector::NotifySuccess() {
  ZX_ASSERT(state_ == State::kInterrogating);
  ZX_ASSERT(connection_);
  ZX_ASSERT(result_cb_);

  state_ = State::kComplete;

  // LowEnergyConnectionManager should immediately set handlers to replace these ones.
  connection_->set_peer_disconnect_callback([peer_id = peer_id_](auto) {
    ZX_PANIC("Peer disconnected without handler set (peer: %s)", bt_str(peer_id));
  });

  connection_->set_error_callback([peer_id = peer_id_]() {
    ZX_PANIC("connection error without handler set (peer: %s)", bt_str(peer_id));
  });

  result_cb_(fit::ok(std::move(connection_)));
}

void LowEnergyConnector::NotifyFailure(hci::Status status) {
  state_ = State::kFailed;
  // The result callback must only be called once, so extraneous failures should be ignored.
  if (result_cb_) {
    result_cb_(fit::error(status));
  }
}

}  // namespace bt::gap::internal
