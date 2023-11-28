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

#pragma once
#include <memory>
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_scanner.h"

namespace bt::hci {

class LocalAddressDelegate;

// LegacyLowEnergyScanner implements the LowEnergyScanner interface for
// controllers that do not support the 5.0 Extended Advertising feature. This
// uses the legacy HCI LE scan commands and events:
//
//     - HCI_LE_Set_Scan_Parameters
//     - HCI_LE_Set_Scan_Enable
//     - HCI_LE_Advertising_Report event
class LegacyLowEnergyScanner final : public LowEnergyScanner {
 public:
  LegacyLowEnergyScanner(LocalAddressDelegate* local_addr_delegate,
                         Transport::WeakPtr transport,
                         pw::async::Dispatcher& pw_dispatcher);
  ~LegacyLowEnergyScanner() override;

  bool StartScan(const ScanOptions& options,
                 ScanStatusCallback callback) override;

 private:
  // Build the HCI command packet to set the scan parameters for the flavor of
  // low energy scanning being implemented.
  EmbossCommandPacket BuildSetScanParametersPacket(
      const DeviceAddress& local_address, const ScanOptions& options) override;

  // Build the HCI command packet to enable scanning for the flavor of low
  // energy scanning being implemented.
  EmbossCommandPacket BuildEnablePacket(
      const ScanOptions& options,
      pw::bluetooth::emboss::GenericEnableParam enable) override;

  // Event handler for HCI LE Advertising Report event.
  CommandChannel::EventCallbackResult OnAdvertisingReportEvent(
      const EventPacket& event);

  // Our event handler ID for the LE Advertising Report event.
  CommandChannel::EventHandlerId event_handler_id_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyScanner);
};

}  // namespace bt::hci
