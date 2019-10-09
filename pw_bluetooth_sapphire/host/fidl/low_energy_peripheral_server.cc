// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_peripheral_server.h"

#include <zircon/assert.h>
#include <zircon/status.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/lib/fxl/strings/string_number_conversions.h"

#define LOG_TAG "le.Peripheral"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

namespace fble = fuchsia::bluetooth::le;

namespace bthost {
namespace {

std::string MessageFromStatus(bt::hci::Status status) {
  switch (status.error()) {
    case bt::HostError::kNoError:
      return "Success";
    case bt::HostError::kNotSupported:
      return "Maximum advertisement amount reached";
    case bt::HostError::kInvalidParameters:
      return "Advertisement exceeds maximum allowed length";
    default:
      return status.ToString();
  }
}

fble::PeripheralError FidlErrorFromStatus(bt::hci::Status status) {
  switch (status.error()) {
    case bt::HostError::kNoError:
      ZX_ASSERT("FidlErrorFromStatus called on success status");
      break;
    case bt::HostError::kNotSupported:
      return fble::PeripheralError::NOT_SUPPORTED;
    case bt::HostError::kInvalidParameters:
      return fble::PeripheralError::INVALID_PARAMETERS;
    case bt::HostError::kAdvertisingDataTooLong:
      return fble::PeripheralError::ADVERTISING_DATA_TOO_LONG;
    case bt::HostError::kScanResponseTooLong:
      return fble::PeripheralError::SCAN_RESPONSE_DATA_TOO_LONG;
    case bt::HostError::kCanceled:
      return fble::PeripheralError::ABORTED;
    default:
      break;
  }
  return fble::PeripheralError::FAILED;
}

// TODO(BT-812): Remove this once the string IDs have been removed from the FIDL
// API.
std::optional<bt::gap::AdvertisementId> AdvertisementIdFromString(const std::string& id) {
  uint64_t value;
  if (!fxl::StringToNumberWithError<decltype(value)>(id, &value, fxl::Base::k16)) {
    return std::nullopt;
  }
  return bt::gap::AdvertisementId(value);
}

}  // namespace

LowEnergyPeripheralServer::InstanceData::InstanceData(bt::gap::AdvertisementInstance instance,
                                                      fxl::WeakPtr<LowEnergyPeripheralServer> owner)
    : instance_(std::move(instance)), owner_(owner) {
  ZX_DEBUG_ASSERT(owner_);
}

void LowEnergyPeripheralServer::InstanceData::RetainConnection(ConnectionRefPtr conn_ref,
                                                               fble::RemoteDevice peer) {
  ZX_DEBUG_ASSERT(connectable());
  ZX_DEBUG_ASSERT(!conn_ref_);

  conn_ref_ = std::move(conn_ref);
  owner_->binding()->events().OnCentralConnected(instance_.id().ToString(), std::move(peer));
}

void LowEnergyPeripheralServer::InstanceData::ReleaseConnection() {
  ZX_DEBUG_ASSERT(connectable());
  ZX_DEBUG_ASSERT(conn_ref_);

  owner_->binding()->events().OnCentralDisconnected(conn_ref_->peer_identifier().ToString());
  conn_ref_ = nullptr;
}

LowEnergyPeripheralServer::AdvertisementInstance::AdvertisementInstance(
    fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> handle)
    : handle_(std::move(handle)) {
  ZX_DEBUG_ASSERT(handle_);
}

LowEnergyPeripheralServer::AdvertisementInstance::~AdvertisementInstance() {
  handle_closed_wait_.Cancel();
}

zx_status_t LowEnergyPeripheralServer::AdvertisementInstance::Register(
    bt::gap::AdvertisementInstance instance) {
  ZX_DEBUG_ASSERT(!instance_);

  instance_ = std::move(instance);

  handle_closed_wait_.set_object(handle_.channel().get());
  handle_closed_wait_.set_trigger(ZX_CHANNEL_PEER_CLOSED);
  handle_closed_wait_.set_handler([this](auto*, auto*, zx_status_t status, const auto*) {
    // Don't do anything if the wait was explicitly canceled by us.
    if (status != ZX_ERR_CANCELED) {
      bt_log(SPEW, LOG_TAG, "AdvertisingHandle closed");
      instance_.reset();
    }
  });

  zx_status_t status = handle_closed_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    bt_log(TRACE, LOG_TAG, "failed to begin wait on AdvertisingHandle: %s",
           zx_status_get_string(status));
  }
  return status;
}

LowEnergyPeripheralServer::LowEnergyPeripheralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                                                     fidl::InterfaceRequest<Peripheral> request)
    : AdapterServerBase(adapter, this, std::move(request)), weak_ptr_factory_(this) {}

LowEnergyPeripheralServer::~LowEnergyPeripheralServer() {
  auto* advertising_manager = adapter()->le_advertising_manager();
  ZX_DEBUG_ASSERT(advertising_manager);

  for (const auto& it : instances_) {
    advertising_manager->StopAdvertising(it.first);
  }
}

