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

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_scanner.h"

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_report_parser.h"
#include "pw_bluetooth_sapphire/internal/host/hci/util.h"

#pragma clang diagnostic ignored "-Wswitch-enum"

namespace bt::hci {

LegacyLowEnergyScanner::LegacyLowEnergyScanner(
    LocalAddressDelegate* local_addr_delegate,
    Transport::WeakPtr transport,
    pw::async::Dispatcher& pw_dispatcher)
    : LowEnergyScanner(
          local_addr_delegate, std::move(transport), pw_dispatcher) {
  event_handler_id_ = hci()->command_channel()->AddLEMetaEventHandler(
      hci_spec::kLEAdvertisingReportSubeventCode,
      fit::bind_member<&LegacyLowEnergyScanner::OnAdvertisingReportEvent>(
          this));
}

LegacyLowEnergyScanner::~LegacyLowEnergyScanner() {
  if (hci()->command_channel()) {
    hci()->command_channel()->RemoveEventHandler(event_handler_id_);
  }
}

bool LegacyLowEnergyScanner::StartScan(const ScanOptions& options,
                                       ScanStatusCallback callback) {
  BT_ASSERT(options.interval >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.interval <= hci_spec::kLEScanIntervalMax);
  BT_ASSERT(options.window >= hci_spec::kLEScanIntervalMin);
  BT_ASSERT(options.window <= hci_spec::kLEScanIntervalMax);
  return LowEnergyScanner::StartScan(options, std::move(callback));
}

EmbossCommandPacket LegacyLowEnergyScanner::BuildSetScanParametersPacket(
    const DeviceAddress& local_address, const ScanOptions& options) {
  auto packet = hci::EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanParametersCommandWriter>(
      hci_spec::kLESetScanParameters);
  auto params = packet.view_t();

  params.le_scan_type().Write(pw::bluetooth::emboss::LEScanType::PASSIVE);
  if (options.active) {
    params.le_scan_type().Write(pw::bluetooth::emboss::LEScanType::ACTIVE);
  }

  params.le_scan_interval().Write(options.interval);
  params.le_scan_window().Write(options.window);
  params.scanning_filter_policy().Write(options.filter_policy);

  if (local_address.type() == DeviceAddress::Type::kLERandom) {
    params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::RANDOM);
  } else {
    params.own_address_type().Write(
        pw::bluetooth::emboss::LEOwnAddressType::PUBLIC);
  }

  return packet;
}

EmbossCommandPacket LegacyLowEnergyScanner::BuildEnablePacket(
    const ScanOptions& options,
    pw::bluetooth::emboss::GenericEnableParam enable) {
  auto packet = EmbossCommandPacket::New<
      pw::bluetooth::emboss::LESetScanEnableCommandWriter>(
      hci_spec::kLESetScanEnable);
  auto params = packet.view_t();
  params.le_scan_enable().Write(enable);

  params.filter_duplicates().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  if (options.filter_duplicates) {
    params.filter_duplicates().Write(
        pw::bluetooth::emboss::GenericEnableParam::ENABLE);
  }

  return packet;
}

CommandChannel::EventCallbackResult
LegacyLowEnergyScanner::OnAdvertisingReportEvent(const EventPacket& event) {
  bt_log(TRACE, "hci-le", "received advertising report");

  if (!IsScanning()) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  AdvertisingReportParser parser(event);
  const hci_spec::LEAdvertisingReportData* report;
  int8_t rssi;

  while (parser.GetNextReport(&report, &rssi)) {
    if (report->length_data > hci_spec::kMaxLEAdvertisingDataLength) {
      bt_log(WARN, "hci-le", "advertising data too long! Ignoring");
      continue;
    }

    DeviceAddress address;
    bool resolved;
    if (!hci::DeviceAddressFromAdvReport(*report, &address, &resolved)) {
      continue;
    }

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
          GetPendingResult(address)->AppendData(
              BufferView(report->data, report->length_data));
          LegacyLowEnergyScanner::HandleScanResponse(address, resolved, rssi);
        }
        continue;
      default:
        break;
    }

    LowEnergyScanResult result{.address = address,
                               .resolved = resolved,
                               .connectable = connectable,
                               .rssi = rssi};

    if (directed) {
      delegate()->OnDirectedAdvertisement(result);
      continue;
    }

    if (!needs_scan_rsp) {
      delegate()->OnPeerFound(result,
                              BufferView(report->data, report->length_data));
      continue;
    }

    BufferView report_data = BufferView(report->data, report->length_data);
    AddPendingResult(address, result, [this, address, resolved, rssi] {
      LegacyLowEnergyScanner::HandleScanResponse(address, resolved, rssi);
    });
    GetPendingResult(address)->AppendData(report_data);
  }
  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
