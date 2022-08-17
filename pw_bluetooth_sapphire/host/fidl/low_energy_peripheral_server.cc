// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_peripheral_server.h"

#include <lib/async/default.h>
#include <zircon/status.h>

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_advertising_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/strings/string_number_conversions.h"

#define LOG_TAG "fidl"

using bt::sm::BondableMode;
using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;
namespace fble = fuchsia::bluetooth::le;

namespace bthost {
namespace {

fble::PeripheralError FidlErrorFromStatus(bt::hci::Result<> status) {
  BT_ASSERT_MSG(status.is_error(), "FidlErrorFromStatus called on success status");
  return status.error_value().Visit(
      [](bt::HostError host_error) {
        switch (host_error) {
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
      },
      [](auto /*hci_error*/) { return fble::PeripheralError::FAILED; });
}

}  // namespace

LowEnergyPeripheralServer::AdvertisementInstance::AdvertisementInstance(
    LowEnergyPeripheralServer* peripheral_server, AdvertisementInstanceId id,
    fuchsia::bluetooth::le::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> handle,
    AdvertiseCompleteCallback complete_cb)
    : peripheral_server_(peripheral_server),
      id_(id),
      parameters_(std::move(parameters)),
      advertise_complete_cb_(std::move(complete_cb)),
      weak_ptr_factory_(this) {
  BT_ASSERT(advertise_complete_cb_);
  advertised_peripheral_.Bind(std::move(handle));
  advertised_peripheral_.set_error_handler([this, peripheral_server, id](zx_status_t /*status*/) {
    CloseWith(fpromise::ok());
    peripheral_server->RemoveAdvertisingInstance(id);
  });
}

LowEnergyPeripheralServer::AdvertisementInstance::~AdvertisementInstance() {
  if (advertise_complete_cb_) {
    CloseWith(fpromise::error(fble::PeripheralError::ABORTED));
  }
}

void LowEnergyPeripheralServer::AdvertisementInstance::StartAdvertising() {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self](auto adv_instance, bt::hci::Result<> status) {
    if (!self) {
      bt_log(DEBUG, LOG_TAG, "advertisement canceled before advertising started");
      // Destroying `adv_instance` will stop advertising.
      return;
    }

    if (bt_is_error(status, WARN, LOG_TAG, "failed to start advertising (status: %s)",
                    bt_str(status))) {
      self->CloseWith(fpromise::error(FidlErrorFromStatus(status)));
      self->peripheral_server_->RemoveAdvertisingInstance(self->id_);
      return;
    }

    self->Register(std::move(adv_instance));
  };

