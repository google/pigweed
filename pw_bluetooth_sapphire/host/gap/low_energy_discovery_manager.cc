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

#include "pw_bluetooth_sapphire/internal/host/gap/low_energy_discovery_manager.h"

#include <lib/fit/function.h>
#include <pw_assert/check.h>

#include <algorithm>
#include <vector>

#include "pw_bluetooth_sapphire/internal/host/gap/peer.h"
#include "pw_bluetooth_sapphire/internal/host/gap/peer_cache.h"
#include "pw_bluetooth_sapphire/internal/host/hci/discovery_filter.h"
#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_scanner.h"

namespace bt::gap {

constexpr uint16_t kLEActiveScanInterval = 80;  // 50ms
constexpr uint16_t kLEActiveScanWindow = 24;    // 15ms
constexpr uint16_t kLEPassiveScanInterval = kLEScanSlowInterval1;
constexpr uint16_t kLEPassiveScanWindow = kLEScanSlowWindow1;

const char* kInspectPausedCountPropertyName = "paused";
const char* kInspectStatePropertyName = "state";
const char* kInspectFailedCountPropertyName = "failed_count";
const char* kInspectScanIntervalPropertyName = "scan_interval_ms";
const char* kInspectScanWindowPropertyName = "scan_window_ms";

LowEnergyDiscoverySession::LowEnergyDiscoverySession(
    uint16_t scan_id,
    bool active,
    std::vector<hci::DiscoveryFilter> filters,
    PeerCache& peer_cache,
    pw::async::Dispatcher& dispatcher,
    fit::function<void(LowEnergyDiscoverySession*)> on_stop_cb,
    fit::function<const std::unordered_set<PeerId>&()> cached_scan_results_fn)
    : WeakSelf(this),
      scan_id_(scan_id),
      active_(active),
      filters_(std::move(filters)),
      peer_cache_(peer_cache),
      heap_dispatcher_(dispatcher),
      on_stop_cb_(std::move(on_stop_cb)),
      cached_scan_results_fn_(std::move(cached_scan_results_fn)) {}

LowEnergyDiscoverySession::~LowEnergyDiscoverySession() {
  if (alive_ && on_stop_cb_) {
    on_stop_cb_(this);
  }
}

void LowEnergyDiscoverySession::SetResultCallback(PeerFoundFunction callback) {
  if (!alive_) {
    return;
  }
  peer_found_fn_ = std::move(callback);

  // Post NotifyDiscoveryResult(), which calls peer_found_fn_, to avoid client
  // bugs (e.g. deadlock) when peer_found_fn_ is called in SetResultCallback().
  pw::Status post_status = heap_dispatcher_.Post([self = GetWeakPtr()](
                                                     pw::async::Context,
                                                     pw::Status status) {
    if (!status.ok() || !self.is_alive()) {
      return;
    }
    for (PeerId cached_peer_id : self->cached_scan_results_fn_()) {
      auto peer = self->peer_cache_.FindById(cached_peer_id);
      // Ignore peers that have since been removed from the peer cache.
      if (!peer) {
        bt_log(
            TRACE,
            "gap",
            "Ignoring cached scan result for peer %s missing from peer cache",
            bt_str(cached_peer_id));
        continue;
      }
      self->NotifyDiscoveryResult(*peer);
    }
  });
  PW_CHECK(post_status.ok());
}

void LowEnergyDiscoverySession::NotifyDiscoveryResult(const Peer& peer) const {
  PW_CHECK(peer.le());

  if (!alive_ || !peer_found_fn_) {
    return;
  }

  if (filters_.empty()) {
    peer_found_fn_(peer);
    return;
  }

  if (std::any_of(filters_.begin(),
                  filters_.end(),
                  [&peer](const hci::DiscoveryFilter& filter) {
                    return filter.MatchLowEnergyResult(
                        peer.le()->parsed_advertising_data(),
                        peer.connectable(),
                        peer.rssi());
                  })) {
    peer_found_fn_(peer);
  }
}

void LowEnergyDiscoverySession::NotifyError() {
  alive_ = false;
  if (error_cb_) {
    error_cb_();
  }
}

void LowEnergyDiscoverySession::Stop() {
  PW_DCHECK(alive_);
  on_stop_cb_(this);
  alive_ = false;
}

LowEnergyDiscoveryManager::LowEnergyDiscoveryManager(
    hci::LowEnergyScanner* scanner,
    PeerCache* peer_cache,
    const hci::LowEnergyScanner::PacketFilterConfig& packet_filter_config,
    pw::async::Dispatcher& dispatcher)
    : WeakSelf(this),
      dispatcher_(dispatcher),
      state_(State::kIdle, StateToString),
      peer_cache_(peer_cache),
      packet_filter_config_(packet_filter_config),
      paused_count_(0),
      scanner_(scanner) {
  PW_DCHECK(peer_cache_);
  PW_DCHECK(scanner_);

  scanner_->set_delegate(this);
}

LowEnergyDiscoveryManager::~LowEnergyDiscoveryManager() {
  scanner_->set_delegate(nullptr);

  DeactivateAndNotifySessions();
}

void LowEnergyDiscoveryManager::StartDiscovery(
    bool active,
    std::vector<hci::DiscoveryFilter> discovery_filters,
    SessionCallback callback) {
  PW_CHECK(callback);
  bt_log(INFO, "gap-le", "start %s discovery", active ? "active" : "passive");

  // If a request to start or stop is currently pending then this one will
  // become pending until the HCI request completes. This does NOT include
  // the state in which we are stopping and restarting scan in between scan
  // periods, in which case session_ will not be empty.
  //
  // If the scan needs to be upgraded to an active scan, it will be handled
  // in OnScanStatus() when the HCI request completes.
  if (!pending_.empty() ||
      (scanner_->state() == hci::LowEnergyScanner::State::kStopping &&
       sessions_.empty())) {
    PW_CHECK(!scanner_->IsScanning());
    pending_.push_back(DiscoveryRequest{.active = active,
                                        .filters = std::move(discovery_filters),
                                        .callback = std::move(callback)});
    return;
  }

  // If a peer scan is already in progress, then the request succeeds (this
  // includes the state in which we are stopping and restarting scan in
  // between scan periods).
  if (!sessions_.empty()) {
    if (active) {
      // If this is the first active session, stop scanning and wait for
      // OnScanStatus() to initiate active scan.
      if (!std::any_of(sessions_.begin(), sessions_.end(), [](auto s) {
            return s->active();
          })) {
        StopScan();
      }
    }

    auto session = AddSession(active, std::move(discovery_filters));
    // Post the callback instead of calling it synchronously to avoid bugs
    // caused by client code not expecting this.
    (void)heap_dispatcher_.Post(
        [cb = std::move(callback), discovery_session = std::move(session)](
            pw::async::Context /*ctx*/, pw::Status status) mutable {
          if (status.ok()) {
            cb(std::move(discovery_session));
          }
        });
    return;
  }

  pending_.push_back({.active = active,
                      .filters = std::move(discovery_filters),
                      .callback = std::move(callback)});

  if (paused()) {
    return;
  }

  // If the scanner is not idle, it is starting/stopping, and the
  // appropriate scanning will be initiated in OnScanStatus().
  if (scanner_->IsIdle()) {
    StartScan(active);
  }
}

LowEnergyDiscoveryManager::PauseToken
LowEnergyDiscoveryManager::PauseDiscovery() {
  if (!paused()) {
    bt_log(TRACE, "gap-le", "Pausing discovery");
    StopScan();
  }

  paused_count_.Set(*paused_count_ + 1);

  return PauseToken([this, self = GetWeakPtr()]() {
    if (!self.is_alive()) {
      return;
    }

    PW_CHECK(paused());
    paused_count_.Set(*paused_count_ - 1);
    if (*paused_count_ == 0) {
      ResumeDiscovery();
    }
  });
}

bool LowEnergyDiscoveryManager::discovering() const {
  return std::any_of(
      sessions_.begin(), sessions_.end(), [](auto& s) { return s->active(); });
}

void LowEnergyDiscoveryManager::AttachInspect(inspect::Node& parent,
                                              std::string name) {
  inspect_.node = parent.CreateChild(name);
  paused_count_.AttachInspect(inspect_.node, kInspectPausedCountPropertyName);
  state_.AttachInspect(inspect_.node, kInspectStatePropertyName);
  inspect_.failed_count =
      inspect_.node.CreateUint(kInspectFailedCountPropertyName, 0);
  inspect_.scan_interval_ms =
      inspect_.node.CreateDouble(kInspectScanIntervalPropertyName, 0);
  inspect_.scan_window_ms =
      inspect_.node.CreateDouble(kInspectScanWindowPropertyName, 0);
}

std::string LowEnergyDiscoveryManager::StateToString(State state) {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kStarting:
      return "Starting";
    case State::kActive:
      return "Active";
    case State::kPassive:
      return "Passive";
    case State::kStopping:
      return "Stopping";
  }
}

