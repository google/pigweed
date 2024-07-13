// Copyright 2023 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_advertiser.h"

#include "pw_bluetooth_sapphire/internal/host/common/advertising_data.h"
#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {
namespace pwemb = pw::bluetooth::emboss;

LegacyLowEnergyAdvertiser::~LegacyLowEnergyAdvertiser() {
  // This object is probably being destroyed because the stack is shutting down,
  // in which case the HCI layer may have already been destroyed.
  if (!hci().is_alive() || !hci()->command_channel()) {
    return;
  }

  StopAdvertising();
}

EmbossCommandPacket LegacyLowEnergyAdvertiser::BuildEnablePacket(
    const DeviceAddress& address,
    pwemb::GenericEnableParam enable,
    bool /*extended_pdu*/) {
  auto packet =
      hci::EmbossCommandPacket::New<pwemb::LESetAdvertisingEnableCommandWriter>(
          hci_spec::kLESetAdvertisingEnable);
  auto packet_view = packet.view_t();
  packet_view.advertising_enable().Write(enable);
  return packet;
}

std::vector<EmbossCommandPacket>
LegacyLowEnergyAdvertiser::BuildSetAdvertisingData(const DeviceAddress& address,
                                                   const AdvertisingData& data,
                                                   AdvFlags flags,
                                                   bool /*extended_pdu*/) {
  auto packet =
      EmbossCommandPacket::New<pwemb::LESetAdvertisingDataCommandWriter>(
          hci_spec::kLESetAdvertisingData);
  auto params = packet.view_t();
  const uint8_t data_length =
      static_cast<uint8_t>(data.CalculateBlockSize(/*include_flags=*/true));
  params.advertising_data_length().Write(data_length);

  MutableBufferView adv_view(params.advertising_data().BackingStorage().data(),
                             data_length);
  data.WriteBlock(&adv_view, flags);

  std::vector<EmbossCommandPacket> packets;
  packets.reserve(1);
  packets.emplace_back(std::move(packet));
  return packets;
}

std::vector<EmbossCommandPacket>
LegacyLowEnergyAdvertiser::BuildSetScanResponse(const DeviceAddress& address,
                                                const AdvertisingData& scan_rsp,
                                                bool /*extended_pdu*/) {
  auto packet =
      EmbossCommandPacket::New<pwemb::LESetScanResponseDataCommandWriter>(
          hci_spec::kLESetScanResponseData);
  auto params = packet.view_t();
  const uint8_t data_length =
      static_cast<uint8_t>(scan_rsp.CalculateBlockSize());
  params.scan_response_data_length().Write(data_length);

  MutableBufferView scan_data_view(
      params.scan_response_data().BackingStorage().data(), data_length);
  scan_rsp.WriteBlock(&scan_data_view, /*flags=*/std::nullopt);

  std::vector<EmbossCommandPacket> packets;
  packets.reserve(1);
  packets.emplace_back(std::move(packet));
  return packets;
}

std::optional<EmbossCommandPacket>
LegacyLowEnergyAdvertiser::BuildSetAdvertisingParams(
    const DeviceAddress& address,
    pwemb::LEAdvertisingType type,
    pwemb::LEOwnAddressType own_address_type,
    const AdvertisingIntervalRange& interval,
    bool /*extended_pdu*/) {
  auto packet =
      EmbossCommandPacket::New<pwemb::LESetAdvertisingParametersCommandWriter>(
          hci_spec::kLESetAdvertisingParameters);
  auto params = packet.view_t();
  params.advertising_interval_min().UncheckedWrite(interval.min());
  params.advertising_interval_max().UncheckedWrite(interval.max());
  params.adv_type().Write(type);
  params.own_address_type().Write(own_address_type);
  params.advertising_channel_map().BackingStorage().WriteUInt(
      hci_spec::kLEAdvertisingChannelAll);
  params.advertising_filter_policy().Write(
      pwemb::LEAdvertisingFilterPolicy::ALLOW_ALL);

  // We don't support directed advertising yet, so leave peer_address and
  // peer_address_type as 0x00
  // (|packet| parameters are initialized to zero above).

  return packet;
}

