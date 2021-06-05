// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "legacy_low_energy_advertiser.h"

#include <endian.h>
#include <lib/async/default.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

// The size, in bytes, of the serialized TX Power Level.
constexpr size_t kTxPowerLevelTLVSize = 3;

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildEnablePacket(
    const DeviceAddress& address, GenericEnableParam enable) {
  constexpr size_t kPayloadSize = sizeof(LESetAdvertisingEnableCommandParams);
  auto packet = CommandPacket::New(kLESetAdvertisingEnable, kPayloadSize);
  packet->mutable_payload<LESetAdvertisingEnableCommandParams>()->advertising_enable = enable;
  ZX_ASSERT(packet);
  return packet;
}

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildSetAdvertisingData(
    const DeviceAddress& address, const AdvertisingData& data, AdvFlags flags) {
  auto packet =
      CommandPacket::New(kLESetAdvertisingData, sizeof(LESetAdvertisingDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetAdvertisingDataCommandParams>();
  params->adv_data_length = data.CalculateBlockSize(/*include_flags=*/true);

  MutableBufferView adv_view(params->adv_data, params->adv_data_length);
  data.WriteBlock(&adv_view, flags);

  return packet;
}

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildSetScanResponse(
    const DeviceAddress& address, const AdvertisingData& scan_rsp) {
  auto packet =
      CommandPacket::New(kLESetScanResponseData, sizeof(LESetScanResponseDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetScanResponseDataCommandParams>();
  params->scan_rsp_data_length = scan_rsp.CalculateBlockSize();

  MutableBufferView scan_data_view(params->scan_rsp_data, sizeof(params->scan_rsp_data));
  scan_rsp.WriteBlock(&scan_data_view, std::nullopt);

  return packet;
}

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildSetAdvertisingParams(
    const DeviceAddress& address, LEAdvertisingType type, LEOwnAddressType own_address_type,
    AdvertisingIntervalRange interval) {
  auto packet = CommandPacket::New(kLESetAdvertisingParameters,
                                   sizeof(LESetAdvertisingParametersCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();

  auto params = packet->mutable_payload<LESetAdvertisingParametersCommandParams>();
  params->adv_interval_min = htole16(interval.min());
  params->adv_interval_max = htole16(interval.max());
  params->adv_type = type;
  params->own_address_type = own_address_type;
  params->adv_channel_map = kLEAdvertisingChannelAll;
  params->adv_filter_policy = LEAdvFilterPolicy::kAllowAll;

  // We don't support directed advertising yet, so leave peer_address and peer_address_type as 0x00
  // (|packet| parameters are initialized to zero above).

  return packet;
}

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildUnsetAdvertisingData(
    const DeviceAddress& address) {
  auto packet =
      CommandPacket::New(kLESetAdvertisingData, sizeof(LESetAdvertisingDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  return packet;
}

std::unique_ptr<CommandPacket> LegacyLowEnergyAdvertiser::BuildUnsetScanResponse(
    const DeviceAddress& address) {
  auto packet =
      CommandPacket::New(kLESetScanResponseData, sizeof(LESetScanResponseDataCommandParams));
  packet->mutable_view()->mutable_payload_data().SetToZeros();
  return packet;
}

static std::unique_ptr<CommandPacket> BuildReadAdvertisingTxPower() {
  auto packet = CommandPacket::New(kLEReadAdvertisingChannelTxPower);
  ZX_ASSERT(packet);
  return packet;
}

void LegacyLowEnergyAdvertiser::StartAdvertising(const DeviceAddress& address,
                                                 const AdvertisingData& data,
                                                 const AdvertisingData& scan_rsp,
                                                 AdvertisingOptions adv_options,
                                                 ConnectionCallback connect_callback,
                                                 StatusCallback status_callback) {
  ZX_ASSERT(status_callback);
  ZX_ASSERT(address.type() != DeviceAddress::Type::kBREDR);

  if (adv_options.anonymous) {
    bt_log(WARN, "hci-le", "anonymous advertising not supported");
    status_callback(Status(HostError::kNotSupported));
    return;
  }

  if (IsAdvertising()) {
    if (!IsAdvertising(address)) {
      bt_log(INFO, "hci-le", "already advertising (only one advertisement supported at a time)");
      status_callback(Status(HostError::kNotSupported));
      return;
    }

    bt_log(DEBUG, "hci-le", "updating existing advertisement");
  }

  // If the TX Power Level is requested, ensure both buffers have enough space.
  size_t size_limit = GetSizeLimit();
  if (adv_options.include_tx_power_level) {
    size_limit -= kTxPowerLevelTLVSize;
  }

  size_t data_size = data.CalculateBlockSize(/*include_flags=*/true);
  if (data_size > size_limit) {
    bt_log(WARN, "hci-le", "advertising data too large (size: %zu, limit: %zu)", data_size,
           size_limit);
    status_callback(Status(HostError::kAdvertisingDataTooLong));
    return;
  }

  size_t scan_rsp_size = scan_rsp.CalculateBlockSize(/*include_flags=*/false);
  if (scan_rsp_size > size_limit) {
    bt_log(WARN, "hci-le", "scan response too large (size: %zu, limit: %zu)", scan_rsp_size,
           size_limit);
    status_callback(Status(HostError::kScanResponseTooLong));
    return;
  }

  // Midst of a TX power level read - send a cancel over the previous status callback.
  if (staged_params_.has_value()) {
    auto status_cb = std::move(staged_params_.value().status_callback);
    status_cb(Status(HostError::kCanceled));
  }

  // If the TX Power level is requested, then stage the parameters for the read operation.
  // If there already is an outstanding TX Power Level read request, return early.
  // Advertising on the outstanding call will now use the updated |staged_params_|.
  if (adv_options.include_tx_power_level) {
    AdvertisingData data_copy;
    data.Copy(&data_copy);

    AdvertisingData scan_rsp_copy;
    scan_rsp.Copy(&scan_rsp_copy);

    staged_params_ = StagedParams{address,
                                  adv_options.interval,
                                  adv_options.flags,
                                  std::move(data_copy),
                                  std::move(scan_rsp_copy),
                                  std::move(connect_callback),
                                  std::move(status_callback)};

    if (starting_ && hci_cmd_runner().IsReady()) {
      return;
    }
  }

  if (!hci_cmd_runner().IsReady()) {
    bt_log(DEBUG, "hci-le",
           "canceling advertising start/stop sequence due to new advertising request");
    // Abort any remaining commands from the current stop sequence. If we got
    // here then the controller MUST receive our request to disable advertising,
    // so the commands that we send next will overwrite the current advertising
    // settings and re-enable it.
    hci_cmd_runner().Cancel();
  }

  starting_ = true;

  // If the TX Power Level is requested, read it from the controller, update the data buf, and
  // proceed with starting advertising.
  //
  // If advertising was canceled during the TX power level read (either |starting_| was
  // reset or the |status_callback| was moved), return early.
  if (adv_options.include_tx_power_level) {
    auto power_cb = [this](auto, const hci::EventPacket& event) mutable {
      ZX_ASSERT(staged_params_.has_value());
      if (!starting_ || !staged_params_.value().status_callback) {
        bt_log(INFO, "hci-le", "Advertising canceled during TX Power Level read.");
        return;
      }

      if (hci_is_error(event, WARN, "hci-le", "read TX power level failed")) {
        staged_params_.value().status_callback(event.ToStatus());
        staged_params_ = {};
        starting_ = false;
        return;
      }

      const auto& params = event.return_params<hci::LEReadAdvertisingChannelTxPowerReturnParams>();

      // Update the advertising and scan response data with the TX power level.
      auto staged_params = std::move(staged_params_.value());
      staged_params.data.SetTxPower(params->tx_power);
      if (staged_params.scan_rsp.CalculateBlockSize()) {
        staged_params.scan_rsp.SetTxPower(params->tx_power);
      }
      // Reset the |staged_params_| as it is no longer in use.
      staged_params_ = {};

      StartAdvertisingInternal(
          staged_params.address, staged_params.data, staged_params.scan_rsp, staged_params.interval,
          staged_params.flags, std::move(staged_params.connect_callback),
          [this, status_callback = std::move(staged_params.status_callback)](const Status& status) {
            starting_ = false;
            status_callback(status);
          });
    };

    hci()->command_channel()->SendCommand(BuildReadAdvertisingTxPower(), std::move(power_cb));
    return;
  }

  StartAdvertisingInternal(
      address, data, scan_rsp, adv_options.interval, adv_options.flags, std::move(connect_callback),
      [this, status_callback = std::move(status_callback)](const Status& status) {
        starting_ = false;
        status_callback(status);
      });
}

bool LegacyLowEnergyAdvertiser::StopAdvertising() {
  bool ret = LowEnergyAdvertiser::StopAdvertising();
  starting_ = false;
  return ret;
}

bool LegacyLowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address) {
  bool ret = LowEnergyAdvertiser::StopAdvertising(address);
  starting_ = false;
  return ret;
}

void LegacyLowEnergyAdvertiser::OnIncomingConnection(ConnectionHandle handle, Connection::Role role,
                                                     std::optional<DeviceAddress> opt_local_address,
                                                     const DeviceAddress& peer_address,
                                                     const LEConnectionParameters& conn_params) {
  static DeviceAddress identity_address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});

  // Since legacy advertising supports only a single address set, the LE_Connection_Complete event
  // doesn't include the local address. Consequently, LegacyLowEnergyAdvertiser is the only entity
  // that knows the local address being advertised. As such, we ignore a potential value in
  // opt_local_address since we already know what the local address should be.
  ZX_ASSERT(!opt_local_address);

  // We use the identity address as the local address if we aren't advertising. If we aren't
  // advertising, this is obviously wrong. However, the link will be disconnected in that case
  // before it can propagate to higher layers.
  DeviceAddress local_address = identity_address;
  if (IsAdvertising()) {
    local_address = connection_callbacks().begin()->first;
  }

  CompleteIncomingConnection(handle, role, local_address, peer_address, conn_params);
}

}  // namespace bt::hci
