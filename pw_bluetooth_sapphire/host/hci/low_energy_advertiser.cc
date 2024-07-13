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

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_advertiser.h"

#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"

namespace bt::hci {
namespace pwemb = pw::bluetooth::emboss;

LowEnergyAdvertiser::LowEnergyAdvertiser(hci::Transport::WeakPtr hci)
    : hci_(std::move(hci)),
      hci_cmd_runner_(std::make_unique<SequentialCommandRunner>(
          hci_->command_channel()->AsWeakPtr())) {}

fit::result<HostError> LowEnergyAdvertiser::CanStartAdvertising(
    const DeviceAddress& address,
    const AdvertisingData& data,
    const AdvertisingData& scan_rsp,
    const AdvertisingOptions& options) const {
  BT_ASSERT(address.type() != DeviceAddress::Type::kBREDR);

  if (options.anonymous) {
    bt_log(WARN, "hci-le", "anonymous advertising not supported");
    return fit::error(HostError::kNotSupported);
  }

  // If the TX Power Level is requested, ensure both buffers have enough space.
  size_t size_limit = GetSizeLimit(options.extended_pdu);
  if (options.include_tx_power_level) {
    size_limit -= kTLVTxPowerLevelSize;
  }

  if (size_t size = data.CalculateBlockSize(/*include_flags=*/true);
      size > size_limit) {
    bt_log(WARN,
           "hci-le",
           "advertising data too large (actual: %zu, max: %zu)",
           size,
           size_limit);
    return fit::error(HostError::kAdvertisingDataTooLong);
  }

  if (size_t size = scan_rsp.CalculateBlockSize(/*include_flags=*/false);
      size > size_limit) {
    bt_log(WARN,
           "hci-le",
           "scan response too large (actual: %zu, max: %zu)",
           size,
           size_limit);
    return fit::error(HostError::kScanResponseTooLong);
  }

  return fit::ok();
}

void LowEnergyAdvertiser::StartAdvertisingInternal(
    const DeviceAddress& address,
    const AdvertisingData& data,
    const AdvertisingData& scan_rsp,
    const AdvertisingOptions& options,
    ConnectionCallback connect_callback,
    hci::ResultFunction<> result_callback) {
  if (IsAdvertising(address, options.extended_pdu)) {
    // Temporarily disable advertising so we can tweak the parameters
    EmbossCommandPacket packet = BuildEnablePacket(
        address, pwemb::GenericEnableParam::DISABLE, options.extended_pdu);
    hci_cmd_runner_->QueueCommand(packet);
  }

  // Set advertising parameters
  pwemb::LEAdvertisingType type =
      pwemb::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED;
  if (connect_callback) {
    type = pwemb::LEAdvertisingType::CONNECTABLE_AND_SCANNABLE_UNDIRECTED;
  } else if (scan_rsp.CalculateBlockSize() > 0) {
    type = pwemb::LEAdvertisingType::SCANNABLE_UNDIRECTED;
  }

  pwemb::LEOwnAddressType own_addr_type;
  if (address.type() == DeviceAddress::Type::kLEPublic) {
    own_addr_type = pwemb::LEOwnAddressType::PUBLIC;
  } else {
    own_addr_type = pwemb::LEOwnAddressType::RANDOM;
  }

  data.Copy(&staged_parameters_.data);
  scan_rsp.Copy(&staged_parameters_.scan_rsp);

  std::optional<EmbossCommandPacket> set_adv_params_packet =
      BuildSetAdvertisingParams(
          address, type, own_addr_type, options.interval, options.extended_pdu);
  if (!set_adv_params_packet) {
    bt_log(
        WARN, "hci-le", "failed to start advertising for %s", bt_str(address));
    return;
  }

  hci_cmd_runner_->QueueCommand(
      *set_adv_params_packet,
      fit::bind_member<&LowEnergyAdvertiser::OnSetAdvertisingParamsComplete>(
          this));

  // In order to support use cases where advertisers use the return parameters
  // of the SetAdvertisingParams HCI command, we place the remaining advertising
  // setup HCI commands in the result callback here. SequentialCommandRunner
  // doesn't allow enqueuing commands within a callback (during a run).
  hci_cmd_runner_->RunCommands([this,
                                address,
                                options,
                                result_callback = std::move(result_callback),
                                connect_callback = std::move(connect_callback)](
                                   hci::Result<> result) mutable {
    if (bt_is_error(result,
                    WARN,
                    "hci-le",
                    "failed to start advertising for %s",
                    bt_str(address))) {
      result_callback(result);
      return;
    }

    bool success = StartAdvertisingInternalStep2(address,
                                                 options,
                                                 std::move(connect_callback),
                                                 std::move(result_callback));
    if (!success) {
      result_callback(ToResult(HostError::kCanceled));
    }
  });
}

bool LowEnergyAdvertiser::StartAdvertisingInternalStep2(
    const DeviceAddress& address,
    const AdvertisingOptions& options,
    ConnectionCallback connect_callback,
    hci::ResultFunction<> result_callback) {
  std::vector<EmbossCommandPacket> set_adv_data_packets =
      BuildSetAdvertisingData(address,
                              staged_parameters_.data,
                              options.flags,
                              options.extended_pdu);
  for (auto& packet : set_adv_data_packets) {
    hci_cmd_runner_->QueueCommand(std::move(packet));
  }

  std::vector<EmbossCommandPacket> set_scan_rsp_packets = BuildSetScanResponse(
      address, staged_parameters_.scan_rsp, options.extended_pdu);
  for (auto& packet : set_scan_rsp_packets) {
    hci_cmd_runner_->QueueCommand(std::move(packet));
  }

  EmbossCommandPacket enable_packet = BuildEnablePacket(
      address, pwemb::GenericEnableParam::ENABLE, options.extended_pdu);
  hci_cmd_runner_->QueueCommand(enable_packet);

  staged_parameters_.reset();
  hci_cmd_runner_->RunCommands([this,
                                address,
                                extended_pdu = options.extended_pdu,
                                result_callback = std::move(result_callback),
                                connect_callback = std::move(connect_callback)](
                                   Result<> result) mutable {
    if (!bt_is_error(result,
                     WARN,
                     "hci-le",
                     "failed to start advertising for %s",
                     bt_str(address))) {
      bt_log(INFO, "hci-le", "advertising enabled for %s", bt_str(address));
      connection_callbacks_[{address, extended_pdu}] =
          std::move(connect_callback);
    }

    result_callback(result);
    OnCurrentOperationComplete();
  });

  return true;
}

// We have StopAdvertising(address) so one would naturally think to implement
// StopAdvertising() by iterating through all addresses and calling
// StopAdvertising(address) on each iteration. However, such an implementation
// won't work. Each call to StopAdvertising(address) checks if the command
// runner is running, cancels any pending commands if it is, and then issues new
// ones. Called in quick succession, StopAdvertising(address) won't have a
// chance to finish its previous HCI commands before being cancelled. Instead,
// we must enqueue them all at once and then run them together.
void LowEnergyAdvertiser::StopAdvertising() {
  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  for (auto itr = connection_callbacks_.begin();
       itr != connection_callbacks_.end();) {
    const auto& [address, extended_pdu] = itr->first;

    bool success = EnqueueStopAdvertisingCommands(address, extended_pdu);
    if (success) {
      itr = connection_callbacks_.erase(itr);
    } else {
      bt_log(WARN, "hci-le", "cannot stop advertising for %s", bt_str(address));
      itr++;
    }
  }

  if (hci_cmd_runner_->HasQueuedCommands()) {
    hci_cmd_runner_->RunCommands([this](hci::Result<> result) {
      bt_log(INFO, "hci-le", "advertising stopped: %s", bt_str(result));
      OnCurrentOperationComplete();
    });
  }
}

void LowEnergyAdvertiser::StopAdvertisingInternal(const DeviceAddress& address,
                                                  bool extended_pdu) {
  if (!IsAdvertising(address, extended_pdu)) {
    return;
  }

  bool success = EnqueueStopAdvertisingCommands(address, extended_pdu);
  if (!success) {
    bt_log(WARN, "hci-le", "cannot stop advertising for %s", bt_str(address));
    return;
  }

  hci_cmd_runner_->RunCommands([this, address](Result<> result) {
    bt_log(INFO,
           "hci-le",
           "advertising stopped for %s: %s",
           bt_str(address),
           bt_str(result));
    OnCurrentOperationComplete();
  });

  connection_callbacks_.erase({address, extended_pdu});
}

bool LowEnergyAdvertiser::EnqueueStopAdvertisingCommands(
    const DeviceAddress& address, bool extended_pdu) {
  EmbossCommandPacket disable_packet = BuildEnablePacket(
      address, pwemb::GenericEnableParam::DISABLE, extended_pdu);
  EmbossCommandPacket unset_scan_rsp_packet =
      BuildUnsetScanResponse(address, extended_pdu);
  EmbossCommandPacket unset_adv_data_packet =
      BuildUnsetAdvertisingData(address, extended_pdu);
  EmbossCommandPacket remove_packet =
      BuildRemoveAdvertisingSet(address, extended_pdu);

  hci_cmd_runner_->QueueCommand(disable_packet);
  hci_cmd_runner_->QueueCommand(unset_scan_rsp_packet);
  hci_cmd_runner_->QueueCommand(unset_adv_data_packet);
  hci_cmd_runner_->QueueCommand(remove_packet);

  return true;
}

void LowEnergyAdvertiser::CompleteIncomingConnection(
    hci_spec::ConnectionHandle handle,
    pwemb::ConnectionRole role,
    const DeviceAddress& local_address,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& conn_params,
    bool extended_pdu) {
  // Immediately construct a Connection object. If this object goes out of scope
  // following the error checks below, it will send the a command to disconnect
  // the link.
  std::unique_ptr<LowEnergyConnection> link =
      std::make_unique<LowEnergyConnection>(
          handle, local_address, peer_address, conn_params, role, hci());

  if (!IsAdvertising(local_address, extended_pdu)) {
    bt_log(DEBUG,
           "hci-le",
           "connection received without advertising address (role: %d, local "
           "address: %s, peer "
           "address: %s, connection parameters: %s)",
           static_cast<uint8_t>(role),
           bt_str(local_address),
           bt_str(peer_address),
           bt_str(conn_params));
    return;
  }

  ConnectionCallback connect_callback =
      std::move(connection_callbacks_[{local_address, extended_pdu}]);
  if (!connect_callback) {
    bt_log(DEBUG,
           "hci-le",
           "connection received when not connectable (role: %d, "
           "local address: %s, "
           "peer address: %s, "
           "connection parameters: %s)",
           static_cast<uint8_t>(role),
           bt_str(local_address),
           bt_str(peer_address),
           bt_str(conn_params));
    return;
  }

  StopAdvertising(local_address, extended_pdu);
  connect_callback(std::move(link));
  connection_callbacks_.erase({local_address, extended_pdu});
}

}  // namespace bt::hci
