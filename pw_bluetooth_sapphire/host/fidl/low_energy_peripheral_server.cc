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
    LowEnergyPeripheralServer* peripheral_server, AdvertisementInstanceId id,
    fuchsia::bluetooth::le::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> handle,
    AdvertiseCompleteCallback complete_cb)
    : peripheral_server_(peripheral_server),
      id_(id),
      parameters_(std::move(parameters)),
      advertise_complete_cb_(std::move(complete_cb)),
      weak_ptr_factory_(this) {
  ZX_ASSERT(advertise_complete_cb_);
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
  auto status_cb = [self](auto adv_instance, bt::hci::Status status) {
    if (!self) {
      bt_log(DEBUG, LOG_TAG, "advertisement canceled before advertising started");
      // Destroying `adv_instance` will stop advertising.
      return;
    }

    if (!status) {
      bt_log(WARN, LOG_TAG, "failed to start advertising (status: %s)", bt_str(status));
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
  ZX_ASSERT(!instance_);
  instance_ = std::move(instance);
}

void LowEnergyPeripheralServer::AdvertisementInstance::OnConnected(
    bt::gap::AdvertisementId advertisement_id, bt::hci::ConnectionPtr link,
    bt::sm::BondableMode bondable_mode) {
  ZX_ASSERT(link);
  ZX_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_conn =
      [self, link_str = link->ToString()](
          fpromise::result<std::unique_ptr<bt::gap::LowEnergyConnectionHandle>, bt::HostError>
              result) mutable {
        if (!self) {
          return;
        }

        // HCI advertising ends when a connection is received (even for error results), so clear the
        // stale advertisement handle.
        self->instance_.reset();

        if (result.is_error()) {
          bt_log(INFO, LOG_TAG, "incoming connection rejected (link: %s)", link_str.c_str());
          self->StartAdvertising();
          return;
        }

        auto conn = result.take_value();
        auto peer_id = conn->peer_identifier();
        auto* peer = self->peripheral_server_->adapter()->peer_cache()->FindById(peer_id);
        ZX_ASSERT(peer);

        bt_log(INFO, LOG_TAG, "central connected (peer: %s)", bt_str(peer->identifier()));

        fidl::InterfaceHandle<fble::Connection> conn_handle =
            self->peripheral_server_->CreateConnectionServer(std::move(conn));

        // Restart advertising after the client acknowledges the connection.
        auto on_connected_cb = [self] {
          if (self) {
            self->StartAdvertising();
          }
        };
        self->advertised_peripheral_->OnConnected(
            fidl_helpers::PeerToFidlLe(*peer), std::move(conn_handle), std::move(on_connected_cb));
      };

  // TODO(fxbug.dev/648): The conversion from hci::Connection to
  // gap::LowEnergyConnectionHandle should be performed by a gap library object
  // and not in this layer.
  peripheral_server_->adapter()->le()->RegisterRemoteInitiatedLink(std::move(link), bondable_mode,
                                                                   std::move(on_conn));
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
  ZX_DEBUG_ASSERT(handle_);
}

LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::~AdvertisementInstanceDeprecated() {
  handle_closed_wait_.Cancel();
}

zx_status_t LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::Register(
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
  ZX_ASSERT(inserted);
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
                       auto instance, bt::hci::Status status) {
    // Advertising will be stopped when |instance| gets destroyed.
    if (!self) {
      return;
    }

    ZX_ASSERT(self->advertisement_deprecated_);
    ZX_ASSERT(self->advertisement_deprecated_->id() == bt::gap::kInvalidAdvertisementId);

    fble::Peripheral_StartAdvertising_Result result;
    if (!status) {
      bt_log(WARN, LOG_TAG, "%s: failed to start advertising (status: %s)", func, bt_str(status));

      result.set_err(FidlErrorFromStatus(status));

      // The only scenario in which it is valid to leave |advertisement_| intact in a failure
      // scenario is if StartAdvertising was called while a previous call was in progress. This
      // aborts the prior request causing it to end with the "kCanceled" status. This means that
      // another request is currently progress.
      if (status.error() != bt::HostError::kCanceled) {
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

void LowEnergyPeripheralServer::OnConnectedDeprecated(bt::gap::AdvertisementId advertisement_id,
                                                      bt::hci::ConnectionPtr link,
                                                      BondableMode bondable_mode) {
  ZX_DEBUG_ASSERT(link);
  ZX_DEBUG_ASSERT(advertisement_id != bt::gap::kInvalidAdvertisementId);

  // Abort connection procedure if advertisement was canceled by the client.
  if (!advertisement_deprecated_ || advertisement_deprecated_->id() != advertisement_id) {
    bt_log(INFO, LOG_TAG,
           "dropping connection from canceled advertisement (advertisement id: "
           "%s)",
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

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto on_conn = [self, local = std::move(local), remote = std::move(remote), advertisement_id,
                  link_str = link->ToString()](auto result) mutable {
    if (!self) {
      return;
    }
    if (result.is_error()) {
      bt_log(INFO, LOG_TAG, "incoming connection rejected (link: %s)", link_str.c_str());
      return;
    }

    auto conn = result.take_value();
    auto peer_id = conn->peer_identifier();
    auto* peer = self->adapter()->peer_cache()->FindById(peer_id);
    ZX_ASSERT(peer);

    bt_log(INFO, LOG_TAG, "central connected (peer: %s)", bt_str(peer->identifier()));

    fidl::InterfaceHandle<fble::Connection> conn_handle =
        self->CreateConnectionServer(std::move(conn));

    self->binding()->events().OnPeerConnected(fidl_helpers::PeerToFidlLe(*peer),
                                              std::move(conn_handle));

    // Close the AdvertisingHandle in response to a connection iff the connection's associated
    // advertisement ID still corresponds to the active advertisement. This may not be the
    // case if another StartAdvertising request comes in during the GAP Connection creation
    // process.
    if (self->advertisement_deprecated_ &&
        self->advertisement_deprecated_->id() == advertisement_id) {
      self->advertisement_deprecated_.reset();
    }
  };

  // TODO(fxbug.dev/648): The conversion from hci::Connection to
  // gap::LowEnergyConnectionHandle should be performed by a gap library object
  // and not in this layer.
  adapter()->le()->RegisterRemoteInitiatedLink(std::move(link), bondable_mode, std::move(on_conn));
}

fidl::InterfaceHandle<fuchsia::bluetooth::le::Connection>
LowEnergyPeripheralServer::CreateConnectionServer(
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection) {
  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  ZX_ASSERT(status == ZX_OK);

  auto conn_server =
      std::make_unique<LowEnergyConnectionServer>(std::move(connection), std::move(local));
  auto conn_server_id = next_connection_server_id_++;
  conn_server->set_closed_handler([this, conn_server_id] {
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
      status_cb({}, bt::hci::Status(bt::HostError::kInvalidParameters));
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
      status_cb({}, bt::hci::Status(bt::HostError::kInvalidParameters));
      return;
    }
    scan_rsp = std::move(*maybe_scan_rsp);
  }
  bt::gap::AdvertisingInterval interval = fidl_helpers::AdvertisingIntervalFromFidl(
      parameters.has_mode_hint() ? parameters.mode_hint() : fble::AdvertisingModeHint::SLOW);

  bt::gap::LowEnergyAdvertisingManager::ConnectionCallback connect_cb;

  // Per the API contract of `AdvertisingParameters` FIDL, if `connection_options` is present or
  // the deprecated `connectable` parameter is true, advertisements will be connectable.
  // `connectable_parameter` was the predecessor of `connection_options` and
  // TODO(fxbug.dev/44749): will be removed once all consumers of it have migrated to
  // `connection_options`.
  bool connectable = parameters.has_connection_options() ||
                     (parameters.has_connectable() && parameters.connectable());
  if (connectable) {
    // Per the API contract of the `ConnectionOptions` FIDL, the bondable mode of the connection
    // defaults to bondable mode unless the `connection_options` table exists and
    // `bondable_mode` is explicitly set to false.
    BondableMode bondable_mode = (!parameters.has_connection_options() ||
                                  !parameters.connection_options().has_bondable_mode() ||
                                  parameters.connection_options().bondable_mode())
                                     ? BondableMode::Bondable
                                     : BondableMode::NonBondable;
    auto self = weak_ptr_factory_.GetWeakPtr();
    connect_cb = [self, bondable_mode, advertisement_instance](bt::gap::AdvertisementId id,
                                                               auto link) {
      if (!self) {
        return;
      }

      // Handle connection for deprecated StartAdvertising method.
      if (!advertisement_instance) {
        self->OnConnectedDeprecated(id, std::move(link), bondable_mode);
        return;
      }

      auto advertisement_iter = self->advertisements_.find(*advertisement_instance);
      if (advertisement_iter == self->advertisements_.end()) {
        bt_log(DEBUG, LOG_TAG, "closing connection for canceled advertisement");
        link.reset();
        return;
      }
      advertisement_iter->second.OnConnected(id, std::move(link), bondable_mode);
    };
  }

  ZX_ASSERT(adapter()->le());
  adapter()->le()->StartAdvertising(std::move(adv_data), std::move(scan_rsp), std::move(connect_cb),
                                    interval, /*anonymous=*/false, include_tx_power_level,
                                    std::move(status_cb));
}

}  // namespace bthost
