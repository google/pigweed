// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_central_server.h"

#include <zircon/assert.h>

#include "gatt_client_server.h"
#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Int8;
using fuchsia::bluetooth::Status;

using bt::sm::BondableMode;
using fuchsia::bluetooth::gatt::Client;
using fuchsia::bluetooth::le::ScanFilterPtr;

namespace bthost {

LowEnergyCentralServer::LowEnergyCentralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                                               fidl::InterfaceRequest<Central> request,
                                               fxl::WeakPtr<bt::gatt::GATT> gatt)
    : AdapterServerBase(adapter, this, std::move(request)),
      gatt_(gatt),
      requesting_scan_(false),
      weak_ptr_factory_(this) {
  ZX_ASSERT(gatt_);
}

std::optional<bt::gap::LowEnergyConnectionHandle*> LowEnergyCentralServer::FindConnectionForTesting(
    bt::PeerId identifier) {
  auto conn_iter = connections_.find(identifier);
  if (conn_iter != connections_.end()) {
    return conn_iter->second.get();
  }
  return std::nullopt;
}

void LowEnergyCentralServer::GetPeripherals(::fidl::VectorPtr<::std::string> service_uuids,
                                            GetPeripheralsCallback callback) {
  // TODO:
  bt_log(ERROR, "bt-host", "GetPeripherals() not implemented");
}

void LowEnergyCentralServer::GetPeripheral(::std::string identifier,
                                           GetPeripheralCallback callback) {
  // TODO:
  bt_log(ERROR, "bt-host", "GetPeripheral() not implemented");
}

void LowEnergyCentralServer::StartScan(ScanFilterPtr filter, StartScanCallback callback) {
  bt_log(DEBUG, "bt-host", "StartScan()");

  if (requesting_scan_) {
    bt_log(DEBUG, "bt-host", "scan request already in progress");
    callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS, "Scan request in progress"));
    return;
  }

  if (filter && !fidl_helpers::IsScanFilterValid(*filter)) {
    bt_log(DEBUG, "bt-host", "invalid scan filter given");
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS,
                                        "ScanFilter contains an invalid UUID"));
    return;
  }

  if (scan_session_) {
    // A scan is already in progress. Update its filter and report success.
    scan_session_->filter()->Reset();
    fidl_helpers::PopulateDiscoveryFilter(*filter, scan_session_->filter());
    callback(Status());
    return;
  }

  requesting_scan_ = true;
  adapter()->le()->StartDiscovery(/*active=*/true, [self = weak_ptr_factory_.GetWeakPtr(),
                                                    filter = std::move(filter),
                                                    callback = std::move(callback)](auto session) {
    if (!self)
      return;

    self->requesting_scan_ = false;

    if (!session) {
      bt_log(DEBUG, "bt-host", "failed to start discovery session");
      callback(fidl_helpers::NewFidlError(ErrorCode::FAILED, "Failed to start discovery session"));
      return;
    }

    // Assign the filter contents if a filter was provided.
    if (filter)
      fidl_helpers::PopulateDiscoveryFilter(*filter, session->filter());

    session->SetResultCallback([self](const auto& peer) {
      if (self)
        self->OnScanResult(peer);
    });

    session->set_error_callback([self] {
      if (self) {
        // Clean up the session and notify the delegate.
        self->StopScan();
      }
    });

    self->scan_session_ = std::move(session);
    self->NotifyScanStateChanged(true);
    callback(Status());
  });
}

void LowEnergyCentralServer::StopScan() {
  bt_log(DEBUG, "bt-host", "StopScan()");

  if (!scan_session_) {
    bt_log(DEBUG, "bt-host", "no active discovery session; nothing to do");
    return;
  }

  scan_session_ = nullptr;
  NotifyScanStateChanged(false);
}

