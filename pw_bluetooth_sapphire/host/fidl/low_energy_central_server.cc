// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_central_server.h"

#include <zircon/assert.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Int8;
using fuchsia::bluetooth::Status;

using fuchsia::bluetooth::gatt::Client;
using fuchsia::bluetooth::le::ScanFilterPtr;

namespace bthost {

LowEnergyCentralServer::LowEnergyCentralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                                               fidl::InterfaceRequest<Central> request,
                                               fbl::RefPtr<GattHost> gatt_host)
    : AdapterServerBase(adapter, this, std::move(request)),
      gatt_host_(gatt_host),
      requesting_scan_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(gatt_host_);
}

LowEnergyCentralServer::~LowEnergyCentralServer() {
  gatt_host_->UnbindGattClient(reinterpret_cast<GattHost::Token>(this));
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
  bt_log(TRACE, "bt-host", "StartScan()");

  if (requesting_scan_) {
    bt_log(TRACE, "bt-host", "scan request already in progress");
    callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS, "Scan request in progress"));
    return;
  }

  if (filter && !fidl_helpers::IsScanFilterValid(*filter)) {
    bt_log(TRACE, "bt-host", "invalid scan filter given");
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
  adapter()->le_discovery_manager()->StartDiscovery([self = weak_ptr_factory_.GetWeakPtr(),
                                                     filter = std::move(filter),
                                                     callback = std::move(callback)](auto session) {
    if (!self)
      return;

    self->requesting_scan_ = false;

    if (!session) {
      bt_log(TRACE, "bt-host", "failed to start discovery session");
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
  bt_log(TRACE, "bt-host", "StopScan()");

  if (!scan_session_) {
    bt_log(TRACE, "bt-host", "no active discovery session; nothing to do");
    return;
  }

  scan_session_ = nullptr;
  NotifyScanStateChanged(false);
}

void LowEnergyCentralServer::ConnectPeripheral(::std::string identifier,
                                               ::fidl::InterfaceRequest<Client> client_request,
                                               ConnectPeripheralCallback callback) {
  bt_log(TRACE, "bt-host", "ConnectPeripheral()");

  auto peer_id = fidl_helpers::PeerIdFromString(identifier);
  if (!peer_id.has_value()) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "invalid peer ID"));
    return;
  }

  auto iter = connections_.find(*peer_id);
  if (iter != connections_.end()) {
    if (iter->second) {
      callback(
          fidl_helpers::NewFidlError(ErrorCode::ALREADY, "Already connected to requested peer"));
    } else {
      callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS, "Connect request pending"));
    }
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto conn_cb = [self, callback = callback.share(), peer_id = *peer_id,
                  request = std::move(client_request)](auto status, auto conn_ref) mutable {
    if (!self)
      return;

    auto iter = self->connections_.find(peer_id);
    if (iter == self->connections_.end()) {
      bt_log(TRACE, "bt-host", "connect request canceled");
      auto error = fidl_helpers::NewFidlError(ErrorCode::FAILED, "Connect request canceled");
      callback(std::move(error));
      return;
    }

    if (!status) {
      ZX_DEBUG_ASSERT(!conn_ref);
      bt_log(TRACE, "bt-host", "failed to connect to connect to peer (id %s)", bt_str(peer_id));
      callback(fidl_helpers::StatusToFidl(status, "failed to connect"));
      return;
    }

    ZX_DEBUG_ASSERT(conn_ref);
    ZX_DEBUG_ASSERT(peer_id == conn_ref->peer_identifier());

    if (iter->second) {
      // This can happen if a connect is requested after a previous request was
      // canceled (e.g. if ConnectPeripheral, DisconnectPeripheral,
      // ConnectPeripheral are called in quick succession). In this case we
      // don't claim |conn_ref| since we already have a reference for this
      // peripheral.
      bt_log(SPEW, "bt-host",
             "dropping extra connection ref due to previously canceled "
             "connection attempt");
    }

    uintptr_t token = reinterpret_cast<GattHost::Token>(self.get());
    self->gatt_host_->BindGattClient(token, peer_id, std::move(request));

    conn_ref->set_closed_callback([self, token, peer_id] {
      if (self && self->connections_.erase(peer_id) != 0) {
        self->gatt_host_->UnbindGattClient(token);
        self->NotifyPeripheralDisconnected(peer_id);
      }
    });

    iter->second = std::move(conn_ref);
    callback(Status());
  };

  if (!adapter()->le_connection_manager()->Connect(*peer_id, std::move(conn_cb))) {
    bt_log(TRACE, "bt-host", "cannot connect to unknown peer (id: %s)", identifier.c_str());
    callback(fidl_helpers::NewFidlError(ErrorCode::NOT_FOUND, "unknown peer ID"));
    return;
  }

  connections_[*peer_id] = nullptr;
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
    bt_log(TRACE, "bt-host", "client not connected to peer (id: %s)", identifier.c_str());
    callback(fidl_helpers::NewFidlError(ErrorCode::NOT_FOUND, "peer not connected"));
    return;
  }

  // If a request to this peer is pending then the request will be canceled.
  bool was_pending = !iter->second;
  connections_.erase(iter);

  if (was_pending) {
    bt_log(TRACE, "bt-host", "canceling connection request");
  } else {
    gatt_host_->UnbindGattClient(reinterpret_cast<uintptr_t>(this));
    NotifyPeripheralDisconnected(*peer_id);
  }

  callback(Status());
}

void LowEnergyCentralServer::OnScanResult(const bt::gap::Peer& peer) {
  auto fidl_device = fidl_helpers::NewLERemoteDevice(peer);
  if (!fidl_device) {
    bt_log(TRACE, "bt-host", "ignoring malformed scan result");
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