std::unique_ptr<LowEnergyDiscoverySession>
LowEnergyDiscoveryManager::AddSession(
    bool active, std::vector<hci::DiscoveryFilter> discovery_filters) {
  auto on_stop_cb = [this](LowEnergyDiscoverySession* session_to_remove) {
    RemoveSession(session_to_remove);
  };
  auto cached_scan_results_fn =
      [this]() -> const decltype(cached_scan_results_)& {
    return this->cached_scan_results_;
  };
  auto session = std::make_unique<LowEnergyDiscoverySession>(
      next_scan_id_++,
      active,
      std::move(discovery_filters),
      *peer_cache_,
      dispatcher_,
      std::move(on_stop_cb),
      std::move(cached_scan_results_fn));
  sessions_.push_back(session.get());
  return session;
}

void LowEnergyDiscoveryManager::RemoveSession(
    LowEnergyDiscoverySession* session) {
  PW_CHECK(session);

  // Only alive sessions are allowed to call this method. If there is at
  // least one alive session object out there, then we MUST be scanning.
  PW_CHECK(session->alive());

  auto iter = std::find(sessions_.begin(), sessions_.end(), session);
  PW_CHECK(iter != sessions_.end());

  bool active = session->active();

  sessions_.erase(iter);

  bool last_active =
      active && std::none_of(sessions_.begin(), sessions_.end(), [](auto& s) {
        return s->active();
      });

  // Stop scanning if the session count has dropped to zero or the scan type
  // needs to be downgraded to passive.
  if (sessions_.empty() || last_active) {
    bt_log(TRACE,
           "gap-le",
           "Last %sdiscovery session removed, stopping scan (sessions: %zu)",
           last_active ? "active " : "",
           sessions_.size());
    StopScan();
    return;
  }
}

