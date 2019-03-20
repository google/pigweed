// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_discovery_manager.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/legacy_low_energy_scanner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

#include "remote_device.h"
#include "remote_device_cache.h"

namespace bt {
namespace gap {

LowEnergyDiscoverySession::LowEnergyDiscoverySession(
    fxl::WeakPtr<LowEnergyDiscoveryManager> manager)
    : active_(true), manager_(manager) {
  ZX_DEBUG_ASSERT(manager_);
}

LowEnergyDiscoverySession::~LowEnergyDiscoverySession() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  if (active_)
    Stop();
}

void LowEnergyDiscoverySession::SetResultCallback(
    DeviceFoundCallback callback) {
  device_found_callback_ = std::move(callback);
  if (!manager_)
    return;
  for (DeviceId cached_device_id : manager_->cached_scan_results()) {
    auto device = manager_->device_cache()->FindDeviceById(cached_device_id);
    ZX_DEBUG_ASSERT(device);
    NotifyDiscoveryResult(*device);
  }
}

void LowEnergyDiscoverySession::Stop() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(active_);
  if (manager_) {
    manager_->RemoveSession(this);
  }
  active_ = false;
}

void LowEnergyDiscoverySession::NotifyDiscoveryResult(
    const RemoteDevice& device) const {
  ZX_DEBUG_ASSERT(device.le());
  if (device_found_callback_ &&
      filter_.MatchLowEnergyResult(device.le()->advertising_data(),
                                   device.connectable(), device.rssi())) {
    device_found_callback_(device);
  }
}

void LowEnergyDiscoverySession::NotifyError() {
  active_ = false;
  if (error_callback_)
    error_callback_();
}

LowEnergyDiscoveryManager::LowEnergyDiscoveryManager(
    Mode mode, fxl::RefPtr<hci::Transport> hci, RemoteDeviceCache* device_cache)
    : dispatcher_(async_get_default_dispatcher()),
      device_cache_(device_cache),
      background_scan_enabled_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(hci);
  ZX_DEBUG_ASSERT(dispatcher_);
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(device_cache_);

  // We currently do not support the Extended Advertising feature.
  ZX_DEBUG_ASSERT(mode == Mode::kLegacy);

  scanner_ =
      std::make_unique<hci::LegacyLowEnergyScanner>(this, hci, dispatcher_);
}

LowEnergyDiscoveryManager::~LowEnergyDiscoveryManager() {
  // TODO(armansito): Invalidate all known session objects here.
}

void LowEnergyDiscoveryManager::StartDiscovery(SessionCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(callback);
  bt_log(INFO, "gap-le", "start discovery");

  // If a request to start or stop is currently pending then this one will
  // become pending until the HCI request completes (this does NOT include the
  // state in which we are stopping and restarting scan in between scan
  // periods).
  if (!pending_.empty() ||
      (scanner_->state() == hci::LowEnergyScanner::State::kStopping &&
       sessions_.empty())) {
    ZX_DEBUG_ASSERT(!scanner_->IsScanning());
    pending_.push(std::move(callback));
    return;
  }

  // If a device scan is already in progress, then the request succeeds (this
  // includes the state in which we are stopping and restarting scan in between
  // scan periods).
  if (!sessions_.empty()) {
    // Invoke |callback| asynchronously.
    auto session = AddSession();
    async::PostTask(dispatcher_, [callback = std::move(callback),
                                  session = std::move(session)]() mutable {
      callback(std::move(session));
    });
    return;
  }

  pending_.push(std::move(callback));

  // If currently scanning in the background, stop it and wait for
  // OnScanStatus() to initiate the active scan. Otherwise, request an active
  // scan if the scanner is idle.
  if (scanner_->IsPassiveScanning()) {
    ZX_DEBUG_ASSERT(background_scan_enabled_);
    scanner_->StopScan();
  } else if (!background_scan_enabled_ && scanner_->IsIdle()) {
    StartActiveScan();
  }
}

