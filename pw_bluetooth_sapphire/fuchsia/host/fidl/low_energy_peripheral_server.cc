// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/low_energy_peripheral_server.h"

#include <lib/async/default.h>
#include <pw_assert/check.h>
#include <zircon/status.h>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/helpers.h"
#include "pw_bluetooth_sapphire/internal/host/common/advertising_data.h"
#include "pw_bluetooth_sapphire/internal/host/common/identifier.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_advertising_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_connection_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gap/peer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

#define LOG_TAG "fidl"

using bt::sm::BondableMode;
namespace fble = fuchsia::bluetooth::le;

namespace bthost {
namespace {

fble::PeripheralError FidlErrorFromStatus(bt::hci::Result<> status) {
  PW_CHECK(status.is_error(), "FidlErrorFromStatus called on success status");
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
    LowEnergyPeripheralServer* peripheral_server,
    AdvertisementInstanceId id,
    fuchsia::bluetooth::le::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral> handle,
    AdvertiseCompleteCallback complete_cb)
    : peripheral_server_(peripheral_server),
      id_(id),
      parameters_(std::move(parameters)),
      advertise_complete_cb_(std::move(complete_cb)),
      weak_self_(this) {
  PW_CHECK(advertise_complete_cb_);
  advertised_peripheral_.Bind(std::move(handle));
  advertised_peripheral_.set_error_handler(
      [this, peripheral_server, id](zx_status_t /*status*/) {
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
  auto self = weak_self_.GetWeakPtr();
  auto status_cb = [self](auto adv_instance, bt::hci::Result<> status) {
    if (!self.is_alive()) {
      bt_log(
          DEBUG, LOG_TAG, "advertisement canceled before advertising started");
      // Destroying `adv_instance` will stop advertising.
      return;
    }

    if (bt_is_error(status,
                    WARN,
                    LOG_TAG,
                    "failed to start advertising (status: %s)",
                    bt_str(status))) {
      self->CloseWith(fpromise::error(FidlErrorFromStatus(status)));
      self->peripheral_server_->RemoveAdvertisingInstance(self->id_);
      return;
    }

    self->Register(std::move(adv_instance));
  };

  peripheral_server_->StartAdvertisingInternal(
      parameters_, std::move(status_cb), self->id_);
}

void LowEnergyPeripheralServer::ListenL2cap(
    fble::ChannelListenerRegistryListenL2capRequest request,
    ListenL2capCallback callback) {
  // TODO(fxbug.dev/42178956): Implement ListenL2cap.
  fble::ChannelListenerRegistry_ListenL2cap_Result result;
  callback(std::move(result.set_err(ZX_ERR_NOT_SUPPORTED)));
}

void LowEnergyPeripheralServer::AdvertisementInstance::Register(
    bt::gap::AdvertisementInstance instance) {
  PW_CHECK(!instance_);
  instance_ = std::move(instance);
}

void LowEnergyPeripheralServer::AdvertisementInstance::OnConnected(
    bt::gap::AdvertisementId advertisement_id,
    bt::gap::Adapter::LowEnergy::ConnectionResult result) {
  PW_CHECK(advertisement_id != bt::gap::kInvalidAdvertisementId);

  // HCI advertising ends when a connection is received (even for error
  // results), so clear the stale advertisement handle.
  instance_.reset();

  if (result.is_error()) {
    bt_log(INFO,
           LOG_TAG,
           "incoming connection failed; restarting advertising (adv instance "
           "id: %zu, prev adv "
           "id: %s)",
           id_,
           bt_str(advertisement_id));
    StartAdvertising();
    return;
  }

  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn =
      std::move(result).value();
  bt::PeerId peer_id = conn->peer_identifier();
  bt::gap::Peer* peer =
      peripheral_server_->adapter()->peer_cache()->FindById(peer_id);
  PW_CHECK(peer);

  bt_log(INFO,
         LOG_TAG,
         "peripheral received connection to advertisement (peer: %s, adv id: "
         "%s, adv "
         "instance id: %zu)",
         bt_str(peer->identifier()),
         bt_str(advertisement_id),
         id_);

  fidl::InterfaceHandle<fble::Connection> conn_handle =
      peripheral_server_->CreateConnectionServer(std::move(conn));

  // Restart advertising after the client acknowledges the connection.
  auto self = weak_self_.GetWeakPtr();
  auto on_connected_cb = [self] {
    if (self.is_alive()) {
      self->StartAdvertising();
    }
  };
  advertised_peripheral_->OnConnected(fidl_helpers::PeerToFidlLe(*peer),
                                      std::move(conn_handle),
                                      std::move(on_connected_cb));
}

void LowEnergyPeripheralServer::AdvertisementInstance::CloseWith(
    fpromise::result<void, fuchsia::bluetooth::le::PeripheralError> result) {
  if (advertise_complete_cb_) {
    advertised_peripheral_.Unbind();
    advertise_complete_cb_(std::move(result));
  }
}

LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::
    AdvertisementInstanceDeprecated(
        fidl::InterfaceRequest<fuchsia::bluetooth::le::AdvertisingHandle>
            handle)
    : handle_(std::move(handle)) {
  PW_DCHECK(handle_);
}

LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::
    ~AdvertisementInstanceDeprecated() {
  handle_closed_wait_.Cancel();
}

zx_status_t
LowEnergyPeripheralServer::AdvertisementInstanceDeprecated::Register(
    bt::gap::AdvertisementInstance instance) {
  PW_DCHECK(!instance_);

  instance_ = std::move(instance);
  pending_ = false;

  handle_closed_wait_.set_object(handle_.channel().get());
  handle_closed_wait_.set_trigger(ZX_CHANNEL_PEER_CLOSED);
  handle_closed_wait_.set_handler(
      [this](auto*, auto*, zx_status_t status, const auto*) {
        // Don't do anything if the wait was explicitly canceled by us.
        if (status != ZX_ERR_CANCELED) {
          bt_log(TRACE, LOG_TAG, "AdvertisingHandle closed");
          instance_.reset();
        }
      });

  zx_status_t status =
      handle_closed_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    bt_log(DEBUG,
           LOG_TAG,
           "failed to begin wait on AdvertisingHandle: %s",
           zx_status_get_string(status));
  }
  return status;
}

LowEnergyPeripheralServer::LowEnergyPeripheralServer(
    bt::gap::Adapter::WeakPtr adapter,
    bt::gatt::GATT::WeakPtr gatt,
    pw::bluetooth_sapphire::LeaseProvider& wake_lease_provider,
    fidl::InterfaceRequest<Peripheral> request,
    bool privileged)
    : AdapterServerBase(std::move(adapter), this, std::move(request)),
      wake_lease_provider_(wake_lease_provider),
      gatt_(std::move(gatt)),
      privileged_(privileged),
      weak_self_(this) {}

LowEnergyPeripheralServer::~LowEnergyPeripheralServer() {
  PW_CHECK(adapter()->bredr());
}

void LowEnergyPeripheralServer::Advertise(
    fble::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral>
        advertised_peripheral,
    AdvertiseCallback callback) {
  // Advertise and StartAdvertising may not be used simultaneously.
  if (advertisement_deprecated_.has_value()) {
    callback(fpromise::error(fble::PeripheralError::FAILED));
    return;
  }

  AdvertisementInstanceId instance_id = next_advertisement_instance_id_++;

  // Non-privileged clients should not be able to advertise with a public
  // address, so we default to a random address type.
  if (!privileged_ && parameters.has_address_type() &&
      parameters.address_type() == fuchsia::bluetooth::AddressType::PUBLIC) {
    bt_log(WARN,
           LOG_TAG,
           "Cannot advertise public address (instance id: %zu)",
           instance_id);
    callback(fpromise::error(fble::PeripheralError::INVALID_PARAMETERS));
    return;
  }

  auto [iter, inserted] =
      advertisements_.try_emplace(instance_id,
                                  this,
                                  instance_id,
                                  std::move(parameters),
                                  std::move(advertised_peripheral),
                                  std::move(callback));
  PW_CHECK(inserted);
  iter->second.StartAdvertising();
}

void LowEnergyPeripheralServer::StartAdvertising(
    fble::AdvertisingParameters parameters,
    ::fidl::InterfaceRequest<fble::AdvertisingHandle> token,
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

  if (queued_start_advertising_) {
    result.set_err(fble::PeripheralError::ABORTED);
    std::get<StartAdvertisingCallback> (*queued_start_advertising_)(
        std::move(result));
    queued_start_advertising_.emplace(
        std::move(parameters), std::move(token), std::move(callback));
    return;
  }

  if (advertisement_deprecated_) {
    bt_log(DEBUG, LOG_TAG, "reconfigure existing advertising instance");
    // If the old advertisement is still pending, queue the new advertisement.
    if (advertisement_deprecated_->pending()) {
      queued_start_advertising_.emplace(
          std::move(parameters), std::move(token), std::move(callback));
      return;
    }
    // Otherwise, immediately replace the old advertisement.
    advertisement_deprecated_.reset();
  }

  // Create an entry to mark that the request is in progress.
  advertisement_deprecated_.emplace(std::move(token));
  advertisement_deprecated_->set_pending(true);

  auto self = weak_self_.GetWeakPtr();
  auto status_cb = [self, callback = std::move(callback), func = __FUNCTION__](
                       auto instance, bt::hci::Result<> status) {
    // Advertising will be stopped when |instance| gets destroyed.
    if (!self.is_alive()) {
      return;
    }

    PW_CHECK(self->advertisement_deprecated_);
    PW_CHECK(self->advertisement_deprecated_->id() ==
             bt::gap::kInvalidAdvertisementId);

    fble::Peripheral_StartAdvertising_Result result;

    // If an advertisement was queued, cancel this advertisement and start a new
    // advertisement.
    if (self->queued_start_advertising_) {
      {
        // Stop the advertisement.
        auto _ = std::move(instance);
      }
      self->advertisement_deprecated_.reset();
      result.set_err(fble::PeripheralError::ABORTED);
      callback(std::move(result));
      auto start_advertising =
          std::move(self->queued_start_advertising_.value());
      self->queued_start_advertising_.reset();
      self->StartAdvertising(std::move(std::get<0>(start_advertising)),
                             std::move(std::get<1>(start_advertising)),
                             std::move(std::get<2>(start_advertising)));
      return;
    }

    if (status.is_error()) {
      bt_log(WARN,
             LOG_TAG,
             "%s: failed to start advertising (status: %s)",
             func,
             bt_str(status));

      result.set_err(FidlErrorFromStatus(status));

      // The only scenario in which it is valid to leave |advertisement_| intact
      // in a failure scenario is if StartAdvertising was called while a
      // previous call was in progress. This aborts the prior request causing it
      // to end with the "kCanceled" status. This means that another request is
      // currently progress.
      if (!status.error_value().is(bt::HostError::kCanceled)) {
        self->advertisement_deprecated_.reset();
      }

      callback(std::move(result));
      return;
    }

    zx_status_t ecode =
        self->advertisement_deprecated_->Register(std::move(instance));
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

const bt::gap::LowEnergyConnectionHandle*
LowEnergyPeripheralServer::FindConnectionForTesting(bt::PeerId id) const {
  auto connections_iter = std::find_if(
      connections_.begin(), connections_.end(), [id](const auto& conn) {
        return conn.second->conn()->peer_identifier() == id;
      });
  if (connections_iter != connections_.end()) {
    return connections_iter->second->conn();
  }
  return nullptr;
}

void LowEnergyPeripheralServer::OnConnectedDeprecated(
    bt::gap::AdvertisementId advertisement_id,
    bt::gap::Adapter::LowEnergy::ConnectionResult result) {
  PW_CHECK(advertisement_id != bt::gap::kInvalidAdvertisementId);

  // Abort connection procedure if advertisement was canceled by the client.
  if (!advertisement_deprecated_ ||
      advertisement_deprecated_->id() != advertisement_id) {
    bt_log(
        INFO,
        LOG_TAG,
        "dropping connection to canceled advertisement (advertisement id: %s)",
        bt_str(advertisement_id));
    return;
  }

  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  if (status != ZX_OK) {
    bt_log(ERROR,
           LOG_TAG,
           "failed to create channel for Connection (status: %s)",
           zx_status_get_string(status));
    return;
  }

  if (result.is_error()) {
    bt_log(INFO,
           LOG_TAG,
           "incoming connection to advertisement failed (advertisement id: %s)",
           bt_str(advertisement_id));
    return;
  }

  auto conn = std::move(result).value();
  auto peer_id = conn->peer_identifier();
  auto* peer = adapter()->peer_cache()->FindById(peer_id);
  PW_CHECK(peer);

  bt_log(INFO,
         LOG_TAG,
         "central connected (peer: %s, advertisement id: %s)",
         bt_str(peer->identifier()),
         bt_str(advertisement_id));

  fidl::InterfaceHandle<fble::Connection> conn_handle =
      CreateConnectionServer(std::move(conn));

  binding()->events().OnPeerConnected(fidl_helpers::PeerToFidlLe(*peer),
                                      std::move(conn_handle));
  advertisement_deprecated_.reset();
}

fidl::InterfaceHandle<fuchsia::bluetooth::le::Connection>
LowEnergyPeripheralServer::CreateConnectionServer(
    std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection) {
  zx::channel local, remote;
  zx_status_t status = zx::channel::create(0, &local, &remote);
  PW_CHECK(status == ZX_OK);

  auto conn_server_id = next_connection_server_id_++;
  auto conn_server = std::make_unique<LowEnergyConnectionServer>(
      adapter(),
      gatt_,
      wake_lease_provider_,
      std::move(connection),
      std::move(local),
      [this, conn_server_id] {
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
  bt::AdvertisingData adv_data;
  bool include_tx_power_level = false;
  if (parameters.has_data()) {
    auto maybe_adv_data =
        fidl_helpers::AdvertisingDataFromFidl(parameters.data());
    if (!maybe_adv_data) {
      bt_log(WARN, LOG_TAG, "invalid advertising data");
      status_cb({}, ToResult(bt::HostError::kInvalidParameters));
      return;
    }

    adv_data = std::move(*maybe_adv_data);
    if (parameters.data().has_include_tx_power_level() &&
        parameters.data().include_tx_power_level()) {
      bt_log(TRACE,
             LOG_TAG,
             "Including TX Power level in advertising data at HCI layer");
      include_tx_power_level = true;
    }
  }

  bt::AdvertisingData scan_rsp;
  if (parameters.has_scan_response()) {
    auto maybe_scan_rsp =
        fidl_helpers::AdvertisingDataFromFidl(parameters.scan_response());
    if (!maybe_scan_rsp) {
      bt_log(WARN, LOG_TAG, "invalid scan response in advertising data");
      status_cb({}, ToResult(bt::HostError::kInvalidParameters));
      return;
    }
    scan_rsp = std::move(*maybe_scan_rsp);
  }

  fble::AdvertisingModeHint mode_hint = fble::AdvertisingModeHint::SLOW;
  if (parameters.has_mode_hint()) {
    mode_hint = parameters.mode_hint();
  }
  bt::gap::AdvertisingInterval interval =
      fidl_helpers::AdvertisingIntervalFromFidl(mode_hint);

  std::optional<bt::gap::Adapter::LowEnergy::ConnectableAdvertisingParameters>
      connectable_params;

  // Per the API contract of `AdvertisingParameters` FIDL, if
  // `connection_options` is present or the deprecated `connectable` parameter
  // is true, advertisements will be connectable. `connectable_parameter` was
  // the predecessor of `connection_options` and
  // TODO: https://fxbug.dev/42121197 - will be removed once all consumers of it
  // have migrated to `connection_options`.
  bool connectable = parameters.has_connection_options() ||
                     (parameters.has_connectable() && parameters.connectable());
  if (connectable) {
    connectable_params.emplace();

    auto self = weak_self_.GetWeakPtr();
    connectable_params->connection_cb =
        [self, advertisement_instance](
            bt::gap::AdvertisementId advertisement_id,
            bt::gap::Adapter::LowEnergy::ConnectionResult result) {
          if (!self.is_alive()) {
            return;
          }

          // Handle connection for deprecated StartAdvertising method.
          if (!advertisement_instance) {
            self->OnConnectedDeprecated(advertisement_id, std::move(result));
            return;
          }

          auto advertisement_iter =
              self->advertisements_.find(*advertisement_instance);
          if (advertisement_iter == self->advertisements_.end()) {
            if (result.is_ok()) {
              bt_log(DEBUG,
                     LOG_TAG,
                     "releasing connection handle for canceled advertisement "
                     "(peer: %s)",
                     bt_str(result.value()->peer_identifier()));
              result.value()->Release();
            }
            return;
          }
          advertisement_iter->second.OnConnected(advertisement_id,
                                                 std::move(result));
        };

    // Per the API contract of the `ConnectionOptions` FIDL, the bondable mode
    // of the connection defaults to bondable mode unless the
    // `connection_options` table exists and `bondable_mode` is explicitly set
    // to false.
    connectable_params->bondable_mode =
        (!parameters.has_connection_options() ||
         !parameters.connection_options().has_bondable_mode() ||
         parameters.connection_options().bondable_mode())
            ? BondableMode::Bondable
            : BondableMode::NonBondable;
  }

  bool extended_pdu = false;
  if (parameters.has_advertising_procedure()) {
    extended_pdu = parameters.advertising_procedure().is_extended();
  }

  std::optional<bt::DeviceAddress::Type> address_type = std::nullopt;
  if (parameters.has_address_type()) {
    address_type =
        fidl_helpers::FidlToDeviceAddressType(parameters.address_type());
  }

  PW_CHECK(adapter()->le());
  adapter()->le()->StartAdvertising(std::move(adv_data),
                                    std::move(scan_rsp),
                                    interval,
                                    extended_pdu,
                                    /*anonymous=*/false,
                                    include_tx_power_level,
                                    std::move(connectable_params),
                                    address_type,
                                    std::move(status_cb));
}

LowEnergyPrivilegedPeripheralServer::LowEnergyPrivilegedPeripheralServer(
    const bt::gap::Adapter::WeakPtr& adapter,
    bt::gatt::GATT::WeakPtr gatt,
    pw::bluetooth_sapphire::LeaseProvider& wake_lease_provider,
    fidl::InterfaceRequest<fuchsia::bluetooth::le::PrivilegedPeripheral>
        request)
    : AdapterServerBase(adapter, this, std::move(request)), weak_self_(this) {
  fidl::InterfaceHandle<fuchsia::bluetooth::le::Peripheral> handle;
  le_peripheral_server_ =
      std::make_unique<LowEnergyPeripheralServer>(adapter,
                                                  std::move(gatt),
                                                  wake_lease_provider,
                                                  handle.NewRequest(),
                                                  /*privileged=*/true);
}

void LowEnergyPrivilegedPeripheralServer::Advertise(
    fuchsia::bluetooth::le::AdvertisingParameters parameters,
    fidl::InterfaceHandle<fuchsia::bluetooth::le::AdvertisedPeripheral>
        advertised_peripheral,
    AdvertiseCallback callback) {
  le_peripheral_server_->Advertise(std::move(parameters),
                                   std::move(advertised_peripheral),
                                   std::move(callback));
}

void LowEnergyPrivilegedPeripheralServer::StartAdvertising(
    fble::AdvertisingParameters parameters,
    ::fidl::InterfaceRequest<fble::AdvertisingHandle> token,
    StartAdvertisingCallback callback) {
  le_peripheral_server_->StartAdvertising(
      std::move(parameters), std::move(token), std::move(callback));
}

void LowEnergyPrivilegedPeripheralServer::ListenL2cap(
    fble::ChannelListenerRegistryListenL2capRequest request,
    ListenL2capCallback callback) {
  le_peripheral_server_->ListenL2cap(std::move(request), std::move(callback));
}

}  // namespace bthost