  peripheral_server_->StartAdvertisingInternal(parameters_, std::move(status_cb), self->id_);
}

void LowEnergyPeripheralServer::AdvertisementInstance::Register(
    bt::gap::AdvertisementInstance instance) {
  BT_ASSERT(!instance_);
  instance_ = std::move(instance);
}

void LowEnergyPeripheralServer::AdvertisementInstance::OnConnected(
    bt::gap::AdvertisementId advertisement_id,
    bt::gap::Adapter::LowEnergy::ConnectionResult result) {
  BT_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  // HCI advertising ends when a connection is received (even for error results), so clear the
  // stale advertisement handle.
  instance_.reset();

  if (result.is_error()) {
    bt_log(INFO, LOG_TAG,
           "incoming connection failed; restarting advertising (adv instance id: %zu, prev adv "
           "id: %s)",
           id_, bt_str(advertisement_id));
    StartAdvertising();
    return;
  }

  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn = std::move(result).value();
  bt::PeerId peer_id = conn->peer_identifier();
  bt::gap::Peer* peer = peripheral_server_->adapter()->peer_cache()->FindById(peer_id);
  BT_ASSERT(peer);

  bt_log(INFO, LOG_TAG,
         "peripheral received connection to advertisement (peer: %s, adv id: %s, adv "
         "instance id: %zu)",
         bt_str(peer->identifier()), bt_str(advertisement_id), id_);

  fidl::InterfaceHandle<fble::Connection> conn_handle =
      peripheral_server_->CreateConnectionServer(std::move(conn));

  // Restart advertising after the client acknowledges the connection.
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_connected_cb = [self] {
    if (self) {
      self->StartAdvertising();
    }
  };
  advertised_peripheral_->OnConnected(fidl_helpers::PeerToFidlLe(*peer), std::move(conn_handle),
                                      std::move(on_connected_cb));
}

void LowEnergyPeripheralServer::AdvertisementInstance::CloseWith(
    fpromise::result<void, fuchsia::bluetooth::le::PeripheralError> result) {
  if (advertise_complete_cb_) {
    advertised_peripheral_.Unbind();
    advertise_complete_cb_(std::move(result));
  }
}

LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::AdvertisementInstanceDeprecated(
    fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle> handle)
    : handle_(std::move(handle)) {
  BT_DEBUG_ASSERT(handle_);
}

LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::~AdvertisementInstanceDeprecated() {
  handle_closed_wait_.Cancel();
}

zx_status_t LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::Register(
    bt::gap::AdvertisementInstance instance) {
  BT_DEBUG_ASSERT(!instance_);

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
                                                     fxl::WeakPtr<bt::gatt::GATT> gatt,
                                                     fidl::InterfaceRequest<Peripheral> request)
    : AdapterServerBase(std::move(adapter), this, std::move(request)),
      gatt_(std::move(gatt)),
      weak_ptr_factory_(this) {}

LowEnergyPeripheralServer::~LowEnergyPeripheralServer() { BT_ASSERT(adapter()->bredr()); }

void LowEnergyPeripheralServer::Advertise(
    fble::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> advertised_peripheral,
    AdvertiseCallback callback) {
  // Advertise and StartAdvertising may not be used simultaneously.
  if (advertisement_deprecated_.has_value()) {
    callback(fpromise::error(fble::PeripheralError::FAILED));
    return;
  }

  // TODO(fxbug.dev/76557): As a temporary hack until multiple advertisements is supported, don't
  // allow more than one advertisement. The current behavior of hci::LegacyLowEnergyAdvertiser
  // is to replace the current advertisement, which is not the intended behavior of `Advertise`.
  // NOTE: This is insufficient  when there are multiple Peripheral clients advertising, but that is
  // the status quo with `StartAdvertising` anyway (the last advertiser wins).
  if (!advertisements_.empty()) {
    callback(fpromise::error(fble::PeripheralError::FAILED));
    return;
  }

  AdvertisementInstanceId instance_id = next_advertisement_instance_id_++;
  auto [iter, inserted] =
      advertisements_.try_emplace(instance_id, this, instance_id, std::move(parameters),
                                  std::move(advertised_peripheral), std::move(callback));
  BT_ASSERT(inserted);
  iter->second.StartAdvertising();
}

void LowEnergyPeripheralServer::StartAdvertising(
    fble::AdvertisingParameters parameters, ::fidl::InterfaceRequest<fble::AdvertisingHandle> token,
    StartAdvertisingCallback callback) {
  fble::Peripheral_StartAdvertising_Result result;

  // Advertise and StartAdvertising may not be used simultaneously.
  if (!advertisements_.empty()) {
    result.set_err(fble::PeripheralError::INVALID_PARAMETERS);
    callback(std::move(result));
    return;
  }

  if (!token) {
    result.set_err(fble::PeripheralError::INVALID_PARAMETERS);
    callback(std::move(result));
    return;
  }

  if (advertisement_deprecated_) {
    bt_log(DEBUG, LOG_TAG, "reconfigure existing advertising instance");
    advertisement_deprecated_.reset();
  }

  // Create an entry to mark that the request is in progress.
  advertisement_deprecated_.emplace(std::move(token));

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self, callback = std::move(callback), func = __FUNCTION__](
                       auto instance, bt::hci::Result<> status) {
    // Advertising will be stopped when |instance| gets destroyed.
    if (!self) {
      return;
    }

    BT_ASSERT(self->advertisement_deprecated_);
    BT_ASSERT(self->advertisement_deprecated_->id() == bt::gap::kInvalidAdvertisementId);

    fble::Peripheral_StartAdvertising_Result result;
    if (status.is_error()) {
      bt_log(WARN, LOG_TAG, "%s: failed to start advertising (status: %s)", func, bt_str(status));

      result.set_err(FidlErrorFromStatus(status));

      // The only scenario in which it is valid to leave |advertisement_| intact in a failure
      // scenario is if StartAdvertising was called while a previous call was in progress. This
      // aborts the prior request causing it to end with the "kCanceled" status. This means that
      // another request is currently progress.
      if (!status.error_value().is(bt::HostError::kCanceled)) {
        self->advertisement_deprecated_.reset();
      }

      callback(std::move(result));
      return;
    }

    zx_status_t ecode = self->advertisement_deprecated_->Register(std::move(instance));
    if (ecode != ZX_OK) {
      result.set_err(fble::PeripheralError::FAILED);
      self->advertisement_deprecated_.reset();
      callback(std::move(result));
      return;
    }

    result.set_response({});
    callback(std::move(result));
  };