EmbossCommandPacket LegacyLowEnergyAdvertiser::BuildUnsetAdvertisingData(
    const DeviceAddress& address, bool /*extended_pdu*/) {
  return EmbossCommandPacket::New<pwemb::LESetAdvertisingDataCommandWriter>(
      hci_spec::kLESetAdvertisingData);
}

EmbossCommandPacket LegacyLowEnergyAdvertiser::BuildUnsetScanResponse(
    const DeviceAddress& address, bool /*extended_pdu*/) {
  auto packet =
      EmbossCommandPacket::New<pwemb::LESetScanResponseDataCommandWriter>(
          hci_spec::kLESetScanResponseData);
  return packet;
}

EmbossCommandPacket LegacyLowEnergyAdvertiser::BuildRemoveAdvertisingSet(
    const DeviceAddress& address, bool /*extended_pdu*/) {
  auto packet =
      hci::EmbossCommandPacket::New<pwemb::LESetAdvertisingEnableCommandWriter>(
          hci_spec::kLESetAdvertisingEnable);
  auto packet_view = packet.view_t();
  packet_view.advertising_enable().Write(pwemb::GenericEnableParam::DISABLE);
  return packet;
}

static EmbossCommandPacket BuildReadAdvertisingTxPower() {
  return EmbossCommandPacket::New<
      pwemb::LEReadAdvertisingChannelTxPowerCommandView>(
      hci_spec::kLEReadAdvertisingChannelTxPower);
}

