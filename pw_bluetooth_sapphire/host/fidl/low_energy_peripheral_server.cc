// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_peripheral_server.h"

#include <zircon/assert.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/lib/fxl/strings/string_number_conversions.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

using fuchsia::bluetooth::le::AdvertisingData;
using fuchsia::bluetooth::le::AdvertisingDataDeprecated;
using fuchsia::bluetooth::le::AdvertisingDataDeprecatedPtr;
using fuchsia::bluetooth::le::AdvertisingDataPtr;
using fuchsia::bluetooth::le::Peripheral;
using fuchsia::bluetooth::le::RemoteDevice;
using fuchsia::bluetooth::le::RemoteDevicePtr;

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

// TODO(BT-812): Remove this once the string IDs have been removed from the FIDL
// API.
std::optional<bt::gap::AdvertisementId> AdvertisementIdFromString(const std::string& id) {
  uint64_t value;
  if (!fxl::StringToNumberWithError<decltype(value)>(id, &value, fxl::Base::k16)) {
    return std::nullopt;
  }
  return bt::gap::AdvertisementId(value);
}

// TODO(BT-812): Remove once all clients refer to the deprecated type.
AdvertisingDataDeprecated ConvertToDeprecated(AdvertisingData data) {
  AdvertisingDataDeprecated depr;
  depr.name = std::move(data.name);
  depr.tx_power_level = std::move(data.tx_power_level);
  depr.appearance = std::move(data.appearance);
  depr.service_uuids = std::move(data.service_uuids);
  depr.service_data = std::move(data.service_data);
  depr.manufacturer_specific_data = std::move(data.manufacturer_specific_data);
  depr.solicited_service_uuids = std::move(data.solicited_service_uuids);
  depr.uris = std::move(data.uris);
  return depr;
}

}  // namespace

LowEnergyPeripheralServer::InstanceData::InstanceData(bt::gap::AdvertisementId id,
                                                      fxl::WeakPtr<LowEnergyPeripheralServer> owner)
    : id_(id), owner_(owner) {
  ZX_DEBUG_ASSERT(owner_);
}

void LowEnergyPeripheralServer::InstanceData::RetainConnection(ConnectionRefPtr conn_ref,
                                                               RemoteDevice peer) {
  ZX_DEBUG_ASSERT(connectable());
  ZX_DEBUG_ASSERT(!conn_ref_);

  conn_ref_ = std::move(conn_ref);
  owner_->binding()->events().OnCentralConnected(id_.ToString(), std::move(peer));
}

void LowEnergyPeripheralServer::InstanceData::ReleaseConnection() {
  ZX_DEBUG_ASSERT(connectable());
  ZX_DEBUG_ASSERT(conn_ref_);

  owner_->binding()->events().OnCentralDisconnected(conn_ref_->peer_identifier().ToString());
  conn_ref_ = nullptr;
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

void LowEnergyPeripheralServer::StartAdvertising(AdvertisingData advertising_data,
                                                 AdvertisingDataPtr scan_result, bool connectable,
                                                 uint32_t interval, bool anonymous,
                                                 StartAdvertisingCallback callback) {
  AdvertisingDataDeprecated ad = ConvertToDeprecated(std::move(advertising_data));
  AdvertisingDataDeprecatedPtr scrsp;
  if (scan_result) {
    scrsp = AdvertisingDataDeprecated::New();
    *scrsp = ConvertToDeprecated(std::move(*scan_result));
  }
  StartAdvertisingDeprecated(std::move(ad), std::move(scrsp), connectable, interval, anonymous,
                             std::move(callback));
}

void LowEnergyPeripheralServer::StartAdvertisingDeprecated(
    AdvertisingDataDeprecated advertising_data, AdvertisingDataDeprecatedPtr scan_result,
    bool connectable, uint32_t interval, bool anonymous,
    StartAdvertisingDeprecatedCallback callback) {
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
        self->OnConnected(adv_id, std::move(link));
    };
  }
  auto advertising_status_cb = [self, callback = std::move(callback)](
                                   bt::gap::AdvertisementId ad_id, bt::hci::Status status) mutable {
    if (!self)
      return;

    if (!status) {
      bt_log(TRACE, "bt-host", "failed to start advertising: %s", status.ToString().c_str());
      callback(fidl_helpers::StatusToFidl(status, MessageFromStatus(status)), "");
      return;
    }

    self->instances_[ad_id] = InstanceData(ad_id, self->weak_ptr_factory_.GetWeakPtr());
    callback(Status(), ad_id.ToString());
  };

  advertising_manager->StartAdvertising(ad_data, scan_data, std::move(connect_cb),
                                        zx::msec(interval), anonymous,
                                        std::move(advertising_status_cb));
}

void LowEnergyPeripheralServer::StopAdvertising(::std::string id,
                                                StopAdvertisingCallback callback) {
  StopAdvertisingDeprecated(std::move(id), std::move(callback));
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

  // If the active adapter that was used to start advertising was changed before
  // we process this connection then the instance will have been removed.
  auto it = instances_.find(advertisement_id);
  if (it == instances_.end()) {
    bt_log(TRACE, "bt-host", "connection received from wrong advertising instance");
    return;
  }

  ZX_DEBUG_ASSERT(it->second.connectable());

  auto conn = adapter()->le_connection_manager()->RegisterRemoteInitiatedLink(std::move(link));
  if (!conn) {
    bt_log(TRACE, "bt-host", "incoming connection rejected");
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  conn->set_closed_callback([self, id = advertisement_id] {
    bt_log(TRACE, "bt-host", "central disconnected");

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

  bt_log(TRACE, "bt-host", "central connected");
  RemoteDevicePtr remote_device = fidl_helpers::NewLERemoteDevice(std::move(*peer));
  ZX_DEBUG_ASSERT(remote_device);
  it->second.RetainConnection(std::move(conn), std::move(*remote_device));
}

}  // namespace bthost
