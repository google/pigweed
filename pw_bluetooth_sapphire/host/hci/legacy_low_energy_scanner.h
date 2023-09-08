// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_

#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include <memory>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"

namespace bt::hci {

class LocalAddressDelegate;

// LegacyLowEnergyScanner implements the LowEnergyScanner interface for
// controllers that do not support the 5.0 Extended Advertising feature. This
// uses the legacy HCI LE scan commands and events:
//     - HCI_LE_Set_Scan_Parameters
//     - HCI_LE_Set_Scan_Enable
//     - HCI_LE_Advertising_Report event
class LegacyLowEnergyScanner : public LowEnergyScanner {
 public:
  LegacyLowEnergyScanner(LocalAddressDelegate* local_addr_delegate, Transport::WeakPtr hci,
                         pw::async::Dispatcher& pw_dispatcher);
  ~LegacyLowEnergyScanner() override;

  // LowEnergyScanner overrides:
  bool StartScan(const ScanOptions& options, ScanStatusCallback callback) override;
  bool StopScan() override;

 private:
  // This represents the data obtained for a scannable advertisement for which a scan response has
  // not yet been received. Clients are notified for scannable advertisement either when the
  // corresponding scan response is received or, otherwise, a timeout expires.
  class PendingScanResult {
   public:
    // |adv|: Initial advertising data payload.
    PendingScanResult(LowEnergyScanResult result, const ByteBuffer& adv,
                      pw::chrono::SystemClock::duration timeout, fit::closure timeout_handler,
                      pw::async::Dispatcher& dispatcher);

    // Return the contents of the data.
    BufferView data() const { return buffer_.view(0, data_size_); }

    const LowEnergyScanResult& result() const { return result_; }

    void set_rssi(int8_t rssi) { result_.rssi = rssi; }
    void set_resolved(bool resolved) { result_.resolved = resolved; }

    // Appends |data| to the end of the current contents.
    void Append(const ByteBuffer& data);

   private:
    LowEnergyScanResult result_;

    // The size of the data so far accumulated in |buffer_|.
    size_t data_size_ = 0u;

    // Buffer large enough to store both advertising and scan response payloads.
    StaticByteBuffer<hci_spec::kMaxLEAdvertisingDataLength * 2> buffer_;

    // Since not all scannable advertisements are always followed by a scan response, we report a
    // pending result if a scan response is not received within a timeout.
    SmartTask timeout_task_;
  };

  // Called by StartScan() after the local peer address has been obtained.
  void StartScanInternal(const DeviceAddress& local_address, const ScanOptions& options,
                         ScanStatusCallback callback);

  // Called by StopScan() and by the scan timeout handler set up by StartScan().
  void StopScanInternal(bool stopped);

  // Event handler for HCI LE Advertising Report event.
  CommandChannel::EventCallbackResult OnAdvertisingReportEvent(const EventPacket& event);

  // Called when a Scan Response is received during an active scan.
  void HandleScanResponse(const hci_spec::LEAdvertisingReportData& report, int8_t rssi);

  // Notifies observers of a peer that was found.
  void NotifyPeerFound(const LowEnergyScanResult& result, const ByteBuffer& data);

  // Called when the scan timeout task executes.
  void OnScanPeriodComplete();

  // Called when the scan response timeout expires for the given device address.
  void OnScanResponseTimeout(const DeviceAddress& address);

  // Used to obtain the local peer address type to use during scanning.
  LocalAddressDelegate* local_addr_delegate_;  // weak

  // Callback passed in to the most recently accepted call to StartScan();
  ScanStatusCallback scan_cb_;

  // The scan period timeout handler for the currently active scan session.
  SmartTask scan_timeout_task_{pw_dispatcher()};

  // Maximum time duration for which a scannable advertisement will be stored and not reported to
  // clients until a corresponding scan response is received.
  pw::chrono::SystemClock::duration scan_response_timeout_;

  // Our event handler ID for the LE Advertising Report event.
  CommandChannel::EventHandlerId event_handler_id_;

  // Scannable advertising events for which a Scan Response PDU has not been
  // received. This is accumulated during a discovery procedure and always
  // cleared at the end of the scan period.
  std::unordered_map<DeviceAddress, std::unique_ptr<PendingScanResult>> pending_results_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LegacyLowEnergyScanner);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LEGACY_LOW_ENERGY_SCANNER_H_