void LowEnergyCentralServer::ConnectPeripheral(
    ::std::string identifier, fuchsia::bluetooth::le::ConnectionOptions connection_options,
    ::fidl::InterfaceRequest<Client> client_request, ConnectPeripheralCallback callback) {
  bt_log(INFO, "fidl", "%s(): (peer: %s)", __FUNCTION__, identifier.c_str());

  auto peer_id = fidl_helpers::PeerIdFromString(identifier);
  if (!peer_id.has_value()) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "invalid peer ID"));
    return;
  }

  auto iter = connections_.find(*peer_id);
  if (iter != connections_.end()) {
    if (iter->second) {
      bt_log(INFO, "fidl", "%s: already connected to %s", __FUNCTION__, bt_str(*peer_id));
      callback(
          fidl_helpers::NewFidlError(ErrorCode::ALREADY, "Already connected to requested peer"));
    } else {
      bt_log(INFO, "fidl", "%s: connect request pending (peer: %s)", __FUNCTION__,
             bt_str(*peer_id));
      callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS, "Connect request pending"));
    }
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto conn_cb = [self, callback = callback.share(), peer_id = *peer_id,
                  request = std::move(client_request)](auto result) mutable {
    if (!self)
      return;

    auto iter = self->connections_.find(peer_id);
    if (iter == self->connections_.end()) {
      bt_log(INFO, "fidl", "connect request canceled (peer: %s)", bt_str(peer_id));
      auto error = fidl_helpers::NewFidlError(ErrorCode::FAILED, "Connect request canceled");
      callback(std::move(error));
      return;
    }

    if (result.is_error()) {
      bt_log(INFO, "fidl", "failed to connect to peer (peer: %s)", bt_str(peer_id));
      self->connections_.erase(peer_id);
      callback(fidl_helpers::StatusToFidlDeprecated(bt::hci::Status(result.error()),
                                                    "failed to connect"));
      return;
    }

    auto conn_ref = result.take_value();
    ZX_ASSERT(conn_ref);
    ZX_ASSERT(peer_id == conn_ref->peer_identifier());

    if (self->gatt_client_servers_.find(peer_id) != self->gatt_client_servers_.end()) {
      bt_log(WARN, "fidl", "only 1 gatt.Client FIDL handle allowed per peer (%s)", bt_str(peer_id));
      // The handle owned by |request| will be closed.
      return;
    }

    auto server = std::make_unique<GattClientServer>(peer_id, self->gatt_, std::move(request));
    server->set_error_handler([self, peer_id](zx_status_t status) {
      if (self) {
        bt_log(DEBUG, "bt-host", "GATT client disconnected");
        self->gatt_client_servers_.erase(peer_id);
      }
    });
    self->gatt_client_servers_.emplace(peer_id, std::move(server));

    conn_ref->set_closed_callback([self, peer_id] {
      if (self && self->connections_.erase(peer_id) != 0) {
        bt_log(INFO, "fidl", "connection closed (peer: %s)", bt_str(peer_id));
        self->gatt_client_servers_.erase(peer_id);
        self->NotifyPeripheralDisconnected(peer_id);
      }
    });

    ZX_ASSERT(!iter->second);
    iter->second = std::move(conn_ref);
    callback(Status());
  };
  BondableMode bondable_mode =
      (!connection_options.has_bondable_mode() || connection_options.bondable_mode())
          ? BondableMode::Bondable
          : BondableMode::NonBondable;
  std::optional<bt::UUID> service_uuid =
      connection_options.has_service_filter()
          ? std::optional(fidl_helpers::UuidFromFidl(connection_options.service_filter()))
          : std::nullopt;
  bt::gap::LowEnergyConnectionOptions mgr_connection_options{.bondable_mode = bondable_mode,
                                                             .service_uuid = service_uuid};

  // An entry for the connection must be created here so that a synchronous call to conn_cb below
  // does not cause conn_cb to treat the connection as cancelled.
  connections_[*peer_id] = nullptr;

  adapter()->le()->Connect(*peer_id, std::move(conn_cb), mgr_connection_options);
}

void LowEnergyCentralServer::DisconnectPeripheral(::std::string identifier,
                                                  DisconnectPeripheralCallback callback) {
  auto peer_id = fidl_helpers::PeerIdFromString(identifier);
  if (!peer_id.has_value()) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "invalid peer ID"));
    return;
  }

  auto iter = connections_.find(*peer_id);
  if (iter == connections_.end()) {
    bt_log(INFO, "fidl", "DisconnectPeripheral: client not connected to peer (id: %s)",
           identifier.c_str());
    callback(Status());
    return;
  }

  // If a request to this peer is pending then the request will be canceled.
  bool was_pending = !iter->second;
  connections_.erase(iter);

  if (was_pending) {
    bt_log(INFO, "fidl", "canceling connection request (peer: %s)", bt_str(*peer_id));
  } else {
    gatt_client_servers_.erase(*peer_id);
    NotifyPeripheralDisconnected(*peer_id);
  }

  callback(Status());
}

void LowEnergyCentralServer::OnScanResult(const bt::gap::Peer& peer) {
  auto fidl_device = fidl_helpers::NewLERemoteDevice(peer);
  if (!fidl_device) {
    bt_log(DEBUG, "bt-host", "ignoring malformed scan result");
    return;
  }

  if (peer.rssi() != bt::hci::kRSSIInvalid) {
    fidl_device->rssi = Int8::New();
    fidl_device->rssi->value = peer.rssi();
  }

  binding()->events().OnDeviceDiscovered(std::move(*fidl_device));
}

void LowEnergyCentralServer::NotifyScanStateChanged(bool scanning) {
  binding()->events().OnScanStateChanged(scanning);
}

void LowEnergyCentralServer::NotifyPeripheralDisconnected(bt::PeerId peer_id) {
  binding()->events().OnPeripheralDisconnected(peer_id.ToString());
}

}  // namespace bthost