void LowEnergyDiscoveryManager::EnableBackgroundScan(bool enable) {
  if (background_scan_enabled_ == enable) {
    bt_log(TRACE, "gap-le", "background scan already %s",
           (enable ? "enabled" : "disabled"));
    return;
  }

  background_scan_enabled_ = enable;

  // Do nothing if an active scan is in progress.
  if (!sessions_.empty() || !pending_.empty()) {
    return;
  }

  if (enable && scanner_->IsIdle()) {
    StartPassiveScan();
  } else if (!enable && scanner_->IsPassiveScanning()) {
    scanner_->StopScan();
  }
  // If neither condition is true, we'll apply a scan policy in OnScanStatus().
}

std::unique_ptr<LowEnergyDiscoverySession>
LowEnergyDiscoveryManager::AddSession() {
  // Cannot use make_unique here since LowEnergyDiscoverySession has a private
  // constructor.
  std::unique_ptr<LowEnergyDiscoverySession> session(
      new LowEnergyDiscoverySession(weak_ptr_factory_.GetWeakPtr()));
  ZX_DEBUG_ASSERT(sessions_.find(session.get()) == sessions_.end());
  sessions_.insert(session.get());
  return session;
}

void LowEnergyDiscoveryManager::RemoveSession(
    LowEnergyDiscoverySession* session) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(session);

  // Only active sessions are allowed to call this method. If there is at least
  // one active session object out there, then we MUST be scanning.
  ZX_DEBUG_ASSERT(session->active());

  ZX_DEBUG_ASSERT(sessions_.find(session) != sessions_.end());
  sessions_.erase(session);

  // Stop scanning if the session count has dropped to zero.
  if (sessions_.empty())
    scanner_->StopScan();
}

void LowEnergyDiscoveryManager::OnDeviceFound(
    const hci::LowEnergyScanResult& result, const common::ByteBuffer& data) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // Ignore regular scan results during a passive scan.
  if (scanner_->IsPassiveScanning()) {
    return;
  }

  auto device = device_cache_->FindDeviceByAddress(result.address);
  if (!device) {
    device = device_cache_->NewDevice(result.address, result.connectable);
  }
  device->MutLe().SetAdvertisingData(result.rssi, data);

  cached_scan_results_.insert(device->identifier());

  for (const auto& session : sessions_) {
    session->NotifyDiscoveryResult(*device);
  }
}

void LowEnergyDiscoveryManager::OnDirectedAdvertisement(
    const hci::LowEnergyScanResult& result) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // TODO(NET-1572): Resolve the address in the host if it is random and
  // |result.resolved| is false.
  bt_log(SPEW, "gap", "Received directed advertisement (address: %s, %s)",
         result.address.ToString().c_str(),
         (result.resolved ? "resolved" : "not resolved"));

  auto device = device_cache_->FindDeviceByAddress(result.address);
  if (!device) {
    bt_log(TRACE, "gap",
           "ignoring connection request from unknown peripheral: %s",
           result.address.ToString().c_str());
    return;
  }

  if (!device->le() || !device->le()->bonded()) {
    bt_log(TRACE, "gap",
           "rejecting connection request from unbonded peripheral: %s",
           result.address.ToString().c_str());
    return;
  }

  // TODO(armansito): We shouldn't always accept connection requests from all
  // bonded peripherals (e.g. if one is explicitly disconnected). Maybe add an
  // "auto_connect()" property to RemoteDevice?
  if (directed_conn_cb_) {
    directed_conn_cb_(device->identifier());
  }
}

