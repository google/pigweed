// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_DISCOVERY_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_DISCOVERY_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/defer.h>
#include <lib/sys/inspect/cpp/component.h>

#include <memory>
#include <queue>
#include <unordered_set>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/discovery_filter.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_scanner.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace hci {
class LowEnergyScanner;
class Transport;
}  // namespace hci

namespace gap {

class LowEnergyDiscoveryManager;
class Peer;
class PeerCache;

// LowEnergyDiscoveryManager implements GAP LE central/observer role
// discovery procedures. This class provides mechanisms for multiple clients to
// simultaneously scan for nearby peers filtered by adveritising data
// contents. This class also provides hooks for other layers to manage the
// Adapter's scan state for other procedures that require it (e.g. connection
// establishment, pairing procedures, and other scan and advertising
// procedures).
// TODO(armansito): The last sentence of this paragraph hasn't been implemented
// yet.
//
// An instance of LowEnergyDiscoveryManager can be initialized in either
// "legacy" or "extended" mode. The legacy mode is intended for Bluetooth
// controllers that only support the pre-5.0 HCI scan command set. The extended
// mode is intended for Bluetooth controllers that claim to support the "LE
// Extended Advertising" feature.
//
// Only one instance of LowEnergyDiscoveryManager should be created per
// hci::Transport object as multiple instances cannot correctly maintain state
// if they operate concurrently.
//
// To request a session, a client calls StartDiscovery() and asynchronously
// obtains a LowEnergyDiscoverySession that it uniquely owns. The session object
// can be configured with a callback to receive scan results. The session
// maintains an internal filter that may be modified to restrict the scan
// results based on properties of received advertisements.
//
// PROCEDURE:
//
// Starting the first discovery session initiates a periodic scan procedure, in
// which the scan is stopped and restarted for a given scan period (10.24
// seconds by default). This continues until all sessions have been removed.
//
// By default duplicate filtering is used which means that a new advertising
// report will be generated for each discovered advertiser only once per scan
// period. Scan results for each scan period are cached so that sessions added
// during a scan period can receive previously processed results.
//
// EXAMPLE:
//     bt::gap::LowEnergyDiscoveryManager discovery_manager(
//         bt::gap::LowEnergyDiscoveryManager::Mode::kLegacy,
//         transport, dispatcher);
//     ...
//
//     std::unique_ptr<bt::gap::LowEnergyDiscoverySession> session;
//     discovery_manager.StartDiscovery(/*active=*/true, [&session](auto new_session) {
//       // Take ownership of the session to make sure it isn't terminated when
//       // this callback returns.
//       session = std::move(new_session);
//
//       // Only scan for peers advertising the "Heart Rate" GATT Service.
//       uint16_t uuid = 0x180d;
//       session->filter()->set_service_uuids({bt::UUID(uuid)});
//       session->SetResultCallback([](const
//       bt::hci::LowEnergyScanResult& result,
//                                     const bt::ByteBuffer&
//                                     advertising_data) {
//         // Do stuff with |result| and |advertising_data|. (|advertising_data|
//         // contains any received Scan Response data as well).
//       });
//     });
//
// NOTE: These classes are not thread-safe. An instance of
// LowEnergyDiscoveryManager is bound to its creation thread and the associated
// dispatcher and must be accessed and destroyed on the same thread.

// Represents a LE discovery session initiated via
// LowEnergyDiscoveryManager::StartDiscovery(). Instances cannot be created
// directly; instead they are handed to callers by LowEnergyDiscoveryManager.
//
// The discovery classes are not thread-safe. A LowEnergyDiscoverySession MUST
// be accessed and destroyed on the thread that it was created on.
class LowEnergyDiscoverySession final {
 public:
  // Destroying a session instance automatically ends the session. To terminate
  // a session, a client may either explicitly call Stop() or simply destroy
  // this instance.
  ~LowEnergyDiscoverySession();