void LowEnergyPeripheralServer::StartAdvertising(
    fble::AdvertisingParameters parameters, ::fidl::InterfaceRequest<fble::AdvertisingHandle> token,
    StartAdvertisingCallback callback) {
  fble::Peripheral_StartAdvertising_Result result;

  if (!token) {
    result.set_err(fble::PeripheralError::INVALID_PARAMETERS);
    callback(std::move(result));
    return;
  }

  if (advertisement_) {
    bt_log(TRACE, LOG_TAG, "reconfigure existing advertising instance");
    advertisement_.reset();
  }

  bt::gap::AdvertisingData adv_data, scan_rsp;
  if (parameters.has_data()) {
    adv_data = fidl_helpers::AdvertisingDataFromFidl(parameters.data());
  }
  if (parameters.has_scan_response()) {
    scan_rsp = fidl_helpers::AdvertisingDataFromFidl(parameters.scan_response());
  }
  bt::gap::AdvertisingInterval interval = fidl_helpers::AdvertisingIntervalFromFidl(
      parameters.has_mode_hint() ? parameters.mode_hint() : fble::AdvertisingModeHint::SLOW);
  bool connectable = parameters.has_connectable() && parameters.connectable();

  // Create an entry to mark that the request is in progress.
  advertisement_.emplace(std::move(token));

  auto self = weak_ptr_factory_.GetWeakPtr();
  bt::gap::LowEnergyAdvertisingManager::ConnectionCallback connect_cb;
  // TODO(armansito): The conversion from hci::Connection to
  // gap::LowEnergyConnectionRef should be performed by a gap library object
  // and not in this layer (see NET-355).
  if (connectable) {
    connect_cb = [self](auto id, auto link) {
      if (self) {
        self->OnConnected(id, std::move(link));
      }
    };
  }
  auto status_cb = [self, callback = std::move(callback)](auto instance, bt::hci::Status status) {
    // Advertising will be stopped when |instance| gets destroyed.
    if (!self) {
      return;
    }

    ZX_ASSERT(self->advertisement_);
    ZX_ASSERT(self->advertisement_->id() == bt::gap::kInvalidAdvertisementId);

    fble::Peripheral_StartAdvertising_Result result;
    if (!status) {
      result.set_err(FidlErrorFromStatus(status));

      // The only scenario in which it is valid to leave |advertisement_| intact in a failure
      // scenario is if StartAdvertising was called while a previous call was in progress. This
      // aborts the prior request causing it to end with the "kCanceled" status. This means that
      // another request is currently progress.
      if (status.error() != bt::HostError::kCanceled) {
        self->advertisement_.reset();
      }

      callback(std::move(result));
      return;
    }

    zx_status_t ecode = self->advertisement_->Register(std::move(instance));
    if (ecode != ZX_OK) {
      result.set_err(fble::PeripheralError::FAILED);
      self->advertisement_.reset();
      callback(std::move(result));
      return;
    }

    result.set_response({});
    callback(std::move(result));
  };

  auto* am = adapter()->le_advertising_manager();
  ZX_DEBUG_ASSERT(am);
  am->StartAdvertising(adv_data, scan_rsp, std::move(connect_cb), interval, false /* anonymous */,
                       std::move(status_cb));
}

void LowEnergyPeripheralServer::StartAdvertisingDeprecated(
    fble::AdvertisingDataDeprecated advertising_data,
    fble::AdvertisingDataDeprecatedPtr scan_result, bool connectable, uint32_t interval,
    bool anonymous, StartAdvertisingDeprecatedCallback callback) {
  auto* advertising_manager = adapter()->le_advertising_manager();
  ZX_DEBUG_ASSERT(advertising_manager);

  bt::gap::AdvertisingData ad_data, scan_data;
  if (!bt::gap::AdvertisingData::FromFidl(advertising_data, &ad_data)) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "Invalid advertising data"),
             "");
    return;
  }

  if (scan_result && !bt::gap::AdvertisingData::FromFidl(*scan_result, &scan_data)) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "Invalid scan response data"),
             "");
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();

  bt::gap::LowEnergyAdvertisingManager::ConnectionCallback connect_cb;
  // TODO(armansito): The conversion from hci::Connection to
  // gap::LowEnergyConnectionRef should be performed by a gap library object
  // and not in this layer (see NET-355).
  if (connectable) {
    connect_cb = [self](auto adv_id, auto link) {
      if (self)
        self->OnConnectedDeprecated(adv_id, std::move(link));
    };
  }
  auto advertising_status_cb = [self, callback = std::move(callback)](
                                   bt::gap::AdvertisementInstance instance,
                                   bt::hci::Status status) mutable {
    if (!self)
      return;

    if (!status) {
      bt_log(TRACE, LOG_TAG, "failed to start advertising: %s", status.ToString().c_str());
      callback(fidl_helpers::StatusToFidl(status, MessageFromStatus(status)), "");
      return;
    }

    auto id = instance.id();
    self->instances_[id] = InstanceData(std::move(instance), self->weak_ptr_factory_.GetWeakPtr());
    callback(Status(), id.ToString());
  };

  // TODO(BT-812): Ignore the input |interval| value and default to FAST1 for now to make current
  // users happy.
  advertising_manager->StartAdvertising(ad_data, scan_data, std::move(connect_cb),
                                        bt::gap::AdvertisingInterval::FAST1, anonymous,
                                        std::move(advertising_status_cb));
}

