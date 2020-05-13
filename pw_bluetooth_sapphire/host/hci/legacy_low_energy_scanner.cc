// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "legacy_low_energy_scanner.h"

#include <endian.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/advertising_report_parser.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"

namespace bt {

namespace hci {
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

  ZX_PANIC("invalid scanner state: %u", static_cast<unsigned int>(state));
  return "(unknown)";
}

}  // namespace

LegacyLowEnergyScanner::LegacyLowEnergyScanner(LocalAddressDelegate* local_addr_delegate,
                                               fxl::RefPtr<Transport> hci,
                                               async_dispatcher_t* dispatcher)
    : LowEnergyScanner(hci, dispatcher), local_addr_delegate_(local_addr_delegate) {
  ZX_DEBUG_ASSERT(local_addr_delegate_);
  event_handler_id_ = transport()->command_channel()->AddLEMetaEventHandler(
      kLEAdvertisingReportSubeventCode,
      fit::bind_member(this, &LegacyLowEnergyScanner::OnAdvertisingReportEvent),
      this->dispatcher());
  scan_timeout_task_.set_handler(
      fit::bind_member(this, &LegacyLowEnergyScanner::OnScanPeriodComplete));
}

LegacyLowEnergyScanner::~LegacyLowEnergyScanner() {
  transport()->command_channel()->RemoveEventHandler(event_handler_id_);
}

bool LegacyLowEnergyScanner::StartScan(bool active, uint16_t scan_interval, uint16_t scan_window,
                                       bool filter_duplicates, LEScanFilterPolicy filter_policy,
                                       zx::duration period, ScanStatusCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(period == kPeriodInfinite || period.get() > 0);
  ZX_DEBUG_ASSERT(scan_interval <= kLEScanIntervalMax && scan_interval >= kLEScanIntervalMin);
  ZX_DEBUG_ASSERT(scan_window <= kLEScanIntervalMax && scan_window >= kLEScanIntervalMin);
  ZX_DEBUG_ASSERT(scan_window < scan_interval);

  if (state() != State::kIdle) {
    bt_log(ERROR, "hci-le", "cannot start scan while in state: %s",
           ScanStateToString(state()).c_str());
    return false;
  }

  ZX_DEBUG_ASSERT(!scan_cb_);
  ZX_DEBUG_ASSERT(!scan_timeout_task_.is_pending());
  ZX_DEBUG_ASSERT(hci_cmd_runner()->IsReady());

  set_state(State::kInitiating);
  set_active_scan_requested(active);
  scan_cb_ = std::move(callback);

  // Obtain the local address type.
  local_addr_delegate_->EnsureLocalAddress(
      [this, active, scan_interval, scan_window, filter_duplicates, filter_policy, period,
       callback = std::move(callback)](const auto& address) mutable {
        StartScanInternal(address, active, scan_interval, scan_window, filter_duplicates,
                          filter_policy, period, std::move(callback));
      });

  return true;
}