void LowEnergyDiscoveryManager::OnPeerFound(
    const hci::LowEnergyScanResult& result) {
  bt_log(DEBUG,
         "gap-le",
         "peer found (address: %s, connectable: %d)",
         bt_str(result.address()),
         result.connectable());

  auto peer = peer_cache_->FindByAddress(result.address());
  if (peer && peer->connectable() && peer->le() && connectable_cb_) {
    bt_log(TRACE,
           "gap-le",
           "found connectable peer (id: %s)",
           bt_str(peer->identifier()));
    connectable_cb_(peer);
  }

  // Don't notify sessions of unknown LE peers during passive scan.
  if (scanner_->IsPassiveScanning() && (!peer || !peer->le())) {
    return;
  }

  // Create a new entry if we found the device during general discovery.
  if (!peer) {
    peer = peer_cache_->NewPeer(result.address(), result.connectable());
    PW_CHECK(peer);
  } else if (!peer->connectable() && result.connectable()) {
    bt_log(DEBUG,
           "gap-le",
           "received connectable advertisement from previously "
           "non-connectable "
           "peer (address: %s, "
           "peer: %s)",
           bt_str(result.address()),
           bt_str(peer->identifier()));
    peer->set_connectable(true);
  }

  peer->MutLe().SetAdvertisingData(
      result.rssi(), result.data(), dispatcher_.now());

  cached_scan_results_.insert(peer->identifier());

  for (auto iter = sessions_.begin(); iter != sessions_.end();) {
    // The session may be erased by the result handler, so we need to get
    // the next iterator before iter is invalidated.
    auto next = std::next(iter);
    auto session = *iter;
    session->NotifyDiscoveryResult(*peer);
    iter = next;
  }
}

