// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_scanner.h"

#include <endian.h>

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/hci/advertising_report_parser.h"
#include "pw_bluetooth_sapphire/internal/host/hci/local_address_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/hci/util.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

#pragma clang diagnostic ignored "-Wswitch-enum"

namespace bt::hci {
namespace {

std::string ScanStateToString(LowEnergyScanner::State state) {
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

}  // namespace

LegacyLowEnergyScanner::PendingScanResult::PendingScanResult(
    LowEnergyScanResult result,
    const ByteBuffer& adv,
    pw::chrono::SystemClock::duration timeout,
    fit::closure timeout_handler,
    pw::async::Dispatcher& dispatcher)
    : result_(result), data_size_(0u), timeout_task_(dispatcher) {
  Append(adv);
  timeout_task_.set_function(
      [timeout_handler = std::move(timeout_handler)](pw::async::Context /*ctx*/,
                                                     pw::Status status) {
        if (status.ok()) {
          timeout_handler();
        }
      });
  timeout_task_.PostAfter(timeout);
}

void LegacyLowEnergyScanner::PendingScanResult::Append(const ByteBuffer& data) {
  BT_ASSERT(data.size() <= hci_spec::kMaxLEAdvertisingDataLength);
  buffer_.Write(data, data_size_);
  data_size_ += data.size();
}

LegacyLowEnergyScanner::LegacyLowEnergyScanner(
    LocalAddressDelegate* local_addr_delegate,
    Transport::WeakPtr hci,
    pw::async::Dispatcher& pw_dispatcher)
    : LowEnergyScanner(std::move(hci), pw_dispatcher),
      local_addr_delegate_(local_addr_delegate) {
  BT_DEBUG_ASSERT(local_addr_delegate_);
  event_handler_id_ = transport()->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLEAdvertisingReportSubeventCode,
      fit::bind_member<&LegacyLowEnergyScanner::OnAdvertisingReportEvent>(
          this));
  scan_timeout_task_.set_function(
      [this](pw::async::Context /*ctx*/, pw::Status status) {
        if (status.ok()) {
          OnScanPeriodComplete();
        }
      });
}

LegacyLowEnergyScanner::~LegacyLowEnergyScanner() {
  if (transport()->command_channel()) {
    transport()->command_channel()->RemoveEventHandler(event_handler_id_);
  }
}

bool LegacyLowEnergyScanner::StartScan(const ScanOptions& options,
                                       ScanStatusCallback callback) {
  BT_ASSERT(callback);
  BT_ASSERT(options.interval <= hci_spec::kLEScanIntervalMax &&
            options.interval >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.window <= hci_spec::kLEScanIntervalMax &&
            options.window >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.window < options.interval);

  if (state() != State::kIdle) {
    bt_log(ERROR,
           "hci-le",
           "cannot start scan while in state: %s",
           ScanStateToString(state()).c_str());
    return false;
  }

  BT_ASSERT(!scan_cb_);
  BT_ASSERT(!scan_timeout_task_.is_pending());
  BT_ASSERT(hci_cmd_runner()->IsReady());
  BT_ASSERT(pending_results_.empty());

  set_state(State::kInitiating);
  set_active_scan_requested(options.active);
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

void LegacyLowEnergyScanner::StartScanInternal(
    const DeviceAddress& local_address,
    const ScanOptions& options,
    ScanStatusCallback callback) {
  // Check if the scan request was canceled by StopScan() while we were waiting
  // for the local address.
  if (state() != State::kInitiating) {
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

  // HCI_LE_Set_Scan_Parameters
  auto scan_params_command = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanParametersCommandWriter>(
      hci_spec::kLESetScanParameters);
  auto scan_params = scan_params_command.view_t();
  scan_params.le_scan_type().Write(
      options.active ? pw::bluetooth::emboss::LEScanType::ACTIVE
                     : pw::bluetooth::emboss::LEScanType::PASSIVE);
  scan_params.le_scan_interval().Write(options.interval);
  scan_params.le_scan_window().Write(options.window);
  scan_params.scanning_filter_policy().Write(options.filter_policy);

  if (local_address.type() == DeviceAddress::Type::kLERandom) {
    scan_params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::RANDOM);
  } else {
    scan_params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::PUBLIC);
  }
  hci_cmd_runner()->QueueCommand(std::move(scan_params_command));

  // HCI_LE_Set_Scan_Enable
  auto scan_enable_command = EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanEnableCommandWriter>(
      hci_spec::kLESetScanEnable);
  auto enable_params = scan_enable_command.view_t();
  enable_params.le_scan_enable().Write(
      pw::bluetooth::emboss::GenericEnableParam::ENABLE);
  enable_params.filter_duplicates().Write(
      options.filter_duplicates
          ? pw::bluetooth::emboss::GenericEnableParam::ENABLE
          : pw::bluetooth::emboss::GenericEnableParam::DISABLE);

  hci_cmd_runner()->QueueCommand(std::move(scan_enable_command));
  hci_cmd_runner()->RunCommands(
      [this, period = options.period](Result<> status) {
        BT_DEBUG_ASSERT(scan_cb_);
        BT_DEBUG_ASSERT(state() == State::kInitiating);

        if (status.is_error()) {
          if (status == ToResult(HostError::kCanceled)) {
            bt_log(DEBUG, "hci-le", "scan canceled");
            return;
          }

          auto cb = std::move(scan_cb_);

          BT_DEBUG_ASSERT(!scan_cb_);
          set_state(State::kIdle);

          bt_log(ERROR, "hci-le", "failed to start scan: %s", bt_str(status));
          cb(ScanStatus::kFailed);
          return;
        }

        // Schedule the timeout.
        if (period != kPeriodInfinite) {
          scan_timeout_task_.PostAfter(period);
        }

        if (active_scan_requested()) {
          set_state(State::kActiveScanning);
          scan_cb_(ScanStatus::kActive);
        } else {
          set_state(State::kPassiveScanning);
          scan_cb_(ScanStatus::kPassive);
        }
      });
}

bool LegacyLowEnergyScanner::StopScan() {
  if (state() == State::kStopping || state() == State::kIdle) {
    bt_log(DEBUG,
           "hci-le",
           "cannot stop scan while in state: %s",
           ScanStateToString(state()).c_str());
    return false;
  }

  // Scan is either being initiated or already running. Cancel any in-flight HCI
  // command sequence.
  if (!hci_cmd_runner()->IsReady())
    hci_cmd_runner()->Cancel();

  // We'll tell the controller to stop scanning even if it is not (this is OK
  // because the command will have no effect; see Core Spec v5.0, Vol 2, Part E,
  // Section 7.8.11, paragraph 4).
  StopScanInternal(true);

  return true;
}

void LegacyLowEnergyScanner::StopScanInternal(bool stopped) {
  BT_DEBUG_ASSERT(scan_cb_);

  scan_timeout_task_.Cancel();
  set_state(State::kStopping);

  // Notify any pending scan results unless the scan was terminated by the user.
  if (!stopped) {
    for (auto& result : pending_results_) {
      auto& pending = result.second;
      NotifyPeerFound(pending->result(), pending->data());
    }
  }

  // Either way clear all results from the previous scan period.
  pending_results_.clear();

  BT_DEBUG_ASSERT(hci_cmd_runner()->IsReady());

  // Tell the controller to stop scanning.
  auto command = EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanEnableCommandWriter>(
      hci_spec::kLESetScanEnable);
  auto enable_params = command.view_t();
  enable_params.le_scan_enable().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  enable_params.filter_duplicates().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);

