// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_advertiser.h"

#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"

namespace bt::hci {

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
  size_t size_limit = GetSizeLimit();
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
    AdvertisingIntervalRange interval,
    AdvFlags flags,
    ConnectionCallback connect_callback,
    hci::ResultFunction<> result_callback) {
  if (IsAdvertising(address)) {
    // Temporarily disable advertising so we can tweak the parameters
    std::optional<EmbossCommandPacket> packet = BuildEnablePacket(
        address, pw::bluetooth::emboss::GenericEnableParam::DISABLE);

    if (!packet) {
      bt_log(WARN,
             "hci-le",
             "cannot build HCI disable packet for %s",
             bt_str(address));
      result_callback(ToResult(HostError::kCanceled));
      return;
    }

    hci_cmd_runner_->QueueCommand(*packet);
  }

  // Set advertising parameters
  pw::bluetooth::emboss::LEAdvertisingType type =
      pw::bluetooth::emboss::LEAdvertisingType::NOT_CONNECTABLE_UNDIRECTED;
  if (connect_callback) {
    type = pw::bluetooth::emboss::LEAdvertisingType::
        CONNECTABLE_AND_SCANNABLE_UNDIRECTED;
  } else if (scan_rsp.CalculateBlockSize() > 0) {
    type = pw::bluetooth::emboss::LEAdvertisingType::SCANNABLE_UNDIRECTED;
  }

  pw::bluetooth::emboss::LEOwnAddressType own_addr_type;
  if (address.type() == DeviceAddress::Type::kLEPublic) {
    own_addr_type = pw::bluetooth::emboss::LEOwnAddressType::PUBLIC;
  } else {
    own_addr_type = pw::bluetooth::emboss::LEOwnAddressType::RANDOM;
  }

  data.Copy(&staged_parameters_.data);
  scan_rsp.Copy(&staged_parameters_.scan_rsp);

  using PacketPtr = std::unique_ptr<CommandPacket>;

  CommandChannel::CommandPacketVariant set_adv_params_packet =
      BuildSetAdvertisingParams(address, type, own_addr_type, interval);
  if (std::holds_alternative<PacketPtr>(set_adv_params_packet) &&
      !std::get<PacketPtr>(set_adv_params_packet)) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI set params packet for %s",
           bt_str(address));
    result_callback(ToResult(HostError::kCanceled));
    return;
  }

  hci_cmd_runner_->QueueCommand(
      std::move(set_adv_params_packet),
      fit::bind_member<&LowEnergyAdvertiser::OnSetAdvertisingParamsComplete>(
          this));

  // In order to support use cases where advertisers use the return parameters
  // of the SetAdvertisingParams HCI command, we place the remaining advertising
  // setup HCI commands in the result callback here. SequentialCommandRunner
  // doesn't allow enqueuing commands within a callback (during a run).
  hci_cmd_runner_->RunCommands([this,
                                address,
                                flags,
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
                                                 flags,
                                                 std::move(connect_callback),
                                                 std::move(result_callback));
    if (!success) {
      result_callback(ToResult(HostError::kCanceled));
    }
  });
}