void LowEnergyDiscoveryManager::OnScanStatus(
    hci::LowEnergyScanner::ScanStatus status) {
  switch (status) {
    case hci::LowEnergyScanner::ScanStatus::kFailed: {
      bt_log(ERROR, "gap-le", "failed to initiate scan!");

      // Clear all sessions.
      auto sessions = std::move(sessions_);
      for (auto& s : sessions) {
        s->NotifyError();
      }

      // Report failure on all currently pending requests. If any of the
      // callbacks issue a retry the new requests will get re-queued and
      // notified of failure in the same loop here.
      while (!pending_.empty()) {
        auto callback = std::move(pending_.front());
        pending_.pop();

        callback(nullptr);
      }
      return;
    }
    case hci::LowEnergyScanner::ScanStatus::kPassive:
      bt_log(SPEW, "gap-le", "passive scan started");

      // Stop the background scan if active scan was requested or background
      // scan was disabled while waiting for the scan to start. If an active
      // scan was requested then the active scan we'll start it once the passive
      // scan stops.
      if (!pending_.empty() || !background_scan_enabled_) {
        scanner_->StopScan();
      }
      return;
    case hci::LowEnergyScanner::ScanStatus::kActive:
      bt_log(SPEW, "gap-le", "active scan started");

      // Create and register all sessions before notifying the clients. We do
      // this so that the reference count is incremented for all new sessions
      // before the callbacks execute, to prevent a potential case in which a
      // callback stops its session immediately which could cause the reference
      // count to drop the zero before all clients receive their session object.
      if (!pending_.empty()) {
        size_t count = pending_.size();
        std::unique_ptr<LowEnergyDiscoverySession> new_sessions[count];
        std::generate(new_sessions, new_sessions + count,
                      [this] { return AddSession(); });
        for (size_t i = 0; i < count; i++) {
          auto callback = std::move(pending_.front());
          pending_.pop();

          callback(std::move(new_sessions[i]));
        }
      }
      ZX_DEBUG_ASSERT(pending_.empty());
      return;
    case hci::LowEnergyScanner::ScanStatus::kStopped:
      bt_log(TRACE, "gap-le", "stopped scanning");

      cached_scan_results_.clear();

      // Some clients might have requested to start scanning while we were
      // waiting for it to stop. Restart active scanning if that is the case.
      // Otherwise start a background scan, if enabled.
      if (!pending_.empty()) {
        bt_log(TRACE, "gap-le", "initiate active scan");
        StartActiveScan();
      } else if (background_scan_enabled_) {
        bt_log(TRACE, "gap-le", "initiate background scan");
        StartPassiveScan();
      }
      return;
    case hci::LowEnergyScanner::ScanStatus::kComplete:
      bt_log(SPEW, "gap-le", "end of scan period");
      cached_scan_results_.clear();

      // If |sessions_| is empty this is because sessions were stopped while the
      // scanner was shutting down after the end of the scan period. Restart the
      // scan as long as clients are waiting for it.
      if (!sessions_.empty() || !pending_.empty()) {
        bt_log(SPEW, "gap-le", "continuing periodic scan");
        StartActiveScan();
      } else if (background_scan_enabled_) {
        bt_log(SPEW, "gap-le", "continuing periodic background scan");
        StartPassiveScan();
      }
      return;
  }
}

void LowEnergyDiscoveryManager::StartScan(bool active) {
  auto cb = [self = weak_ptr_factory_.GetWeakPtr()](auto status) {
    if (self)
      self->OnScanStatus(status);
  };

  // TODO(armansito): A client that is interested in scanning nearby beacons and
  // calculating proximity based on RSSI changes may want to disable duplicate
  // filtering. We generally shouldn't allow this unless a client has the
  // capability for it. Processing all HCI events containing advertising reports
  // will both generate a lot of bus traffic and performing duplicate filtering
  // on the host will take away CPU cycles from other things. It's a valid use
  // case but needs proper management. For now we always make the controller
  // filter duplicate reports.

  // See Vol 3, Part C, 9.3.11 "Connection Establishment Timing Parameters".
  uint16_t interval, window;
  if (active) {
    interval = kLEScanFastInterval;
    window = kLEScanFastWindow;
  } else {
    interval = kLEScanSlowInterval1;
    window = kLEScanSlowWindow1;
    // TODO(armansito): Use the controller whitelist to filter advertisements.
  }

  // Since we use duplicate filtering, we stop and start the scan periodically
  // to re-process advertisements. We use the minimum required scan period for
  // general discovery (by default; |scan_period_| can be modified, e.g. by unit
  // tests).
  scanner_->StartScan(active, interval, window, true /* filter_duplicates */,
                      hci::LEScanFilterPolicy::kNoWhiteList, scan_period_, cb);
}

}  // namespace gap
}  // namespace bt
