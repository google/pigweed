// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_

#include <queue>
#include <unordered_set>

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include "src/lib/fxl/macros.h"
#include "src/lib/fxl/memory/weak_ptr.h"

#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"

namespace bt {

namespace hci {
class Transport;
}  // namespace hci

namespace gap {

class BrEdrDiscoveryManager;
class RemoteDeviceCache;

// BrEdrDiscoveryManager implements discovery for BR/EDR devices.  We provide a
// mechanism for multiple clients to simultaneously request discovery.  Devices
// discovered will be added to the RemoteDeviceCache.
//
// Only one instance of BrEdrDiscoveryManager should be created for a bt-host.
//
// Request discovery using RequestDiscovery() which will provide a
// BrEdrDiscoverySession object in the |callback| when discovery is started.
// Ownership of this session is passed to the caller; when no sessions exist,
// discovery is halted.
//
// TODO(jamuraa): Name resolution should also happen here. (NET-509)
//
// This class is not thread-safe, BrEdrDiscoverySessions should be created and
// accessed on the same thread the BrEdrDiscoveryManager is created.
class BrEdrDiscoverySession final {
 public:
  // Destroying a session instance ends this discovery session. Discovery may
  // continue if other clients have started discovery sesisons.
  ~BrEdrDiscoverySession();

  // Set a result callback that will be notified whenever a result is returned
  // from the controller.  You will get duplicate results when using this
  // method.
  // Prefer RemoteDeviceCache.set_device_updated_callback() instead.
  using DeviceFoundCallback = fit::function<void(const RemoteDevice& device)>;
  void set_result_callback(DeviceFoundCallback callback) {
    device_found_callback_ = std::move(callback);
  }

  // Set a callback to be notified if the session becomes inactive because
  // of internal errors.
  void set_error_callback(fit::closure callback) {
    error_callback_ = std::move(callback);
  }

 private:
  friend class BrEdrDiscoveryManager;

  // Used by the BrEdrDiscoveryManager to create a session.
  explicit BrEdrDiscoverySession(fxl::WeakPtr<BrEdrDiscoveryManager> manager);

  // Called by the BrEdrDiscoveryManager when a device report is found.
  void NotifyDiscoveryResult(const RemoteDevice& device) const;

  // Marks this session as ended because of an error.
  void NotifyError() const;

  fxl::WeakPtr<BrEdrDiscoveryManager> manager_;
  fit::closure error_callback_;
  DeviceFoundCallback device_found_callback_;
  fxl::ThreadChecker thread_checker_;

  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrDiscoverySession);
};

class BrEdrDiscoverableSession final {
 public:
  // Destroying a session instance relinquishes the request.
  // The device may still be discoverable if others are requesting so.
  ~BrEdrDiscoverableSession();

 private:
  friend class BrEdrDiscoveryManager;

  // Used by the BrEdrDiscoveryManager to create a session.
  explicit BrEdrDiscoverableSession(
      fxl::WeakPtr<BrEdrDiscoveryManager> manager);

  fxl::WeakPtr<BrEdrDiscoveryManager> manager_;
  fxl::ThreadChecker thread_checker_;

  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrDiscoverableSession);
};

class BrEdrDiscoveryManager final {
 public:
  // |device_cache| MUST out-live this BrEdrDiscoveryManager.
  BrEdrDiscoveryManager(fxl::RefPtr<hci::Transport> hci, hci::InquiryMode mode,
                        RemoteDeviceCache* device_cache);

  ~BrEdrDiscoveryManager();

  // Starts discovery and reports the status via |callback|. If discovery has
  // been successfully started, the callback will receive a session object that
  // it owns. If no sessions are owned, device discovery is stopped.
  using DiscoveryCallback =
      fit::function<void(const hci::Status& status,
                         std::unique_ptr<BrEdrDiscoverySession> session)>;
  void RequestDiscovery(DiscoveryCallback callback);

  // Returns whether a discovery session is active.
  bool discovering() const { return !discovering_.empty(); }

  // Requests this device be discoverable. Devices are discoverable as long as
  // anyone holds a discoverable session.
  using DiscoverableCallback =
      fit::function<void(const hci::Status& status,
                         std::unique_ptr<BrEdrDiscoverableSession> session)>;
  void RequestDiscoverable(DiscoverableCallback callback);

  bool discoverable() const { return !discoverable_.empty(); }

 private:
  friend class BrEdrDiscoverySession;
  friend class BrEdrDiscoverableSession;

  // Starts the inquiry procedure if any sessions exist.
  void MaybeStartInquiry();

  // Stops the inquiry procedure.
  void StopInquiry();

  // Used to receive Inquiry Results.
  void InquiryResult(const hci::EventPacket& event);

  // Used to receive Inquiry Results.
  void ExtendedInquiryResult(const hci::EventPacket& event);

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
  // If |interlaced| is true, and the controller does not supoport interlaces
  // inquiry scan mode, standard mode is used.
  void WriteInquiryScanSettings(uint16_t interval, uint16_t window,
                                bool interlaced);

  // Creates and stores a new session object and returns it.
  std::unique_ptr<BrEdrDiscoverableSession> AddDiscoverableSession();

  // Removes |session_| from the active sessions.
  void RemoveDiscoverableSession(BrEdrDiscoverableSession* session);

  // Sends a RemoteNameRequest to the device with |id|.
  void RequestRemoteDeviceName(DeviceId id);

  // The HCI Transport
  fxl::RefPtr<hci::Transport> hci_;

  // The dispatcher that we use for invoking callbacks asynchronously.
  async_dispatcher_t* dispatcher_;

  // Device cache to use.
  // We hold a raw pointer is because it must out-live us.
  RemoteDeviceCache* cache_;

  // The list of discovering sessions. We store raw pointers here as we
  // don't own the sessions.  Sessions notify us when they are destroyed to
  // maintain this list.
  //
  // When |discovering_| becomes empty then scanning is stopped.
  std::unordered_set<BrEdrDiscoverySession*> discovering_;
  // Sessions that have been removed but are still active.
  // Inquiry persists until we receive a Inquiry Complete event.
  // TODO(NET-619): we should not need these once we can Inquiry Cancel.
  std::unordered_set<BrEdrDiscoverySession*> zombie_discovering_;

  // The set of callbacks that are waiting on inquiry to start.
  std::queue<DiscoveryCallback> pending_discovery_;

  // The list of discoverable sessions. We store raw pointers here as we
  // don't own the sessions.  Sessions notify us when they are destroyed to
  // maintain this list.
  //
  // When |discoverable_| becomes empty then inquiry scan is disabled.
  std::unordered_set<BrEdrDiscoverableSession*> discoverable_;

  // The set of callbacks that are waiting on inquiry scan to be active.
  std::queue<hci::StatusCallback> pending_discoverable_;

  // The Handler IDs of the event handlers for inquiry results.
  hci::CommandChannel::EventHandlerId result_handler_id_;
  hci::CommandChannel::EventHandlerId rssi_handler_id_;
  hci::CommandChannel::EventHandlerId eir_handler_id_;

  // The inquiry mode that we should use.
  hci::InquiryMode desired_inquiry_mode_;
  // The current inquiry mode.
  hci::InquiryMode current_inquiry_mode_;

  fxl::ThreadChecker thread_checker_;

  fxl::WeakPtrFactory<BrEdrDiscoveryManager> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrDiscoveryManager);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_DISCOVERY_MANAGER_H_
