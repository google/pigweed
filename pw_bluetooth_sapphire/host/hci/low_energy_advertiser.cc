// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_advertiser.h"

#include "lib/async/default.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"

namespace bt::hci {

LowEnergyAdvertiser::LowEnergyAdvertiser(fxl::WeakPtr<Transport> hci)
    : hci_(std::move(hci)),
      hci_cmd_runner_(
          std::make_unique<SequentialCommandRunner>(async_get_default_dispatcher(), hci_)) {}

void LowEnergyAdvertiser::StartAdvertisingInternal(
    const DeviceAddress& address, const AdvertisingData& data, const AdvertisingData& scan_rsp,
    AdvertisingIntervalRange interval, AdvFlags flags, ConnectionCallback connect_callback,
    StatusCallback status_callback) {
  if (IsAdvertising(address)) {
    // Temporarily disable advertising so we can tweak the parameters
    hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kDisable));
  }

  // Set advertising and scan response data. If either piece of data is empty then it will be
  // cleared accordingly.
  hci_cmd_runner_->QueueCommand(BuildSetAdvertisingData(data, flags));
  hci_cmd_runner_->QueueCommand(BuildSetScanResponse(scan_rsp));

  // Set advertising parameters
  LEAdvertisingType type = LEAdvertisingType::kAdvNonConnInd;
  if (connect_callback) {
    type = LEAdvertisingType::kAdvInd;
  } else if (scan_rsp.CalculateBlockSize() > 0) {
    type = LEAdvertisingType::kAdvScanInd;
  }

  LEOwnAddressType own_addr_type;
  if (address.type() == DeviceAddress::Type::kLEPublic) {
    own_addr_type = LEOwnAddressType::kPublic;
  } else {
    own_addr_type = LEOwnAddressType::kRandom;
  }

  hci_cmd_runner_->QueueCommand(BuildSetAdvertisingParams(type, own_addr_type, interval));

  // Enable advertising
  hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kEnable));

  hci_cmd_runner_->RunCommands(
      [this, address, status_callback = std::move(status_callback),
       connect_callback = std::move(connect_callback)](Status status) mutable {
        if (bt_is_error(status, ERROR, "hci-le", "failed to start advertising for %s",
                        bt_str(address))) {
          // Clear out the advertising data in case it partially succeeded
          StopAdvertising(address);
        } else {
          bt_log(INFO, "hci-le", "advertising enabled for %s", bt_str(address));
          connection_callbacks_.emplace(address, std::move(connect_callback));
        }

        status_callback(status);
      });
}

// We have StopAdvertising(address) so one would naturally think to implement StopAdvertising() by
// iterating through all addresses and calling StopAdvertising(address) on each iteration. However,
// such an implementation won't work. Each call to StopAdvertising(address) checks if the command
// runner is running, cancels any pending commands if it is, and then issues new ones. Called in
// quick succession, StopAdvertising(address) won't have a chance to finish its previous hci
// commands before being cancelled. Instead, we must enqueue them all at once and then run them
// together.
bool LowEnergyAdvertiser::StopAdvertising() {
  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kDisable));

  for (const auto& [address, _] : connection_callbacks_) {
    hci_cmd_runner_->QueueCommand(BuildUnsetAdvertisingData(address));
    hci_cmd_runner_->QueueCommand(BuildUnsetScanResponse(address));
  }

  hci_cmd_runner_->RunCommands(
      [](Status status) { bt_log(INFO, "hci-le", "all advertising stopped: %s", bt_str(status)); });
  connection_callbacks_.clear();

  return true;
}

// TODO(fxbug.dev/50542): StopAdvertising() should cancel outstanding calls to StartAdvertising()
// and clean up state.
bool LowEnergyAdvertiser::StopAdvertising(const DeviceAddress& address) {
  if (!IsAdvertising(address)) {
    // not advertising, or not advertising on this address.
    return false;
  }

  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  hci_cmd_runner_->QueueCommand(BuildEnablePacket(GenericEnableParam::kDisable));
  hci_cmd_runner_->QueueCommand(BuildUnsetAdvertisingData(address));
  hci_cmd_runner_->QueueCommand(BuildUnsetScanResponse(address));
  hci_cmd_runner_->RunCommands([address](Status status) {
    bt_log(INFO, "hci-le", "advertising stopped for %s: %s", bt_str(address), bt_str(status));
  });

  connection_callbacks_.erase(address);
  return true;
}

void LowEnergyAdvertiser::CompleteIncomingConnection(ConnectionHandle handle, Connection::Role role,
                                                     const DeviceAddress& local_address,
                                                     const DeviceAddress& peer_address,
                                                     const LEConnectionParameters& conn_params) {
  // Immediately construct a Connection object. If this object goes out of scope following the error
  // checks below, it will send the a command to disconnect the link.
  std::unique_ptr<Connection> link =
      Connection::CreateLE(handle, role, local_address, peer_address, conn_params, hci());

  if (!IsAdvertising(local_address)) {
    bt_log(DEBUG, "hci-le",
           "connection received without advertising address (role: %d, local address: %s, peer "
           "address: %s, connection parameters: %s)",
           role, bt_str(local_address), bt_str(peer_address), bt_str(conn_params));
    return;
  }

  if (!connection_callbacks_[local_address]) {
    bt_log(WARN, "hci-le",
           "connection received when not connectable (role: %d, local address: %s, peer "
           "address: %s, connection parameters: %s)",
           role, bt_str(local_address), bt_str(peer_address), bt_str(conn_params));
    return;
  }

  ConnectionCallback connect_callback = std::move(connection_callbacks_[local_address]);
  StopAdvertising(local_address);
  connect_callback(std::move(link));
}

}  // namespace bt::hci