void LegacyLowEnergyScanner::StartScanInternal(const DeviceAddress& local_address, bool active,
                                               uint16_t scan_interval, uint16_t scan_window,
                                               bool filter_duplicates,
                                               LEScanFilterPolicy filter_policy,
                                               zx::duration period, ScanStatusCallback callback) {
  // Check if the scan request was canceled by StopScan() while we were waiting
  // for the local address.
  if (state() != State::kInitiating) {
    bt_log(DEBUG, "hci-le", "scan request was canceled while obtaining local address");
    return;
  }

  bt_log(DEBUG, "hci-le", "requesting scan (%s, address: %s, interval: %#.4x, window: %#.4x)",
         (active ? "active" : "passive"), local_address.ToString().c_str(), scan_interval,
         scan_window);

  // HCI_LE_Set_Scan_Parameters
  auto command = CommandPacket::New(kLESetScanParameters, sizeof(LESetScanParametersCommandParams));
  auto scan_params = command->mutable_payload<LESetScanParametersCommandParams>();
  scan_params->scan_type = active ? LEScanType::kActive : LEScanType::kPassive;
  scan_params->scan_interval = htole16(scan_interval);
  scan_params->scan_window = htole16(scan_window);
  scan_params->filter_policy = filter_policy;

  if (local_address.type() == DeviceAddress::Type::kLERandom) {
    scan_params->own_address_type = LEOwnAddressType::kRandom;
  } else {
    scan_params->own_address_type = LEOwnAddressType::kPublic;
  }
  hci_cmd_runner()->QueueCommand(std::move(command));

  // HCI_LE_Set_Scan_Enable
  command = CommandPacket::New(kLESetScanEnable, sizeof(LESetScanEnableCommandParams));
  auto enable_params = command->mutable_payload<LESetScanEnableCommandParams>();
  enable_params->scanning_enabled = GenericEnableParam::kEnable;
  enable_params->filter_duplicates =
      filter_duplicates ? GenericEnableParam::kEnable : GenericEnableParam::kDisable;

  hci_cmd_runner()->QueueCommand(std::move(command));
  hci_cmd_runner()->RunCommands([this, period](Status status) {
    ZX_DEBUG_ASSERT(scan_cb_);
    ZX_DEBUG_ASSERT(state() == State::kInitiating);

    if (!status) {
      if (status.error() == HostError::kCanceled) {
        bt_log(DEBUG, "hci-le", "scan canceled");
        return;
      }

      auto cb = std::move(scan_cb_);

      ZX_DEBUG_ASSERT(!scan_cb_);
      set_state(State::kIdle);

      bt_log(ERROR, "hci-le", "failed to start scan: %s", status.ToString().c_str());
      cb(ScanStatus::kFailed);
      return;
    }

    // Schedule the timeout.
    if (period != kPeriodInfinite) {
      scan_timeout_task_.PostDelayed(dispatcher(), period);
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
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (state() == State::kStopping || state() == State::kIdle) {
    bt_log(DEBUG, "hci-le", "cannot stop scan while in state: %s",
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

void LegacyLowEnergyScanner::StopScanPeriodForTesting() {
  ZX_DEBUG_ASSERT(IsScanning());
  StopScanInternal(false);
}

void LegacyLowEnergyScanner::StopScanInternal(bool stopped) {
  ZX_DEBUG_ASSERT(scan_cb_);

  scan_timeout_task_.Cancel();
  set_state(State::kStopping);

  ZX_DEBUG_ASSERT(hci_cmd_runner()->IsReady());

  // Tell the controller to stop scanning.
  auto command = CommandPacket::New(kLESetScanEnable, sizeof(LESetScanEnableCommandParams));
  auto enable_params = command->mutable_payload<LESetScanEnableCommandParams>();
  enable_params->scanning_enabled = GenericEnableParam::kDisable;
  enable_params->filter_duplicates = GenericEnableParam::kDisable;

  hci_cmd_runner()->QueueCommand(std::move(command));
  hci_cmd_runner()->RunCommands([this, stopped](Status status) {
    ZX_DEBUG_ASSERT(scan_cb_);
    ZX_DEBUG_ASSERT(state() == State::kStopping);

    if (!status) {
      bt_log(WARN, "hci-le", "failed to stop scan: %s", status.ToString().c_str());
      // Something went wrong but there isn't really a meaningful way to
      // recover, so we just fall through and notify the caller with
      // ScanStatus::kFailed instead.
    }

    auto cb = std::move(scan_cb_);
    set_state(State::kIdle);

    cb(!status ? ScanStatus::kFailed : (stopped ? ScanStatus::kStopped : ScanStatus::kComplete));
  });
}

CommandChannel::EventCallbackResult LegacyLowEnergyScanner::OnAdvertisingReportEvent(
    const EventPacket& event) {
  bt_log(TRACE, "hci-le", "received advertising report");

  // Drop the event if not requested to scan.
  if (!IsScanning())
    return CommandChannel::EventCallbackResult::kContinue;

  AdvertisingReportParser parser(event);
  const LEAdvertisingReportData* report;
  int8_t rssi;
  while (parser.GetNextReport(&report, &rssi)) {
    bool is_scan_rsp = false;
    bool connectable = false;
    bool directed = false;
    switch (report->event_type) {
      case LEAdvertisingEventType::kAdvDirectInd:
        directed = true;
        break;
      case LEAdvertisingEventType::kAdvInd:
        connectable = true;
        break;
      case LEAdvertisingEventType::kAdvScanInd:
        break;
      case LEAdvertisingEventType::kScanRsp:
        is_scan_rsp = true;
        break;
      default:
        break;
    }

    if (report->length_data > kMaxLEAdvertisingDataLength) {
      bt_log(WARN, "hci-le", "advertising data too long! Ignoring");
      continue;
    }

    DeviceAddress address;
    bool resolved;
    if (!DeviceAddressFromAdvReport(*report, &address, &resolved))
      continue;

    LowEnergyScanResult result(address, resolved, connectable, is_scan_rsp, rssi);
    if (directed) {
      delegate()->OnDirectedAdvertisement(result);
    } else {
      delegate()->OnPeerFound(result, BufferView(report->data, report->length_data));
    }
  }
  return CommandChannel::EventCallbackResult::kContinue;
}

void LegacyLowEnergyScanner::OnScanPeriodComplete() {
  if (IsScanning()) {
    StopScanInternal(false);
  }
}

}  // namespace hci
}  // namespace bt