  hci_cmd_runner()->QueueCommand(std::move(command));
  hci_cmd_runner()->RunCommands([this, stopped](Result<> status) {
    BT_DEBUG_ASSERT(scan_cb_);
    BT_DEBUG_ASSERT(state() == State::kStopping);

    bt_is_error(
        status, WARN, "hci-le", "failed to stop scan: %s", bt_str(status));
    // Something went wrong but there isn't really a meaningful way to recover,
    // so we just fall through and notify the caller with ScanStatus::kFailed
    // instead.

    auto cb = std::move(scan_cb_);
    set_state(State::kIdle);

    cb(status.is_error()
           ? ScanStatus::kFailed
           : (stopped ? ScanStatus::kStopped : ScanStatus::kComplete));
  });
}

CommandChannel::EventCallbackResult
LegacyLowEnergyScanner::OnAdvertisingReportEvent(const EventPacket& event) {
  bt_log(TRACE, "hci-le", "received advertising report");

  // Drop the event if not requested to scan.
  if (!IsScanning()) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  AdvertisingReportParser parser(event);
  const hci_spec::LEAdvertisingReportData* report;
  int8_t rssi;
  while (parser.GetNextReport(&report, &rssi)) {
    bool needs_scan_rsp = false;
    bool connectable = false;
    bool directed = false;
    switch (report->event_type) {
      case hci_spec::LEAdvertisingEventType::kAdvDirectInd:
        directed = true;
        break;
      case hci_spec::LEAdvertisingEventType::kAdvInd:
        connectable = true;
        [[fallthrough]];
      case hci_spec::LEAdvertisingEventType::kAdvScanInd:
        if (IsActiveScanning()) {
          needs_scan_rsp = true;
        }
        break;
      case hci_spec::LEAdvertisingEventType::kScanRsp:
        if (IsActiveScanning()) {
          HandleScanResponse(*report, rssi);
        }
        continue;
      default:
        break;
    }

    if (report->length_data > hci_spec::kMaxLEAdvertisingDataLength) {
      bt_log(WARN, "hci-le", "advertising data too long! Ignoring");
      continue;
    }

    DeviceAddress address;
    bool resolved;
    if (!hci::DeviceAddressFromAdvReport(*report, &address, &resolved)) {
      continue;
    }

    LowEnergyScanResult result{address, resolved, connectable, rssi};
    if (directed) {
      delegate()->OnDirectedAdvertisement(result);
      continue;
    }

    if (!needs_scan_rsp) {
      NotifyPeerFound(result, BufferView(report->data, report->length_data));
      continue;
    }

    pending_results_[address] = std::make_unique<PendingScanResult>(
        result,
        BufferView(report->data, report->length_data),
        scan_response_timeout_,
        [this, address] { OnScanResponseTimeout(address); },
        pw_dispatcher());
  }
  return CommandChannel::EventCallbackResult::kContinue;
}