void LowEnergyDiscoveryManager::OnDirectedAdvertisement(
    const hci::LowEnergyScanResult& result) {
  bt_log(TRACE,
         "gap-le",
         "Received directed advertisement (address: %s, %s)",
         result.address().ToString().c_str(),
         (result.resolved() ? "resolved" : "not resolved"));

  auto peer = peer_cache_->FindByAddress(result.address());
  if (!peer) {
    bt_log(DEBUG,
           "gap-le",
           "ignoring connection request from unknown peripheral: %s",
           result.address().ToString().c_str());
    return;
  }

  if (!peer->le()) {
    bt_log(DEBUG,
           "gap-le",
           "rejecting connection request from non-LE peripheral: %s",
           result.address().ToString().c_str());
    return;
  }

  if (peer->connectable() && connectable_cb_) {
    connectable_cb_(peer);
  }

  // Only notify passive sessions.
  for (auto iter = sessions_.begin(); iter != sessions_.end();) {
    // The session may be erased by the result handler, so we need to get
    // the next iterator before iter is invalidated.
    auto next = std::next(iter);
    auto session = *iter;
    if (!session->active()) {
      session->NotifyDiscoveryResult(*peer);
    }
    iter = next;
  }
}

void LowEnergyDiscoveryManager::OnScanStatus(
    hci::LowEnergyScanner::ScanStatus status) {
  switch (status) {
    case hci::LowEnergyScanner::ScanStatus::kFailed:
      OnScanFailed();
      return;
    case hci::LowEnergyScanner::ScanStatus::kPassive:
      OnPassiveScanStarted();
      return;
    case hci::LowEnergyScanner::ScanStatus::kActive:
      OnActiveScanStarted();
      return;
    case hci::LowEnergyScanner::ScanStatus::kStopped:
      OnScanStopped();
      return;
    case hci::LowEnergyScanner::ScanStatus::kComplete:
      OnScanComplete();
      return;
  }
}

void LowEnergyDiscoveryManager::OnScanFailed() {
  bt_log(ERROR, "gap-le", "failed to initiate scan!");

  inspect_.failed_count.Add(1);
  DeactivateAndNotifySessions();

  // Report failure on all currently pending requests. If any of the
  // callbacks issue a retry the new requests will get re-queued and
  // notified of failure in the same loop here.
  while (!pending_.empty()) {
    auto request = std::move(pending_.back());
    pending_.pop_back();
    request.callback(nullptr);
  }

  state_.Set(State::kIdle);
}

void LowEnergyDiscoveryManager::OnPassiveScanStarted() {
  bt_log(TRACE, "gap-le", "passive scan started");

  state_.Set(State::kPassive);

  // Stop the passive scan if an active scan was requested while the scan
  // was starting. The active scan will start in OnScanStopped() once the
  // passive scan stops.
  if (std::any_of(sessions_.begin(),
                  sessions_.end(),
                  [](auto& s) { return s->active(); }) ||
      std::any_of(
          pending_.begin(), pending_.end(), [](auto& p) { return p.active; })) {
    bt_log(TRACE,
           "gap-le",
           "active scan requested while passive scan was starting");
    StopScan();
    return;
  }

  NotifyPending();
}

void LowEnergyDiscoveryManager::OnActiveScanStarted() {
  bt_log(TRACE, "gap-le", "active scan started");
  state_.Set(State::kActive);
  NotifyPending();
}

void LowEnergyDiscoveryManager::OnScanStopped() {
  bt_log(DEBUG,
         "gap-le",
         "stopped scanning (paused: %d, pending: %zu, sessions: %zu)",
         paused(),
         pending_.size(),
         sessions_.size());

  state_.Set(State::kIdle);
  cached_scan_results_.clear();

  if (paused()) {
    return;
  }

  if (!sessions_.empty()) {
    bt_log(DEBUG, "gap-le", "initiating scanning");
    bool active = std::any_of(sessions_.begin(), sessions_.end(), [](auto& s) {
      return s->active();
    });
    StartScan(active);
    return;
  }

  // Some clients might have requested to start scanning while we were
  // waiting for it to stop. Restart scanning if that is the case.
  if (!pending_.empty()) {
    bt_log(DEBUG, "gap-le", "initiating scanning");
    bool active = std::any_of(
        pending_.begin(), pending_.end(), [](auto& p) { return p.active; });
    StartScan(active);
    return;
  }
}

void LowEnergyDiscoveryManager::OnScanComplete() {
  bt_log(TRACE, "gap-le", "end of scan period");

  state_.Set(State::kIdle);
  cached_scan_results_.clear();

  if (paused()) {
    return;
  }

  // If |sessions_| is empty this is because sessions were stopped while the
  // scanner was shutting down after the end of the scan period. Restart the
  // scan as long as clients are waiting for it.
  ResumeDiscovery();
}

