// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <queue>
#include <unordered_set>

#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/error.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::gap {

class PeerCache;

class BrEdrDiscoverySession;
class BrEdrDiscoverableSession;

// BrEdrDiscoveryManager implements discovery for BR/EDR peers.  We provide a
// mechanism for multiple clients to simultaneously request discovery.  Peers
// discovered will be added to the PeerCache.
//
// Only one instance of BrEdrDiscoveryManager should be created for a bt-host.
//
// Request discovery using RequestDiscovery() which will provide a
// BrEdrDiscoverySession object in the |callback| when discovery is started.
// Ownership of this session is passed to the caller; when no sessions exist,
// discovery is halted.
//
// TODO(jamuraa): Name resolution should also happen here. (fxbug.dev/851)
//
// This class is not thread-safe, BrEdrDiscoverySessions should be created and
// accessed on the same thread the BrEdrDiscoveryManager is created.
class BrEdrDiscoveryManager final {
 public:
  // |peer_cache| MUST out-live this BrEdrDiscoveryManager.
  BrEdrDiscoveryManager(hci::CommandChannel::WeakPtr cmd, hci_spec::InquiryMode mode,
                        PeerCache* peer_cache);

  ~BrEdrDiscoveryManager();

  // Starts discovery and reports the status via |callback|. If discovery has
  // been successfully started, the callback will receive a session object that
  // it owns. If no sessions are owned, peer discovery is stopped.
  using DiscoveryCallback = fit::function<void(const hci::Result<>& status,
                                               std::unique_ptr<BrEdrDiscoverySession> session)>;
  void RequestDiscovery(DiscoveryCallback callback);

  // Returns whether a discovery session is active.
  bool discovering() const { return !discovering_.empty(); }

  // Requests this device be discoverable. We are discoverable as long as
  // anyone holds a discoverable session.
  using DiscoverableCallback = fit::function<void(
      const hci::Result<>& status, std::unique_ptr<BrEdrDiscoverableSession> session)>;
  void RequestDiscoverable(DiscoverableCallback callback);

  bool discoverable() const { return !discoverable_.empty(); }

  // Updates local name of BrEdrDiscoveryManager.
  // Updates the extended inquiry response to include the new |name|.
  void UpdateLocalName(std::string name, hci::ResultFunction<> callback);

  // Returns the BR/EDR local name used for EIR.
  std::string local_name() const { return local_name_; }

  // Attach discovery manager inspect node as a child node of |parent|.
  void AttachInspect(inspect::Node& parent, std::string name);

  using WeakPtr = WeakSelf<BrEdrDiscoveryManager>::WeakPtr;

 private:
  friend class BrEdrDiscoverySession;
  friend class BrEdrDiscoverableSession;

  // Starts the inquiry procedure if any sessions exist.
  void MaybeStartInquiry();

  // Stops the inquiry procedure.
  void StopInquiry();

  // Used to receive Inquiry Results.
  hci::CommandChannel::EventCallbackResult InquiryResult(const hci::EventPacket& event);

  // Used to receive Inquiry Results.
  hci::CommandChannel::EventCallbackResult ExtendedInquiryResult(const hci::EventPacket& event);

  // Creates and stores a new session object and returns it.
  std::unique_ptr<BrEdrDiscoverySession> AddDiscoverySession();

  // Removes |session_| from the active sessions.
  void RemoveDiscoverySession(BrEdrDiscoverySession* session);

  // Invalidates all discovery sessions, invoking their error callbacks.
  void InvalidateDiscoverySessions();

  // Sets the Inquiry Scan to the correct state given discoverable sessions,
  // pending requests and the current scan state.
  void SetInquiryScan();

  // Writes the Inquiry Scan Settings to the controller.
  // If |interlaced| is true, and the controller does not support interlaces
  // inquiry scan mode, standard mode is used.
  void WriteInquiryScanSettings(uint16_t interval, uint16_t window, bool interlaced);

  // Creates and stores a new session object and returns it.
  std::unique_ptr<BrEdrDiscoverableSession> AddDiscoverableSession();

  // Removes |session_| from the active sessions.
  void RemoveDiscoverableSession(BrEdrDiscoverableSession* session);

  // Sends a RemoteNameRequest to the peer with |id|.
  void RequestPeerName(PeerId id);

  // Updates the EIR response data with |name|.
  // Currently, only the name field in EIR is supported.
  void UpdateEIRResponseData(std::string name, hci::ResultFunction<> callback);

  // Updates the Inspect properties
  void UpdateInspectProperties();

  // The Command channel
  hci::CommandChannel::WeakPtr cmd_;

  struct InspectProperties {
    inspect::Node node;