void LowEnergyPeripheralServer::StopAdvertisingDeprecated(
    ::std::string id, StopAdvertisingDeprecatedCallback callback) {
  auto peer_id = AdvertisementIdFromString(id);
  if (!peer_id.has_value()) {
    callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS, "invalid peer ID"));
    return;
  }

  if (StopAdvertisingInternal(*peer_id)) {
    callback(Status());
  } else {
    callback(fidl_helpers::NewFidlError(ErrorCode::NOT_FOUND, "Unrecognized advertisement ID"));
  }
}

bool LowEnergyPeripheralServer::StopAdvertisingInternal(bt::gap::AdvertisementId id) {
  auto count = instances_.erase(id);
  if (count) {
    adapter()->le_advertising_manager()->StopAdvertising(id);
  }

  return count != 0;
}

void LowEnergyPeripheralServer::OnConnected(bt::gap::AdvertisementId advertisement_id,
                                            bt::hci::ConnectionPtr link) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  if (!advertisement_ || advertisement_->id() != advertisement_id) {
    bt_log(TRACE, LOG_TAG, "dropping connection from unrecognized advertisement ID: %s",
           advertisement_id.ToString().c_str());
    return;
  }

  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  if (status != ZX_OK) {
    bt_log(TRACE, LOG_TAG, "failed to create channel for Connection (%s)",
           zx_status_get_string(status));
    return;
  }

  auto conn = adapter()->le_connection_manager()->RegisterRemoteInitiatedLink(std::move(link));
  if (!conn) {
    bt_log(TRACE, LOG_TAG, "incoming connection rejected");
    return;
  }

  auto peer_id = conn->peer_identifier();
  auto conn_handle = std::make_unique<LowEnergyConnectionServer>(std::move(conn), std::move(local));
  auto self = weak_ptr_factory_.GetWeakPtr();
  conn_handle->set_closed_handler([self, peer_id] {
    bt_log(TRACE, LOG_TAG, "peer disconnected");
    if (self) {
      // Removing the connection
      self->connections_.erase(peer_id);
    }
  });

  auto* peer = adapter()->peer_cache()->FindById(peer_id);
  ZX_ASSERT(peer);

  bt_log(TRACE, LOG_TAG, "central connected");
  auto fidl_peer = fidl_helpers::PeerToFidlLe(*peer);
  binding()->events().OnPeerConnected(std::move(fidl_peer),
                                      fidl::InterfaceHandle<fble::Connection>(std::move(remote)));

  // Close the AdvertisingHandle since advertising is stopped in response to a connection.
  advertisement_.reset();
  connections_[peer_id] = std::move(conn_handle);
}

void LowEnergyPeripheralServer::OnConnectedDeprecated(bt::gap::AdvertisementId advertisement_id,
                                                      bt::hci::ConnectionPtr link) {
  ZX_DEBUG_ASSERT(link);

  // If the active adapter that was used to start advertising was changed before
  // we process this connection then the instance will have been removed.
  auto it = instances_.find(advertisement_id);
  if (it == instances_.end()) {
    bt_log(TRACE, LOG_TAG, "connection received from wrong advertising instance");
    return;
  }

  ZX_DEBUG_ASSERT(it->second.connectable());

  auto conn = adapter()->le_connection_manager()->RegisterRemoteInitiatedLink(std::move(link));
  if (!conn) {
    bt_log(TRACE, LOG_TAG, "incoming connection rejected");
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  conn->set_closed_callback([self, id = advertisement_id] {
    bt_log(TRACE, LOG_TAG, "central disconnected");

    if (!self)
      return;

    // Make sure that the instance hasn't been removed.
    auto it = self->instances_.find(id);
    if (it == self->instances_.end())
      return;

    // This sends OnCentralDisconnected() to the delegate.
    it->second.ReleaseConnection();
  });

  // A peer will have been created for the new connection.
  auto* peer = adapter()->peer_cache()->FindById(conn->peer_identifier());
  ZX_DEBUG_ASSERT(peer);

  bt_log(TRACE, LOG_TAG, "central connected");
  fble::RemoteDevicePtr remote_device = fidl_helpers::NewLERemoteDevice(std::move(*peer));
  ZX_DEBUG_ASSERT(remote_device);
  it->second.RetainConnection(std::move(conn), std::move(*remote_device));
}

}  // namespace bthost