bool LowEnergyAdvertiser::StartAdvertisingInternalStep2(
    const DeviceAddress& address,
    AdvFlags flags,
    ConnectionCallback connect_callback,
    hci::ResultFunction<> result_callback) {
  using PacketPtr = std::unique_ptr<CommandPacket>;

  CommandChannel::CommandPacketVariant set_adv_data_packet =
      BuildSetAdvertisingData(address, staged_parameters_.data, flags);
  if (std::holds_alternative<PacketPtr>(set_adv_data_packet) &&
      !std::get<PacketPtr>(set_adv_data_packet)) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI set advertising data packet for %s",
           bt_str(address));
    return false;
  }

  CommandChannel::CommandPacketVariant set_scan_rsp_packet =
      BuildSetScanResponse(address, staged_parameters_.scan_rsp);
  if (std::holds_alternative<PacketPtr>(set_scan_rsp_packet) &&
      !std::get<PacketPtr>(set_scan_rsp_packet)) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI set scan response data packet for %s",
           bt_str(address));
    return false;
  }

  std::optional<EmbossCommandPacket> enable_packet = BuildEnablePacket(
      address, pw::bluetooth::emboss::GenericEnableParam::ENABLE);

  if (!enable_packet) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI enable packet for %s",
           bt_str(address));
    return false;
  }

  hci_cmd_runner_->QueueCommand(std::move(set_adv_data_packet));
  hci_cmd_runner_->QueueCommand(std::move(set_scan_rsp_packet));
  hci_cmd_runner_->QueueCommand(*enable_packet);

  staged_parameters_.reset();
  hci_cmd_runner_->RunCommands([this,
                                address,
                                result_callback = std::move(result_callback),
                                connect_callback = std::move(connect_callback)](
                                   Result<> result) mutable {
    if (bt_is_error(result,
                    WARN,
                    "hci-le",
                    "failed to start advertising for %s",
                    bt_str(address))) {
    } else {
      bt_log(INFO, "hci-le", "advertising enabled for %s", bt_str(address));
      connection_callbacks_.emplace(address, std::move(connect_callback));
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
    const DeviceAddress& address = itr->first;

    bool success = EnqueueStopAdvertisingCommands(address);
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

void LowEnergyAdvertiser::StopAdvertisingInternal(
    const DeviceAddress& address) {
  if (!IsAdvertising(address)) {
    return;
  }

  bool success = EnqueueStopAdvertisingCommands(address);
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

  connection_callbacks_.erase(address);
}

bool LowEnergyAdvertiser::EnqueueStopAdvertisingCommands(
    const DeviceAddress& address) {
  std::optional<EmbossCommandPacket> disable_packet = BuildEnablePacket(
      address, pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  if (!disable_packet) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI disable packet for %s",
           bt_str(address));
    return false;
  }

  using PacketPtr = std::unique_ptr<hci::CommandPacket>;

  hci::CommandChannel::CommandPacketVariant unset_scan_rsp_packet =
      BuildUnsetScanResponse(address);
  if (std::holds_alternative<PacketPtr>(unset_scan_rsp_packet) &&
      !std::get<PacketPtr>(unset_scan_rsp_packet)) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI unset scan rsp packet for %s",
           bt_str(address));
    return false;
  }

  hci::CommandChannel::CommandPacketVariant unset_adv_data_packet =
      BuildUnsetAdvertisingData(address);
  if (std::holds_alternative<PacketPtr>(unset_adv_data_packet) &&
      !std::get<PacketPtr>(unset_adv_data_packet)) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI unset advertising data packet for %s",
           bt_str(address));
    return false;
  }

  std::optional<EmbossCommandPacket> remove_packet =
      BuildRemoveAdvertisingSet(address);
  if (!remove_packet) {
    bt_log(WARN,
           "hci-le",
           "cannot build HCI remove packet for %s",
           bt_str(address));
    return false;
  }

  hci_cmd_runner_->QueueCommand(*disable_packet);
  hci_cmd_runner_->QueueCommand(std::move(unset_scan_rsp_packet));
  hci_cmd_runner_->QueueCommand(std::move(unset_adv_data_packet));
  hci_cmd_runner_->QueueCommand(*remove_packet);
  return true;
}

void LowEnergyAdvertiser::CompleteIncomingConnection(
    hci_spec::ConnectionHandle handle,
    pw::bluetooth::emboss::ConnectionRole role,
    const DeviceAddress& local_address,
    const DeviceAddress& peer_address,
    const hci_spec::LEConnectionParameters& conn_params) {
  // Immediately construct a Connection object. If this object goes out of scope
  // following the error checks below, it will send the a command to disconnect
  // the link.
  std::unique_ptr<LowEnergyConnection> link =
      std::make_unique<LowEnergyConnection>(
          handle, local_address, peer_address, conn_params, role, hci());

  if (!IsAdvertising(local_address)) {
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

  if (!connection_callbacks_[local_address]) {
    bt_log(WARN,
           "hci-le",
           "connection received when not connectable (role: %d, local address: "
           "%s, peer "
           "address: %s, connection parameters: %s)",
           static_cast<uint8_t>(role),
           bt_str(local_address),
           bt_str(peer_address),
           bt_str(conn_params));
    return;
  }

  ConnectionCallback connect_callback =
      std::move(connection_callbacks_[local_address]);
  StopAdvertising(local_address);
  connect_callback(std::move(link));
}

}  // namespace bt::hci
