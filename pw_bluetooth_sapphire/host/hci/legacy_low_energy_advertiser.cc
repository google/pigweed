// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "legacy_low_energy_advertiser.h"

#include <endian.h>
#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/advertising_data.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/util.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

LegacyLowEnergyAdvertiser::~LegacyLowEnergyAdvertiser() {
  // This object is probably being destroyed because the stack is shutting down, in which case the
  // HCI layer may have already been destroyed.
  if (!hci().is_alive() || !hci()->command_channel()) {
    return;
  }
  StopAdvertising();
}

std::optional<EmbossCommandPacket> LegacyLowEnergyAdvertiser::BuildEnablePacket(
    const DeviceAddress& address, pw::bluetooth::emboss::GenericEnableParam enable) {
  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LESetAdvertisingEnableCommandWriter>(
          hci_spec::kLESetAdvertisingEnable);
  auto packet_view = packet.view_t();
  packet_view.advertising_enable().Write(enable);
  return packet;
}

CommandChannel::CommandPacketVariant LegacyLowEnergyAdvertiser::BuildSetAdvertisingData(
    const DeviceAddress& address, const AdvertisingData& data, AdvFlags flags) {
  auto packet = EmbossCommandPacket::New<pw::bluetooth::emboss::LESetAdvertisingDataCommandWriter>(
      hci_spec::kLESetAdvertisingData);
  auto params = packet.view_t();
  const uint8_t data_length = static_cast<uint8_t>(data.CalculateBlockSize(/*include_flags=*/true));
  params.advertising_data_length().Write(data_length);

  MutableBufferView adv_view(params.advertising_data().BackingStorage().data(), data_length);
  data.WriteBlock(&adv_view, flags);

  return packet;
}

CommandChannel::CommandPacketVariant LegacyLowEnergyAdvertiser::BuildSetScanResponse(
    const DeviceAddress& address, const AdvertisingData& scan_rsp) {
  auto packet = EmbossCommandPacket::New<pw::bluetooth::emboss::LESetScanResponseDataCommandWriter>(
      hci_spec::kLESetScanResponseData);
  auto params = packet.view_t();
  const uint8_t data_length = static_cast<uint8_t>(scan_rsp.CalculateBlockSize());
  params.scan_response_data_length().Write(data_length);

  MutableBufferView scan_data_view(params.scan_response_data().BackingStorage().data(),
                                   data_length);
  scan_rsp.WriteBlock(&scan_data_view, /*flags=*/std::nullopt);

  return packet;
}

CommandChannel::CommandPacketVariant LegacyLowEnergyAdvertiser::BuildSetAdvertisingParams(
    const DeviceAddress& address, pw::bluetooth::emboss::LEAdvertisingType type,
    pw::bluetooth::emboss::LEOwnAddressType own_address_type, AdvertisingIntervalRange interval) {
  auto packet =
      EmbossCommandPacket::New<pw::bluetooth::emboss::LESetAdvertisingParametersCommandWriter>(
          hci_spec::kLESetAdvertisingParameters);
  auto params = packet.view_t();
  params.advertising_interval_min().UncheckedWrite(interval.min());
  params.advertising_interval_max().UncheckedWrite(interval.max());
  params.adv_type().Write(type);
  params.own_address_type().Write(own_address_type);
  params.advertising_channel_map().BackingStorage().WriteUInt(hci_spec::kLEAdvertisingChannelAll);
  params.advertising_filter_policy().Write(
      pw::bluetooth::emboss::LEAdvertisingFilterPolicy::ALLOW_ALL);

  // We don't support directed advertising yet, so leave peer_address and peer_address_type as 0x00
  // (|packet| parameters are initialized to zero above).

  return packet;
}

CommandChannel::CommandPacketVariant LegacyLowEnergyAdvertiser::BuildUnsetAdvertisingData(
    const DeviceAddress& address) {
  return EmbossCommandPacket::New<pw::bluetooth::emboss::LESetAdvertisingDataCommandWriter>(
      hci_spec::kLESetAdvertisingData);
}

CommandChannel::CommandPacketVariant LegacyLowEnergyAdvertiser::BuildUnsetScanResponse(
    const DeviceAddress& address) {
  auto packet = EmbossCommandPacket::New<pw::bluetooth::emboss::LESetScanResponseDataCommandWriter>(
      hci_spec::kLESetScanResponseData);
  return packet;
}