void LegacyLowEnergyAdvertiser::StartAdvertising(
    const DeviceAddress& address,
    const AdvertisingData& data,
    const AdvertisingData& scan_rsp,
    const AdvertisingOptions& options,
    ConnectionCallback connect_callback,
    ResultFunction<> result_callback) {
  fit::result<HostError> result =
      CanStartAdvertising(address, data, scan_rsp, options);
  if (result.is_error()) {
    result_callback(ToResult(result.error_value()));
    return;
  }

  if (IsAdvertising() && !IsAdvertising(address, options.extended_pdu)) {
    bt_log(INFO,
           "hci-le",
           "already advertising (only one advertisement supported at a time)");
    result_callback(ToResult(HostError::kNotSupported));
    return;
  }

  if (options.extended_pdu) {
    bt_log(INFO,
           "hci-le",
           "legacy advertising cannot use extended advertising PDUs");
    result_callback(ToResult(HostError::kNotSupported));
    return;
  }

  if (IsAdvertising()) {
    bt_log(DEBUG, "hci-le", "updating existing advertisement");
  }

  // Midst of a TX power level read - send a cancel over the previous status
  // callback.
  if (staged_params_.has_value()) {
    auto result_cb = std::move(staged_params_.value().result_callback);
    result_cb(ToResult(HostError::kCanceled));
  }

  // If the TX Power level is requested, then stage the parameters for the read
  // operation. If there already is an outstanding TX Power Level read request,
  // return early. Advertising on the outstanding call will now use the updated
  // |staged_params_|.
  if (options.include_tx_power_level) {
    AdvertisingData data_copy;
    data.Copy(&data_copy);

    AdvertisingData scan_rsp_copy;
    scan_rsp.Copy(&scan_rsp_copy);

    staged_params_ = StagedParams{address,
                                  std::move(data_copy),
                                  std::move(scan_rsp_copy),
                                  options,
                                  std::move(connect_callback),
                                  std::move(result_callback)};

    if (starting_ && hci_cmd_runner().IsReady()) {
      return;
    }
  }

  if (!hci_cmd_runner().IsReady()) {
    bt_log(DEBUG,
           "hci-le",
           "canceling advertising start/stop sequence due to new advertising "
           "request");
    // Abort any remaining commands from the current stop sequence. If we got
    // here then the controller MUST receive our request to disable advertising,
    // so the commands that we send next will overwrite the current advertising
    // settings and re-enable it.
    hci_cmd_runner().Cancel();
  }

  starting_ = true;
  local_address_ = DeviceAddress();

  // If the TX Power Level is requested, read it from the controller, update the
  // data buf, and proceed with starting advertising.
  //
  // If advertising was canceled during the TX power level read (either
  // |starting_| was reset or the |result_callback| was moved), return early.
  if (options.include_tx_power_level) {
    auto power_cb = [this](auto, const hci::EventPacket& event) mutable {
      BT_ASSERT(staged_params_.has_value());
      if (!starting_ || !staged_params_.value().result_callback) {
        bt_log(
            INFO, "hci-le", "Advertising canceled during TX Power Level read.");
        return;
      }

      if (hci_is_error(event, WARN, "hci-le", "read TX power level failed")) {
        staged_params_.value().result_callback(event.ToResult());
        staged_params_ = {};
        local_address_ = DeviceAddress();
        starting_ = false;
        return;
      }

      auto staged_params = std::move(staged_params_.value());
      staged_params_ = {};

      // Update the advertising and scan response data with the TX power level.
      const auto& params = event.return_params<
          hci_spec::LEReadAdvertisingChannelTxPowerReturnParams>();
      staged_params.data.SetTxPower(params->tx_power);
      if (staged_params.scan_rsp.CalculateBlockSize()) {
        staged_params.scan_rsp.SetTxPower(params->tx_power);
      }

      StartAdvertisingInternal(
          staged_params.address,
          staged_params.data,
          staged_params.scan_rsp,
          staged_params.options,
          std::move(staged_params.connect_callback),
          [this,
           address = staged_params.address,
           result_callback = std::move(staged_params.result_callback)](
              const Result<>& result) {
            starting_ = false;
            local_address_ = address;
            result_callback(result);
          });
    };

    hci()->command_channel()->SendCommand(BuildReadAdvertisingTxPower(),
                                          std::move(power_cb));
    return;
  }

  StartAdvertisingInternal(
      address,
      data,
      scan_rsp,
      options,
      std::move(connect_callback),
      [this, address, result_callback = std::move(result_callback)](
          const Result<>& result) {
        starting_ = false;
        local_address_ = address;
        result_callback(result);
      });
}

void LegacyLowEnergyAdvertiser::StopAdvertising() {
  LowEnergyAdvertiser::StopAdvertising();
  starting_ = false;
  local_address_ = DeviceAddress();
}

void LegacyLowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address,
                                                bool extended_pdu) {
  if (extended_pdu) {
    bt_log(INFO,
           "hci-le",
           "legacy advertising cannot use extended advertising PDUs");
    return;
  }

  if (!hci_cmd_runner().IsReady()) {
    hci_cmd_runner().Cancel();
  }

  LowEnergyAdvertiser::StopAdvertisingInternal(address, extended_pdu);
  starting_ = false;
  local_address_ = DeviceAddress();
}

void LegacyLowEnergyAdvertiser::OnIncomingConnection(
    hci_spec::ConnectionHandle handle,
    pwemb::ConnectionRole role,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& conn_params) {
  static DeviceAddress identity_address =
      DeviceAddress(DeviceAddress::Type::kLEPublic, {0});

  // We use the identity address as the local address if we aren't advertising.
  // If we aren't advertising, this is obviously wrong. However, the link will
  // be disconnected in that case before it can propagate to higher layers.
  DeviceAddress local_address = identity_address;
  if (IsAdvertising()) {
    local_address = local_address_;
  }

  CompleteIncomingConnection(handle,
                             role,
                             local_address,
                             peer_address,
                             conn_params,
                             /*extended_pdu=*/false);
}

}  // namespace bt::hci
