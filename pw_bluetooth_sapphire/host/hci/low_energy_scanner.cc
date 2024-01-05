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

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_scanner.h"

#include "pw_bluetooth_sapphire/internal/host/hci/util.h"

namespace bt::hci {

static std::string ScanStateToString(LowEnergyScanner::State state) {
  switch (state) {
    case LowEnergyScanner::State::kIdle:
      return "(idle)";
    case LowEnergyScanner::State::kStopping:
      return "(stopping)";
    case LowEnergyScanner::State::kInitiating:
      return "(initiating)";
    case LowEnergyScanner::State::kActiveScanning:
      return "(active scanning)";
    case LowEnergyScanner::State::kPassiveScanning:
      return "(passive scanning)";
    default:
      break;
  }

  BT_PANIC("invalid scanner state: %u", static_cast<unsigned int>(state));
  return "(unknown)";
}

LowEnergyScanner::PendingScanResult::PendingScanResult(
    LowEnergyScanResult result,
    pw::chrono::SystemClock::duration timeout,
    pw::async::Dispatcher& dispatcher,
    fit::closure timeout_handler)
    : result_(result), timeout_(timeout), timeout_task_(dispatcher) {
  timeout_task_.set_function(
      [timeout_handler = std::move(timeout_handler)](pw::async::Context /*ctx*/,
                                                     pw::Status status) {
        if (status.ok()) {
          timeout_handler();
        }
      });
}

void LowEnergyScanner::PendingScanResult::AppendData(const ByteBuffer& data) {
  buffer_.Write(data, data_size_);
  data_size_ += data.size();

  timeout_task_.Cancel();
  timeout_task_.PostAfter(timeout_);
}

LowEnergyScanner::LowEnergyScanner(LocalAddressDelegate* local_addr_delegate,
                                   hci::Transport::WeakPtr hci,
                                   pw::async::Dispatcher& pw_dispatcher)
    : pw_dispatcher_(pw_dispatcher),
      scan_timeout_task_(pw_dispatcher_),
      local_addr_delegate_(local_addr_delegate),
      hci_(std::move(hci)) {
  BT_DEBUG_ASSERT(local_addr_delegate_);
  BT_DEBUG_ASSERT(hci_.is_alive());
  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(
      hci_->command_channel()->AsWeakPtr());

  scan_timeout_task_.set_function(
      [this](pw::async::Context /*ctx*/, pw::Status status) {
        if (status.ok() && IsScanning()) {
          StopScanInternal(false);
        }
      });
}

bool LowEnergyScanner::StartScan(const ScanOptions& options,
                                 ScanStatusCallback callback) {
  BT_ASSERT(callback);
  BT_ASSERT(options.window < options.interval);

  if (state_ != State::kIdle) {
    bt_log(ERROR,
           "hci-le",
           "cannot start scan while in state: %s",
           ScanStateToString(state_).c_str());
    return false;
  }

  state_ = State::kInitiating;
  scan_response_timeout_ = options.scan_response_timeout;
  scan_cb_ = std::move(callback);

  // Obtain the local address type.
  local_addr_delegate_->EnsureLocalAddress(
      [this, options, callback = std::move(callback)](
          const auto& address) mutable {
        StartScanInternal(address, options, std::move(callback));
      });

  return true;
}

void LowEnergyScanner::StartScanInternal(const DeviceAddress& local_address,
                                         const ScanOptions& options,
                                         ScanStatusCallback callback) {
  // Check if the scan request was canceled by StopScan() while we were waiting
  // for the local address.
  if (state_ != State::kInitiating) {
    bt_log(DEBUG,
           "hci-le",
           "scan request was canceled while obtaining local address");
    return;
  }

  bt_log(DEBUG,
         "hci-le",
         "requesting scan (%s, address: %s, interval: %#.4x, window: %#.4x)",
         (options.active ? "active" : "passive"),
         local_address.ToString().c_str(),
         options.interval,
         options.window);

  EmbossCommandPacket scan_params_command =
      BuildSetScanParametersPacket(local_address, options);
  EmbossCommandPacket scan_enable_command = BuildEnablePacket(
      options, pw::bluetooth::emboss::GenericEnableParam::ENABLE);

  hci_cmd_runner_->QueueCommand(std::move(scan_params_command));
  hci_cmd_runner_->QueueCommand(std::move(scan_enable_command));
  hci_cmd_runner_->RunCommands([this,
                                active = options.active,
                                period = options.period](Result<> status) {
    BT_DEBUG_ASSERT(scan_cb_);
    BT_DEBUG_ASSERT(state_ == State::kInitiating);

    if (status.is_error()) {
      if (status == ToResult(HostError::kCanceled)) {
        bt_log(DEBUG, "hci-le", "scan canceled");
        return;
      }

      bt_log(ERROR, "hci-le", "failed to start scan: %s", bt_str(status));
      state_ = State::kIdle;
      scan_cb_(ScanStatus::kFailed);
      return;
    }

    // Schedule the timeout.
    if (period != kPeriodInfinite) {
      scan_timeout_task_.PostAfter(period);
    }

    if (active) {
      state_ = State::kActiveScanning;
      scan_cb_(ScanStatus::kActive);
    } else {
      state_ = State::kPassiveScanning;
      scan_cb_(ScanStatus::kPassive);
    }
  });
}

bool LowEnergyScanner::StopScan() {
  if (state_ == State::kStopping || state_ == State::kIdle) {
    bt_log(DEBUG,
           "hci-le",
           "cannot stop scan while in state: %s",
           ScanStateToString(state_).c_str());
    return false;
  }

  // Scan is either being initiated or already running. Cancel any in-flight HCI
  // command sequence.
  if (!hci_cmd_runner_->IsReady()) {
    hci_cmd_runner_->Cancel();
  }

  // We'll tell the controller to stop scanning even if it is not (this is OK
  // because the command will have no effect; see Core Spec v5.0, Vol 2, Part E,
  // Section 7.8.11, paragraph 4).
  StopScanInternal(true);
  return true;
}

void LowEnergyScanner::StopScanInternal(bool stopped_by_user) {
  BT_DEBUG_ASSERT(scan_cb_);

  scan_timeout_task_.Cancel();
  state_ = State::kStopping;

  // Notify any pending scan results unless the scan was terminated by the user.
  if (!stopped_by_user) {
    for (auto& result : pending_results_) {
      auto& pending = result.second;
      delegate_->OnPeerFound(pending->result(), pending->data());
    }
  }

  // Either way clear all results from the previous scan period.
  pending_results_.clear();

  BT_DEBUG_ASSERT(hci_cmd_runner_->IsReady());

  // Tell the controller to stop scanning.
  ScanOptions options;
  EmbossCommandPacket command = BuildEnablePacket(
      options, pw::bluetooth::emboss::GenericEnableParam::DISABLE);

  hci_cmd_runner_->QueueCommand(std::move(command));
  hci_cmd_runner_->RunCommands([this, stopped_by_user](Result<> status) {
    BT_DEBUG_ASSERT(scan_cb_);
    BT_DEBUG_ASSERT(state_ == State::kStopping);
    state_ = State::kIdle;

    // Something went wrong but there isn't really a meaningful way to recover,
    // so we just fall through and notify the caller with ScanStatus::kFailed
    // instead.
    bt_is_error(
        status, WARN, "hci-le", "failed to stop scan: %s", bt_str(status));

    ScanStatus scan_status = ScanStatus::kFailed;
    if (status.is_error()) {
      scan_status = ScanStatus::kFailed;
    } else if (stopped_by_user) {
      scan_status = ScanStatus::kStopped;
    } else {
      scan_status = ScanStatus::kComplete;
    }

    scan_cb_(scan_status);
  });
}

void LowEnergyScanner::HandleScanResponse(const DeviceAddress& address,
                                          bool resolved,
                                          int8_t rssi) {
  if (!HasPendingResult(address)) {
    bt_log(TRACE, "hci-le", "dropping unmatched scan response");
    return;
  }

  std::unique_ptr<PendingScanResult>& pending = GetPendingResult(address);
  BT_DEBUG_ASSERT(address == pending->result().address);

  pending->set_resolved(resolved);
  pending->set_rssi(rssi);
  delegate_->OnPeerFound(pending->result(), pending->data());
  RemovePendingResult(address);
}

}  // namespace bt::hci