  // Sets a callback for receiving notifications on discovered peers.
  // |data| contains advertising and scan response data (if any) obtained during
  // discovery.
  //
  // When this callback is set, it will immediately receive notifications for
  // the cached results from the most recent scan period. If a filter was
  // assigned earlier, then the callback will only receive results that match
  // the filter.
  //
  // Passive discovery sessions will call this callback for both directed and undirected
  // advertisements from known peers, while active discovery sessions will ignore directed
  // advertisements (as they are not from new peers).
  using PeerFoundCallback = fit::function<void(const Peer& peer)>;
  void SetResultCallback(PeerFoundCallback callback);

  // Sets a callback to get notified when the session becomes inactive due to an
  // internal error.
  void set_error_callback(fit::closure callback) { error_callback_ = std::move(callback); }

  // Returns the filter that belongs to this session. The caller may modify the
  // filter as desired. By default no peers are filtered.
  //
  // NOTE: The client is responsible for setting up the filter's "flags" field
  // for discovery procedures.
  DiscoveryFilter* filter() { return &filter_; }

  // Ends this session. This instance will stop receiving notifications for
  // peers.
  void Stop();

  // Returns true if this session has not been stopped and has not errored.
  bool alive() const { return alive_; }

  // Returns true if this is an active discovery session, or false if this is a passive discovery
  // session.
  bool active() const { return active_; }

 private:
  friend class LowEnergyDiscoveryManager;

  // Called by LowEnergyDiscoveryManager.
  explicit LowEnergyDiscoverySession(bool active, fxl::WeakPtr<LowEnergyDiscoveryManager> manager);

  // Called by LowEnergyDiscoveryManager on newly discovered scan results.
  void NotifyDiscoveryResult(const Peer& peer) const;

  // Marks this session as inactive and notifies the error handler.
  void NotifyError();

  bool alive_;
  bool active_;
  fxl::WeakPtr<LowEnergyDiscoveryManager> manager_;
  fit::closure error_callback_;
  PeerFoundCallback peer_found_callback_;
  DiscoveryFilter filter_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyDiscoverySession);
};

using LowEnergyDiscoverySessionPtr = std::unique_ptr<LowEnergyDiscoverySession>;

// See comments above.
class LowEnergyDiscoveryManager final : public hci::LowEnergyScanner::Delegate {
 public:
  // |peer_cache| and |scanner| MUST out-live this LowEnergyDiscoveryManager.
  LowEnergyDiscoveryManager(fxl::WeakPtr<hci::Transport> hci, hci::LowEnergyScanner* scanner,
                            PeerCache* peer_cache);
  virtual ~LowEnergyDiscoveryManager();

  // Starts a new discovery session and reports the result via |callback|. If a
  // session has been successfully started the caller will receive a new
  // LowEnergyDiscoverySession instance via |callback| which it uniquely owns.
  // |active| indicates whether active or passive discovery should occur.
  // On failure a nullptr will be returned via |callback|.
  //
  // TODO(armansito): Implement option to disable duplicate filtering. Would
  // this require software filtering for clients that did not request it?
  using SessionCallback = fit::function<void(LowEnergyDiscoverySessionPtr)>;
  void StartDiscovery(bool active, SessionCallback callback);

  // Pause current and future discovery sessions until the returned PauseToken is destroyed.
  // If PauseDiscovery is called multiple times, discovery will be paused until all returned
  // PauseTokens are destroyed.
  // NOTE: deferred_action::cancel() must not be called, or else discovery will never resume.
  using PauseToken = fit::deferred_action<fit::callback<void()>>;
  [[nodiscard]] PauseToken PauseDiscovery();

  // Sets a new scan period to any future and ongoing discovery procedures.
  void set_scan_period(zx::duration period) { scan_period_ = period; }

  // Returns whether there is an active scan in progress.
  bool discovering() const;

  // Returns true if discovery is paused.
  bool paused() const { return *paused_count_ != 0; }

  // Registers a callback which runs when a connectable advertisement is
  // received from known peer which was previously observed to be connectable during general
  // discovery. The |peer| argument is guaranteed to be valid until the callback returns.
  // The callback can also assume that LE transport information (i.e. |peer->le()|) will be present
  // and accessible.
  using PeerConnectableCallback = fit::function<void(Peer* peer)>;
  void set_peer_connectable_callback(PeerConnectableCallback callback) {
    connectable_cb_ = std::move(callback);
  }

  void AttachInspect(inspect::Node& parent, std::string name);

