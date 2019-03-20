// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_

#include <memory>
#include <unordered_map>

#include <lib/async/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/ref_counted.h"
#include "lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace hci {

// LegacyLowEnergyScanner implements the LowEnergyScanner interface for
// controllers that do not support the 5.0 Extended Advertising feature. This
// uses the legacy HCI LE device scan commands and events:
//     - HCI_LE_Set_Scan_Parameters
//     - HCI_LE_Set_Scan_Enable
//     - HCI_LE_Advertising_Report event
class LegacyLowEnergyScanner : public LowEnergyScanner {
 public:
  LegacyLowEnergyScanner(Delegate* delegate, fxl::RefPtr<Transport> hci,
                         async_dispatcher_t* dispatcher);
  ~LegacyLowEnergyScanner() override;

  // LowEnergyScanner overrides:
  bool StartScan(bool active, uint16_t scan_interval, uint16_t scan_window,
                 bool filter_duplicates, LEScanFilterPolicy filter_policy,
                 zx::duration period, ScanStatusCallback callback) override;
  bool StopScan() override;

  // Used by tests to directly end a scan period without relying on a timeout.
  void StopScanPeriodForTesting();

 private:
  struct PendingScanResult {
    LowEnergyScanResult result;

    // Make this large enough to store both advertising and scan response data
    // PDUs.
    size_t adv_data_len;
    common::StaticByteBuffer<kMaxLEAdvertisingDataLength * 2> data;
  };

  // Called by StopScan() and by the scan timeout handler set up by StartScan().
  void StopScanInternal(bool stopped);

  // Event handler for HCI LE Advertising Report event.
  void OnAdvertisingReportEvent(const EventPacket& event);

  // Called when a Scan Response is received during an active scan.
  void HandleScanResponse(const LEAdvertisingReportData& report, int8_t rssi);

  // Notifies observers of a device that was found.
  void NotifyDeviceFound(const LowEnergyScanResult& result,
                         const common::ByteBuffer& data);

  // Called when the scan timeout task executes.
  void OnScanPeriodComplete();

  // Callback passed in to the most recently accepted call to StartScan();
  ScanStatusCallback scan_cb_;

  // The scan period timeout handler for the currently active scan session.
  async::TaskClosure scan_timeout_task_;

  // Our event handler ID for the LE Advertising Report event.
  CommandChannel::EventHandlerId event_handler_id_;

  // Scannable advertising events for which a Scan Response PDU has not been
  // received. This is accumulated during a discovery procedure and always
  // cleared at the end of the scan period.
  std::unordered_map<common::DeviceAddress, PendingScanResult> pending_results_;
  fxl::ThreadChecker thread_checker_;

  FXL_DISALLOW_COPY_AND_ASSIGN(LegacyLowEnergyScanner);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_
