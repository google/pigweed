// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_peripheral_server.h"

#include <lib/async/default.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/strings/string_number_conversions.h"

#define LOG_TAG "fidl"

using bt::sm::BondableMode;
using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;
namespace fble = fuchsia::bluetooth::le;

namespace bthost {
namespace {

fble::PeripheralError FidlErrorFromStatus(bt::hci::Status status) {
  switch (status.error()) {
    case bt::HostError::kNoError:
      ZX_PANIC("FidlErrorFromStatus called on success status");
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

}  // namespace

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
      bt_log(TRACE, LOG_TAG, "AdvertisingHandle closed");
      instance_.reset();
    }
  });

  zx_status_t status = handle_closed_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    bt_log(DEBUG, LOG_TAG, "failed to begin wait on AdvertisingHandle: %s",
           zx_status_get_string(status));
  }
  return status;
}

LowEnergyPeripheralServer::LowEnergyPeripheralServer(fxl::WeakPtr<bt::gap::Adapter> adapter,
                                                     fidl::InterfaceRequest<Peripheral> request)
    : AdapterServerBase(adapter, this, std::move(request)), weak_ptr_factory_(this) {}

LowEnergyPeripheralServer::~LowEnergyPeripheralServer() { ZX_ASSERT(adapter()->bredr()); }

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
    bt_log(DEBUG, LOG_TAG, "reconfigure existing advertising instance");
    advertisement_.reset();
  }

  bt::AdvertisingData adv_data, scan_rsp;
  bool include_tx_power_level = false;
  if (parameters.has_data()) {
    auto maybe_adv_data = fidl_helpers::AdvertisingDataFromFidl(parameters.data());
    if (!maybe_adv_data) {
      bt_log(WARN, LOG_TAG, "%s: invalid advertising data", __FUNCTION__);
      result.set_err(fble::PeripheralError::INVALID_PARAMETERS);
      callback(std::move(result));
      return;
    }
    adv_data = std::move(*maybe_adv_data);
    if (parameters.data().has_tx_power_level()) {
      bt_log(TRACE, LOG_TAG, "Including TX Power level at HCI layer");
      include_tx_power_level = true;
    }
  }
  if (parameters.has_scan_response()) {
    auto maybe_scan_rsp = fidl_helpers::AdvertisingDataFromFidl(parameters.scan_response());
    if (!maybe_scan_rsp) {
      bt_log(WARN, LOG_TAG, "%s: invalid scan response in advertising data", __FUNCTION__);
      result.set_err(fble::PeripheralError::INVALID_PARAMETERS);
      callback(std::move(result));
      return;
    }
    scan_rsp = std::move(*maybe_scan_rsp);
  }
  bt::gap::AdvertisingInterval interval = fidl_helpers::AdvertisingIntervalFromFidl(
      parameters.has_mode_hint() ? parameters.mode_hint() : fble::AdvertisingModeHint::SLOW);

  bool connectable_parameter = parameters.has_connectable() && parameters.connectable();

  // Create an entry to mark that the request is in progress.
  advertisement_.emplace(std::move(token));

  auto self = weak_ptr_factory_.GetWeakPtr();
  bt::gap::LowEnergyAdvertisingManager::ConnectionCallback connect_cb;
  // TODO(armansito): The conversion from hci::Connection to
  // gap::LowEnergyConnectionHandle should be performed by a gap library object
  // and not in this layer (see fxbug.dev/648).

  // Per the API contract of `AdvertisingParameters` FIDL, if `connection_options` is present or
  // the deprecated `connectable` parameter is true, advertisements will be connectable.
  // `connectable_parameter` was the predecessor of `connection_options` and
  // TODO(fxbug.dev/44749): will be removed once all consumers of it have migrated to
  // `connection_options`.
  if (connectable_parameter || parameters.has_connection_options()) {
    // Per the API contract of the `ConnectionOptions` FIDL, the bondable mode of the connection
    // defaults to bondable mode unless the `connection_options` table exists and `bondable_mode`
    // is explicitly set to false.
    BondableMode bondable_mode = (!parameters.has_connection_options() ||
                                  !parameters.connection_options().has_bondable_mode() ||
                                  parameters.connection_options().bondable_mode())
                                     ? BondableMode::Bondable
                                     : BondableMode::NonBondable;
    connect_cb = [self, bondable_mode](auto id, auto link) {
      if (self) {
        self->OnConnected(id, std::move(link), bondable_mode);
      }
    };
  }
  auto status_cb = [self, callback = std::move(callback), func = __FUNCTION__](
                       auto instance, bt::hci::Status status) {
    // Advertising will be stopped when |instance| gets destroyed.
    if (!self) {
      return;
    }

    ZX_ASSERT(self->advertisement_);
    ZX_ASSERT(self->advertisement_->id() == bt::gap::kInvalidAdvertisementId);

    fble::Peripheral_StartAdvertising_Result result;
    if (!status) {
      bt_log(WARN, LOG_TAG, "%s: failed to start advertising (status: %s)", func, bt_str(status));

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

  auto* am = adapter()->le();
  ZX_DEBUG_ASSERT(am);
  am->StartAdvertising(std::move(adv_data), std::move(scan_rsp), std::move(connect_cb), interval,
                       false /* anonymous */, include_tx_power_level, std::move(status_cb));
}

const bt::gap::LowEnergyConnectionHandle* LowEnergyPeripheralServer::FindConnectionForTesting(
    bt::PeerId id) const {
  auto connections_iter = connections_.find(id);
  if (connections_iter != connections_.end()) {
    return connections_iter->second->conn();
  }
  return nullptr;
}

void LowEnergyPeripheralServer::OnConnected(bt::gap::AdvertisementId advertisement_id,
                                            bt::hci::ConnectionPtr link,
                                            BondableMode bondable_mode) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  if (!advertisement_ || advertisement_->id() != advertisement_id) {
    bt_log(INFO, LOG_TAG, "%s: dropping connection from unrecognized advertisement ID: %s",
           __FUNCTION__, advertisement_id.ToString().c_str());
    return;
  }

  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  if (status != ZX_OK) {
    bt_log(ERROR, LOG_TAG, "%s: failed to create channel for Connection (status: %s)", __FUNCTION__,
           zx_status_get_string(status));
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_conn = [self, local = std::move(local), remote = std::move(remote),
                  link_str = link->ToString(), func = __FUNCTION__](auto result) mutable {
    if (!self) {
      return;
    }
    if (result.is_error()) {
      bt_log(INFO, LOG_TAG, "%s: incoming connection rejected (link: %s)", func, link_str.c_str());
      return;
    }

    auto conn = result.take_value();
    auto peer_id = conn->peer_identifier();
    auto conn_handle =
        std::make_unique<LowEnergyConnectionServer>(std::move(conn), std::move(local));
    conn_handle->set_closed_handler([self, peer_id, func] {
      bt_log(INFO, LOG_TAG, "%s closed handler: peer disconnected (peer: %s)", func,
             bt_str(peer_id));
      if (self) {
        // Removing the connection
        self->connections_.erase(peer_id);
      }
    });

    auto* peer = self->adapter()->peer_cache()->FindById(peer_id);
    ZX_ASSERT(peer);

    bt_log(INFO, LOG_TAG, "%s: central connected (peer: %s)", func, bt_str(peer->identifier()));
    auto fidl_peer = fidl_helpers::PeerToFidlLe(*peer);
    self->binding()->events().OnPeerConnected(
        std::move(fidl_peer), fidl::InterfaceHandle<fble::Connection>(std::move(remote)));

    // Close the AdvertisingHandle since advertising is stopped in response to a connection.
    self->advertisement_.reset();
    self->connections_[peer_id] = std::move(conn_handle);
  };

  adapter()->le()->RegisterRemoteInitiatedLink(std::move(link), bondable_mode, std::move(on_conn));
}

}  // namespace bthost