  fxl::WeakPtr<LowEnergyDiscoveryManager> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  friend class LowEnergyDiscoverySession;

  enum class State {
    kIdle,
    kStarting,
    kActive,
    kPassive,
    kStopping,
  };
  static std::string StateToString(State state);

  struct InspectProperties {
    inspect::Node node;
    inspect::UintProperty failed_count;
    inspect::DoubleProperty scan_interval_ms;
    inspect::DoubleProperty scan_window_ms;
  };

  const PeerCache* peer_cache() const { return peer_cache_; }

  const std::unordered_set<PeerId>& cached_scan_results() const { return cached_scan_results_; }

  // Creates and stores a new session object and returns it.
  std::unique_ptr<LowEnergyDiscoverySession> AddSession(bool active);

  // Called by LowEnergyDiscoverySession to stop a session that it was assigned
  // to.
  void RemoveSession(LowEnergyDiscoverySession* session);

  // hci::LowEnergyScanner::Delegate override:
  void OnPeerFound(const hci::LowEnergyScanResult& result, const ByteBuffer& data) override;
  void OnDirectedAdvertisement(const hci::LowEnergyScanResult& result) override;

  // Called by hci::LowEnergyScanner
  void OnScanStatus(hci::LowEnergyScanner::ScanStatus status);

  // Handlers for scan status updates.
  void OnScanFailed();
  void OnPassiveScanStarted();
  void OnActiveScanStarted();
  void OnScanStopped();
  void OnScanComplete();

  // Create sessions for all pending requests and pass the sessions to the request callbacks.
  void NotifyPending();

  // Tells the scanner to start scanning. Aliases are provided for improved
  // readability.
  void StartScan(bool active);
  inline void StartActiveScan() { StartScan(true); }
  inline void StartPassiveScan() { StartScan(false); }

  // Tells the scanner to stop scanning.
  void StopScan();

  // If there are any pending requests or valid sessions, start discovery.
  // Discovery must not be paused.
  // Called when discovery is unpaused or the scan period ends and needs to be restarted.
  void ResumeDiscovery();

  // Used by destructor to handle all sessions
  void DeactivateAndNotifySessions();

  // The dispatcher that we use for invoking callbacks asynchronously.
  async_dispatcher_t* dispatcher_;

  InspectProperties inspect_;

  StringInspectable<State> state_;

  // The peer cache that we use for storing and looking up scan results. We
  // hold a raw pointer as we expect this to out-live us.
  PeerCache* const peer_cache_;

  // Called when a directed connectable advertisement is received during an
  // active or passive scan.
  PeerConnectableCallback connectable_cb_;

  // The list of currently pending calls to start discovery.
  struct DiscoveryRequest {
    bool active;
    SessionCallback callback;
  };
  std::vector<DiscoveryRequest> pending_;

  // The list of currently active/known sessions. We store raw (weak) pointers
  // here because, while we don't actually own the session objects they will
  // always notify us before destruction so we can remove them from this list.
  //
  // The number of elements in |sessions_| acts as our scan reference count.
  // When |sessions_| becomes empty scanning is stopped. Similarly, scanning is
  // started on the insertion of the first element.
  std::list<LowEnergyDiscoverySession*> sessions_;

  // Identifiers for the cached scan results for the current scan period during
  // discovery. The minimum (and default) scan period is 10.24 seconds
  // when performing LE discovery. This can cause a long wait for a discovery
  // session that joined in the middle of a scan period and duplicate filtering
  // is enabled. We maintain this cache to immediately notify new sessions of
  // the currently cached results for this period.
  std::unordered_set<PeerId> cached_scan_results_;

  // The value (in ms) that we use for the duration of each scan period.
  zx::duration scan_period_ = kLEGeneralDiscoveryScanMin;

  // Count of the number of outstanding PauseTokens. When |paused_count_| is 0, discovery is
  // unpaused.
  IntInspectable<int> paused_count_;

  // The scanner that performs the HCI procedures. |scanner_| must out-live this
  // discovery manager.
  hci::LowEnergyScanner* scanner_;  // weak

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyDiscoveryManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyDiscoveryManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_DISCOVERY_MANAGER_H_