  StartAdvertisingInternal(parameters, std::move(status_cb));
}

const bt::gap::LowEnergyConnectionHandle* LowEnergyPeripheralServer::FindConnectionForTesting(
    bt::PeerId id) const {
  auto connections_iter =
      std::find_if(connections_.begin(), connections_.end(),
                   [id](const auto& conn) { return conn.second->conn()->peer_identifier() == id; });
  if (connections_iter != connections_.end()) {
    return connections_iter->second->conn();
  }
  return nullptr;
}

void LowEnergyPeripheralServer::OnConnectedDeprecated(
    bt::gap::AdvertisementId advertisement_id,
    bt::gap::Adapter::LowEnergy::ConnectionResult result) {
  BT_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  // Abort connection procedure if advertisement was canceled by the client.
  if (!advertisement_deprecated_ || advertisement_deprecated_->id() != advertisement_id) {
    bt_log(INFO, LOG_TAG, "dropping connection to canceled advertisement (advertisement id: %s)",
           bt_str(advertisement_id));
    return;
  }

  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  if (status != ZX_OK) {
    bt_log(ERROR, LOG_TAG, "failed to create channel for Connection (status: %s)",
           zx_status_get_string(status));
    return;
  }

  if (result.is_error()) {
    bt_log(INFO, LOG_TAG, "incoming connection to advertisement failed (advertisement id: %s)",
           bt_str(advertisement_id));
    return;
  }

  auto conn = std::move(result).value();
  auto peer_id = conn->peer_identifier();
  auto* peer = adapter()->peer_cache()->FindById(peer_id);
  BT_ASSERT(peer);

  bt_log(INFO, LOG_TAG, "central connected (peer: %s, advertisement id: %s)",
         bt_str(peer->identifier()), bt_str(advertisement_id));

  fidl::InterfaceHandle<fble::Connection> conn_handle = CreateConnectionServer(std::move(conn));

  binding()->events().OnPeerConnected(fidl_helpers::PeerToFidlLe(*peer), std::move(conn_handle));
  advertisement_deprecated_.reset();
}

fidl::InterfaceHandle<fuchsia::bluetooth::le::Connection>
LowEnergyPeripheralServer::CreateConnectionServer(
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection) {
  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  BT_ASSERT(status == ZX_OK);

  auto conn_server_id = next_connection_server_id_++;
  auto conn_server = std::make_unique<LowEnergyConnectionServer>(
      gatt_, std::move(connection), std::move(local), [this, conn_server_id] {
        bt_log(INFO, LOG_TAG, "connection closed");
        connections_.erase(conn_server_id);
      });
  connections_[conn_server_id] = std::move(conn_server);

  return fidl::InterfaceHandle<fble::Connection>(std::move(remote));
}