void LowEnergyDiscoveryManager::NotifyPending() {
  // Create and register all sessions before notifying the clients. We do
  // this so that the reference count is incremented for all new sessions
  // before the callbacks execute, to prevent a potential case in which a
  // callback stops its session immediately which could cause the reference
  // count to drop the zero before all clients receive their session object.
  if (!pending_.empty()) {
    size_t count = pending_.size();
    std::vector<std::unique_ptr<LowEnergyDiscoverySession>> new_sessions(count);
    std::generate(
        new_sessions.begin(), new_sessions.end(), [this, i = 0]() mutable {
          bool active = pending_[i].active;
          std::vector<hci::DiscoveryFilter> filters =
              std::move(pending_[i].filters);
          i++;
          return AddSession(active, filters);
        });

    for (size_t i = count - 1; i < count; i--) {
      auto cb = std::move(pending_.back().callback);
      pending_.pop_back();
      cb(std::move(new_sessions[i]));
    }
  }
  PW_CHECK(pending_.empty());
}

void LowEnergyDiscoveryManager::StartScan(bool active) {
  auto cb = [self = GetWeakPtr()](auto status) {
    if (self.is_alive())
      self->OnScanStatus(status);
  };

  // TODO(armansito): A client that is interested in scanning nearby beacons
  // and calculating proximity based on RSSI changes may want to disable
  // duplicate filtering. We generally shouldn't allow this unless a client
  // has the capability for it. Processing all HCI events containing
  // advertising reports will both generate a lot of bus traffic and
  // performing duplicate filtering on the host will take away CPU cycles
  // from other things. It's a valid use case but needs proper management.
  // For now we always make the controller filter duplicate reports.
  hci::LowEnergyScanner::ScanOptions options{
      .active = active,
      .filter_duplicates = true,
      .filter_policy =
          pw::bluetooth::emboss::LEScanFilterPolicy::BASIC_UNFILTERED,
      .period = scan_period_,
      .scan_response_timeout = kLEScanResponseTimeout,
  };

  // See Vol 3, Part C, 9.3.11 "Connection Establishment Timing Parameters".
  if (active) {
    options.interval = kLEActiveScanInterval;
    options.window = kLEActiveScanWindow;
  } else {
    options.interval = kLEPassiveScanInterval;
    options.window = kLEPassiveScanWindow;
    // TODO(armansito): Use the controller filter accept policy to filter
    // advertisements.
  }

  // Since we use duplicate filtering, we stop and start the scan
  // periodically to re-process advertisements. We use the minimum required
  // scan period for general discovery (by default; |scan_period_| can be
  // modified, e.g. by unit tests).
  state_.Set(State::kStarting);
  scanner_->StartScan(options, std::move(cb));

  inspect_.scan_interval_ms.Set(HciScanIntervalToMs(options.interval));
  inspect_.scan_window_ms.Set(HciScanWindowToMs(options.window));
}

void LowEnergyDiscoveryManager::StopScan() {
  state_.Set(State::kStopping);
  scanner_->StopScan();
}

void LowEnergyDiscoveryManager::ResumeDiscovery() {
  PW_CHECK(!paused());

  if (!scanner_->IsIdle()) {
    bt_log(TRACE, "gap-le", "attempt to resume discovery when it is not idle");
    return;
  }

  if (!sessions_.empty()) {
    bt_log(TRACE, "gap-le", "resuming scan");
    bool active = std::any_of(sessions_.begin(), sessions_.end(), [](auto& s) {
      return s->active();
    });
    StartScan(active);
    return;
  }

  if (!pending_.empty()) {
    bt_log(TRACE, "gap-le", "starting scan");
    bool active = std::any_of(
        pending_.begin(), pending_.end(), [](auto& s) { return s.active; });
    StartScan(active);
    return;
  }
}

void LowEnergyDiscoveryManager::DeactivateAndNotifySessions() {
  // If there are any active sessions we invalidate by notifying of an
  // error.

  // We move the initial set and notify those, if any error callbacks create
  // additional sessions they will be added to pending_
  auto sessions = std::move(sessions_);
  for (const auto& session : sessions) {
    if (session->alive()) {
      session->NotifyError();
    }
  }

  // Due to the move, sessions_ should be empty before the loop and any
  // callbacks will add sessions to pending_ so it should be empty
  // afterwards as well.
  PW_CHECK(sessions_.empty());
}

}  // namespace bt::gap