void LegacyLowEnergyScanner::HandleScanResponse(
    const hci_spec::LEAdvertisingReportData& report, int8_t rssi) {
  DeviceAddress address;
  bool resolved;
  if (!hci::DeviceAddressFromAdvReport(report, &address, &resolved))
    return;

  auto iter = pending_results_.find(address);
  if (iter == pending_results_.end()) {
    bt_log(TRACE, "hci-le", "dropping unmatched scan response");
    return;
  }

  if (report.length_data > hci_spec::kMaxLEAdvertisingDataLength) {
    bt_log(WARN, "hci-le", "scan response too long! Ignoring");
    return;
  }
  auto& pending = iter->second;
  BT_DEBUG_ASSERT(address == pending->result().address);

  // Append the scan response to the pending advertising data.
  pending->Append(BufferView(report.data, report.length_data));

  // Update the newest RSSI and address resolution information.
  pending->set_rssi(rssi);
  pending->set_resolved(resolved);

  NotifyPeerFound(pending->result(), pending->data());

  // If scan is stopped in notify callback, iter may be invalidated.
  iter = pending_results_.find(address);
  if (iter != pending_results_.end()) {
    pending_results_.erase(iter);
  }
}

void LegacyLowEnergyScanner::NotifyPeerFound(const LowEnergyScanResult& result,
                                             const ByteBuffer& data) {
  delegate()->OnPeerFound(result, data);
}

void LegacyLowEnergyScanner::OnScanPeriodComplete() {
  if (IsScanning()) {
    StopScanInternal(false);
  }
}

void LegacyLowEnergyScanner::OnScanResponseTimeout(
    const DeviceAddress& address) {
  bt_log(
      TRACE, "hci-le", "scan response timeout expired for %s", bt_str(address));

  auto iter = pending_results_.find(address);
  if (iter == pending_results_.end()) {
    bt_log(DEBUG,
           "hci-le",
           "timeout expired for unknown address: %s",
           bt_str(address));
    return;
  }

  auto pending = std::move(iter->second);
  pending_results_.erase(iter);
  NotifyPeerFound(pending->result(), pending->data());
}

}  // namespace bt::hci
