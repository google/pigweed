// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_SCANNER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_SCANNER_H_

#include <lib/async/dispatcher.h>

#include <set>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

// Represents a discovered Bluetooth Low Energy peer.
struct LowEnergyScanResult {
  // The device address of the remote peer.
  DeviceAddress address;

  // True if |address| is a static or random identity address resolved by the controller.
  bool resolved = false;

  // True if this peer accepts connections. This is the case if this peer sent a connectable
  // advertising PDU.
  bool connectable = false;

  // The received signal strength of the advertisement packet corresponding to this peer.
  int8_t rssi = hci_spec::kRSSIInvalid;
};

// LowEnergyScanner manages Low Energy scan procedures that are used during general and limited
// discovery and connection establishment procedures. This is an abstract class that provides a
// common interface over 5.0 Extended Advertising and Legacy Advertising features.
//
// Instances of this class are expected to each as a singleton on a per-transport basis as multiple
// instances cannot accurately reflect the state of the controller while allowing simultaneous scan
// operations.
class LowEnergyScanner : public LocalAddressClient {
 public:
  // Value that can be passed to StartScan() to scan indefinitely.
  static constexpr pw::chrono::SystemClock::duration kPeriodInfinite =
      pw::chrono::SystemClock::duration::zero();

  enum class State {
    // No scan is currently being performed.
    kIdle,

    // A previously running scan is being stopped.
    kStopping,

    // A scan is being initiated.
    kInitiating,

    // An active scan is currently being performed.
    kActiveScanning,

    // A passive scan is currently being performed.
    kPassiveScanning,
  };

  enum class ScanStatus {
    // Reported when the scan could not be started.
    kFailed,

    // Reported when an active scan was started and is currently in progress.
    kActive,

    // Reported when a passive scan was started and is currently in progress.
    kPassive,

    // Called when the scan was terminated naturally at the end of the scan period.
    kComplete,

    // Called when the scan was terminated due to a call to StopScan().
    kStopped,
  };

  struct ScanOptions {
    // Perform an active scan if true. During an active scan, scannable advertisements are reported
    // alongside their corresponding scan response.
    bool active = false;

    // When enabled, the controller will filter out duplicate advertising reports. This means that
    // Delegate::OnPeerFound will be called only once per device address during the scan period.
    //
    // When disabled, Delegate::OnPeerFound will get called once for every observed advertisement
    // (depending on |filter_policy|).
    bool filter_duplicates = false;

    // Determines the type of filtering the controller should perform to limit the number of
    // advertising reports.
    pw::bluetooth::emboss::LEScanFilterPolicy filter_policy =
        pw::bluetooth::emboss::LEScanFilterPolicy::BASIC_UNFILTERED;

    // Determines the length of the software defined scan period. If the value is kPeriodInfinite,
    // then the scan will remain enabled until StopScan() gets called. For all other values, the
    // scan will be disabled after the duration expires.
    pw::chrono::SystemClock::duration period;

    // Maximum time duration during an active scan for which a scannable advertisement will be
    // stored and not reported to clients until a corresponding scan response is received.
    pw::chrono::SystemClock::duration scan_response_timeout;

    // Scan parameters.
    uint16_t interval = hci_spec::defaults::kLEScanInterval;
    uint16_t window = hci_spec::defaults::kLEScanWindow;
  };

  // Interface for receiving events related to Low Energy scan.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a peer is found. During a passive scan |data| contains the advertising data.
    // During an active scan |data| contains the combined advertising and scan response data (if the
    // peer is scannable).
    virtual void OnPeerFound(const LowEnergyScanResult& result, const ByteBuffer& data);

    // Called when a directed advertising report is received from the peer with the given address.
    virtual void OnDirectedAdvertisement(const LowEnergyScanResult& result);
  };

  LowEnergyScanner(Transport::WeakPtr hci, pw::async::Dispatcher& pw_dispatcher);
  ~LowEnergyScanner() override = default;

  // Returns the current Scan state.
  State state() const { return state_; }

  bool IsActiveScanning() const { return state() == State::kActiveScanning; }
  bool IsPassiveScanning() const { return state() == State::kPassiveScanning; }
  bool IsScanning() const { return IsActiveScanning() || IsPassiveScanning(); }
  bool IsInitiating() const { return state() == State::kInitiating; }

  // LocalAddressClient override:
  bool AllowsRandomAddressChange() const override {
    return !IsScanning() && hci_cmd_runner()->IsReady();
  }

  // True if no scan procedure is currently enabled.
  bool IsIdle() const { return state() == State::kIdle; }

  // Initiates a scan. This is an asynchronous operation that abides by the following rules:
  //
  //   - This method synchronously returns false if the procedure could not be started, e.g. because
  //   discovery is already in progress, or it is in the process of being stopped, or the controller
  //   does not support discovery, etc.
  //
  //   - Synchronously returns true if the procedure was initiated but the it is unknown whether or
  //   not the procedure has succeeded.
  //
  //   - |callback| is invoked asynchronously to report the status of the procedure. In the case of
  //   failure, |callback| will be invoked once to report the end of the procedure. In the case of
  //   success, |callback| will be invoked twice: the first time to report that the procedure has
  //   started, and a second time to report when the procedure ends, either due to a timeout or
  //   cancellation.
  //
  //   - |period| specifies (in milliseconds) the duration of the scan. If the special value of
  //   kPeriodInfinite is passed then scanning will continue indefinitely and must be explicitly
  //   stopped by calling StopScan(). Otherwise, the value must be non-zero.
  //
  // Once started, a scan can be terminated at any time by calling the StopScan() method. Otherwise,
  // an ongoing scan will terminate at the end of the scan period if a finite value for |period| was
  // provided.
  //
  // During an active scan, scannable advertisements are reported alongside their corresponding scan
  // response. Every scannable advertisement is stored and not reported until either
  //
  //   a) a scan response is received
  //   b) an implementation determined timeout period expires
  //   c) for periodic scans, when the scan period expires
  //
  // Since a passive scan involves no scan request/response, all advertisements are reported
  // immediately without waiting for a scan response.
  //
  // (For more information about passive and active scanning, see Core Spec. v5.2, Vol 6, Part
  // B, 4.4.3.1 and 4.4.3.2).
  using ScanStatusCallback = fit::function<void(ScanStatus)>;
  virtual bool StartScan(const ScanOptions& options, ScanStatusCallback callback) = 0;

  // Stops a previously started scan. Returns false if a scan is not in progress. Otherwise, cancels
  // any in progress scan procedure and returns true.
  virtual bool StopScan() = 0;

  // Assigns the delegate for scan events.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 protected:
  pw::async::Dispatcher& pw_dispatcher() { return pw_dispatcher_; }
  Transport::WeakPtr transport() const { return transport_; }
  SequentialCommandRunner* hci_cmd_runner() const { return hci_cmd_runner_.get(); }
  Delegate* delegate() const {
    BT_ASSERT(delegate_);
    return delegate_;
  }

  void set_state(State state) { state_ = state; }

  // Returns true if an active scan was most recently requested. This applies to the on-going scan
  // only if IsScanning() returns true.
  bool active_scan_requested() const { return active_scan_requested_; }
  void set_active_scan_requested(bool value) { active_scan_requested_ = value; }

 private:
  State state_;
  bool active_scan_requested_;

  Delegate* delegate_;  // weak

  pw::async::Dispatcher& pw_dispatcher_;

  // The HCI transport.
  Transport::WeakPtr transport_;

  // Command runner for all HCI commands sent out by implementations.
  std::unique_ptr<SequentialCommandRunner> hci_cmd_runner_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyScanner);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_SCANNER_H_