void LowEnergyPeripheralServer::StartAdvertisingInternal(
    fuchsia::bluetooth::le::AdvertisingParameters& parameters,
    bt::gap::Adapter::LowEnergy::AdvertisingStatusCallback status_cb,
    std::optional<AdvertisementInstanceId> advertisement_instance) {
  bt::AdvertisingData adv_data, scan_rsp;
  bool include_tx_power_level = false;
  if (parameters.has_data()) {
    auto maybe_adv_data = fidl_helpers::AdvertisingDataFromFidl(parameters.data());
    if (!maybe_adv_data) {
      bt_log(WARN, LOG_TAG, "invalid advertising data");
      status_cb({}, ToResult(bt::HostError::kInvalidParameters));
      return;
    }
    adv_data = std::move(*maybe_adv_data);
    if (parameters.data().has_include_tx_power_level() &&
        parameters.data().include_tx_power_level()) {
      bt_log(TRACE, LOG_TAG, "Including TX Power level in advertising data at HCI layer");
      include_tx_power_level = true;
    }
  }
  if (parameters.has_scan_response()) {
    auto maybe_scan_rsp = fidl_helpers::AdvertisingDataFromFidl(parameters.scan_response());
    if (!maybe_scan_rsp) {
      bt_log(WARN, LOG_TAG, "invalid scan response in advertising data");
      status_cb({}, ToResult(bt::HostError::kInvalidParameters));
      return;
    }
    scan_rsp = std::move(*maybe_scan_rsp);
  }
  bt::gap::AdvertisingInterval interval = fidl_helpers::AdvertisingIntervalFromFidl(
      parameters.has_mode_hint() ? parameters.mode_hint() : fble::AdvertisingModeHint::SLOW);

  std::optional<bt::gap::Adapter::LowEnergy::ConnectableAdvertisingParameters> connectable_params;

  // Per the API contract of `AdvertisingParameters` FIDL, if `connection_options` is present or
  // the deprecated `connectable` parameter is true, advertisements will be connectable.
  // `connectable_parameter` was the predecessor of `connection_options` and
  // TODO(fxbug.dev/44749): will be removed once all consumers of it have migrated to
  // `connection_options`.
  bool connectable = parameters.has_connection_options() ||
                     (parameters.has_connectable() && parameters.connectable());
  if (connectable) {
    connectable_params.emplace();

    auto self = weak_ptr_factory_.GetWeakPtr();
    connectable_params->connection_cb = [self, advertisement_instance](
                                            bt::gap::AdvertisementId advertisement_id,
                                            bt::gap::Adapter::LowEnergy::ConnectionResult result) {
      if (!self) {
        return;
      }

      // Handle connection for deprecated StartAdvertising method.
      if (!advertisement_instance) {
        self->OnConnectedDeprecated(advertisement_id, std::move(result));
        return;
      }

      auto advertisement_iter = self->advertisements_.find(*advertisement_instance);
      if (advertisement_iter == self->advertisements_.end()) {
        if (result.is_ok()) {
          bt_log(DEBUG, LOG_TAG,
                 "releasing connection handle for canceled advertisement (peer: %s)",
                 bt_str(result.value()->peer_identifier()));
          result.value()->Release();
        }
        return;
      }
      advertisement_iter->second.OnConnected(advertisement_id, std::move(result));
    };

    // Per the API contract of the `ConnectionOptions` FIDL, the bondable mode of the connection
    // defaults to bondable mode unless the `connection_options` table exists and
    // `bondable_mode` is explicitly set to false.
    connectable_params->bondable_mode = (!parameters.has_connection_options() ||
                                         !parameters.connection_options().has_bondable_mode() ||
                                         parameters.connection_options().bondable_mode())
                                            ? BondableMode::Bondable
                                            : BondableMode::NonBondable;
  }

  BT_ASSERT(adapter()->le());
  adapter()->le()->StartAdvertising(std::move(adv_data), std::move(scan_rsp), interval,
                                    /*anonymous=*/false, include_tx_power_level,
                                    std::move(connectable_params), std::move(status_cb));
}

}  // namespace bthost