    inspect::UintProperty discoverable_sessions;
    inspect::UintProperty pending_discoverable_sessions;
    inspect::UintProperty discoverable_sessions_count;
    inspect::UintProperty last_discoverable_length_sec;

    inspect::UintProperty discovery_sessions;
    inspect::UintProperty inquiry_sessions_count;
    inspect::UintProperty last_inquiry_length_sec;

    std::optional<zx_time_t> discoverable_started_time;
    std::optional<zx_time_t> inquiry_started_time;

    void Initialize(inspect::Node node);
    void Update(size_t discoverable_count, size_t pending_discoverable_count,
                size_t discovery_count, zx_time_t now);
  };

  InspectProperties inspect_properties_;

  // The dispatcher that we use for invoking callbacks asynchronously.
  async_dispatcher_t* dispatcher_;

  // Peer cache to use.
  // We hold a raw pointer is because it must out-live us.
  PeerCache* cache_;

  // The local name that was last successfully written to the controller.
  std::string local_name_;

  // The list of discovering sessions. We store raw pointers here as we
  // don't own the sessions.  Sessions notify us when they are destroyed to
  // maintain this list.
  //
  // When |discovering_| becomes empty then scanning is stopped.
  std::unordered_set<BrEdrDiscoverySession*> discovering_;
  // Sessions that have been removed but are still active.
  // Inquiry persists until we receive a Inquiry Complete event.
  // TODO(fxbug.dev/668): we should not need these once we can Inquiry Cancel.
  std::unordered_set<BrEdrDiscoverySession*> zombie_discovering_;

  // The set of peers that we have pending name requests for.
  std::unordered_set<PeerId> requesting_names_;

  // The set of callbacks that are waiting on inquiry to start.
  std::queue<DiscoveryCallback> pending_discovery_;

  // The list of discoverable sessions. We store raw pointers here as we
  // don't own the sessions.  Sessions notify us when they are destroyed to
  // maintain this list.
  //
  // When |discoverable_| becomes empty then inquiry scan is disabled.
  std::unordered_set<BrEdrDiscoverableSession*> discoverable_;

  // The set of callbacks that are waiting on inquiry scan to be active.
  std::queue<hci::ResultFunction<>> pending_discoverable_;

  // The Handler IDs of the event handlers for inquiry results.
  hci::CommandChannel::EventHandlerId result_handler_id_;
  hci::CommandChannel::EventHandlerId rssi_handler_id_;
  hci::CommandChannel::EventHandlerId eir_handler_id_;

  // The inquiry mode that we should use.
  hci_spec::InquiryMode desired_inquiry_mode_;
  // The current inquiry mode.
  hci_spec::InquiryMode current_inquiry_mode_;

  WeakSelf<BrEdrDiscoveryManager> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrDiscoveryManager);
};

class BrEdrDiscoverySession final {
 public:
  // Destroying a session instance ends this discovery session. Discovery may
  // continue if other clients have started discovery sesisons.
  ~BrEdrDiscoverySession();

  // Set a result callback that will be notified whenever a result is returned
  // from the controller.  You will get duplicate results when using this
  // method.
  // Prefer PeerCache.add_peer_updated_callback() instead.
  using PeerFoundCallback = fit::function<void(const Peer& peer)>;
  void set_result_callback(PeerFoundCallback callback) {
    peer_found_callback_ = std::move(callback);
  }

  // Set a callback to be notified if the session becomes inactive because
  // of internal errors.
  void set_error_callback(fit::closure callback) { error_callback_ = std::move(callback); }

 private:
  friend class BrEdrDiscoveryManager;

  // Used by the BrEdrDiscoveryManager to create a session.
  explicit BrEdrDiscoverySession(BrEdrDiscoveryManager::WeakPtr manager);

  // Called by the BrEdrDiscoveryManager when a peer report is found.
  void NotifyDiscoveryResult(const Peer& peer) const;

  // Marks this session as ended because of an error.
  void NotifyError() const;

  BrEdrDiscoveryManager::WeakPtr manager_;
  fit::closure error_callback_;
  PeerFoundCallback peer_found_callback_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrDiscoverySession);
};

class BrEdrDiscoverableSession final {
 public:
  // Destroying a session instance relinquishes the request.
  // The peer may still be discoverable if others are requesting so.
  ~BrEdrDiscoverableSession();

 private:
  friend class BrEdrDiscoveryManager;

  // Used by the BrEdrDiscoveryManager to create a session.
  explicit BrEdrDiscoverableSession(BrEdrDiscoveryManager::WeakPtr manager);

  BrEdrDiscoveryManager::WeakPtr manager_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrDiscoverableSession);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_