std::optional<EmbossCommandPacket> LegacyLowEnergyAdvertiser::BuildRemoveAdvertisingSet(
    const DeviceAddress& address) {
  auto packet =
      hci::EmbossCommandPacket::New<pw::bluetooth::emboss::LESetAdvertisingEnableCommandWriter>(
          hci_spec::kLESetAdvertisingEnable);
  auto packet_view = packet.view_t();
  packet_view.advertising_enable().Write(pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  return packet;
}

static EmbossCommandPacket BuildReadAdvertisingTxPower() {
  return EmbossCommandPacket::New<
      pw::bluetooth::emboss::LEReadAdvertisingChannelTxPowerCommandView>(
      hci_spec::kLEReadAdvertisingChannelTxPower);
}

void LegacyLowEnergyAdvertiser::StartAdvertising(const DeviceAddress& address,
                                                 const AdvertisingData& data,
                                                 const AdvertisingData& scan_rsp,
                                                 AdvertisingOptions options,
                                                 ConnectionCallback connect_callback,
                                                 ResultFunction<> result_callback) {
  fit::result<HostError> result = CanStartAdvertising(address, data, scan_rsp, options);
  if (result.is_error()) {
    result_callback(ToResult(result.error_value()));
    return;
  }

  if (IsAdvertising() && !IsAdvertising(address)) {
    bt_log(INFO, "hci-le", "already advertising (only one advertisement supported at a time)");
    result_callback(ToResult(HostError::kNotSupported));
    return;
  }

  if (IsAdvertising()) {
    bt_log(DEBUG, "hci-le", "updating existing advertisement");
  }

  // Midst of a TX power level read - send a cancel over the previous status callback.
  if (staged_params_.has_value()) {
    auto result_cb = std::move(staged_params_.value().result_callback);
    result_cb(ToResult(HostError::kCanceled));
  }

  // If the TX Power level is requested, then stage the parameters for the read operation.
  // If there already is an outstanding TX Power Level read request, return early.
  // Advertising on the outstanding call will now use the updated |staged_params_|.
  if (options.include_tx_power_level) {
    AdvertisingData data_copy;
    data.Copy(&data_copy);

    AdvertisingData scan_rsp_copy;
    scan_rsp.Copy(&scan_rsp_copy);

    staged_params_ = StagedParams{address,
                                  options.interval,
                                  options.flags,
                                  std::move(data_copy),
                                  std::move(scan_rsp_copy),
                                  std::move(connect_callback),
                                  std::move(result_callback)};

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
  // reset or the |result_callback| was moved), return early.
  if (options.include_tx_power_level) {
    auto power_cb = [this](auto, const hci::EventPacket& event) mutable {
      BT_ASSERT(staged_params_.has_value());
      if (!starting_ || !staged_params_.value().result_callback) {
        bt_log(INFO, "hci-le", "Advertising canceled during TX Power Level read.");
        return;
      }

      if (hci_is_error(event, WARN, "hci-le", "read TX power level failed")) {
        staged_params_.value().result_callback(event.ToResult());
        staged_params_ = {};
        starting_ = false;
        return;
      }

      const auto& params =
          event.return_params<hci_spec::LEReadAdvertisingChannelTxPowerReturnParams>();

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
          [this,
           result_callback = std::move(staged_params.result_callback)](const Result<>& result) {
            starting_ = false;
            result_callback(result);
          });
    };

    hci()->command_channel()->SendCommand(BuildReadAdvertisingTxPower(), std::move(power_cb));
    return;
  }

  StartAdvertisingInternal(
      address, data, scan_rsp, options.interval, options.flags, std::move(connect_callback),
      [this, result_callback = std::move(result_callback)](const Result<>& result) {
        starting_ = false;
        result_callback(result);
      });
}

void LegacyLowEnergyAdvertiser::StopAdvertising() {
  LowEnergyAdvertiser::StopAdvertising();
  starting_ = false;
}

void LegacyLowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address) {
  if (!hci_cmd_runner().IsReady()) {
    hci_cmd_runner().Cancel();
  }

  LowEnergyAdvertiser::StopAdvertisingInternal(address);
  starting_ = false;
}

void LegacyLowEnergyAdvertiser::OnIncomingConnection(
    hci_spec::ConnectionHandle handle, pw::bluetooth::emboss::ConnectionRole role,
    const DeviceAddress& peer_address, const hci_spec::LEConnectionParameters& conn_params) {
  static DeviceAddress identity_address = DeviceAddress(DeviceAddress::Type::kLEPublic, {0});

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
