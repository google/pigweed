// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTOR_H_

#include <fbl/macros.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/local_address_delegate.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace hci {

class LocalAddressDelegate;
class Transport;

// A LowEnergyConnector abstracts over the HCI commands and events involved in
// initiating a direct link-layer connection with a peer device. This class also
// provides a way for a delegate to be notified when a connection is initiated
// by a remote.
//
// This class vends Connection objects for LE link layer connections.
//
// Instances of this class are expected to each exist as a singleton on a
// per-transport basis as multiple instances cannot accurately reflect the state
// of the controller while allowing simultaneous operations.
class LowEnergyConnector : public LocalAddressClient {
 public:
  // The constructor expects the following arguments:
  //   - |hci|: The HCI transport this should operate on.
  //
  //   - |local_addr_delegate|: The delegate used to obtain the current public
  //     or random device address to use in locally initiated requests.
  //
  //   - |dispatcher|: The dispatcher that will be used to run all
  //     asynchronous operations. This must be bound to the thread on which the
  //     LowEnergyConnector is created.
  //
  //   - |delegate|: The delegate that will be notified when a new logical link
  //     is established due to an incoming request (remote initiated).
  using IncomingConnectionDelegate = fit::function<void(
      ConnectionHandle handle, Connection::Role role, const DeviceAddress& peer_address,
      const LEConnectionParameters& conn_params)>;
  LowEnergyConnector(fxl::RefPtr<Transport> hci, LocalAddressDelegate* local_addr_delegate,
                     async_dispatcher_t* dispatcher, IncomingConnectionDelegate delegate);

  // Deleting an instance cancels any pending connection request.
  ~LowEnergyConnector();

  // Creates a LE link layer connection to the remote device identified by
  // |peer_address| with initial connection parameters |initial_parameters|.
  // Returns false, if a create connection request is currently pending.
  //
  // If |use_whitelist| is true, then the controller white list is used to
  // determine which advertiser to connect to. Otherwise, the controller will
  // connect to |peer_address|.
  //
  // |status_callback| is called asynchronously to notify the status of the
  // operation. A valid |link| will be provided on success.
  //
  // |timeout_ms| specifies a time period after which the request will time out.
  // When a request to create connection times out, |status_callback| will be
  // called with a null |link| and a |status| with error Host::Error::kTimedOut.
  using StatusCallback = fit::function<void(Status status, ConnectionPtr link)>;
  bool CreateConnection(bool use_whitelist, const DeviceAddress& peer_address,
                        uint16_t scan_interval, uint16_t scan_window,
                        const LEPreferredConnectionParameters& initial_parameters,
                        StatusCallback status_callback, zx::duration timeout);

  // Cancels the currently pending connection attempt.
  void Cancel();

  // Returns true if a connection request is currently pending.
  bool request_pending() const { return pending_request_.has_value(); }

  // Returns true if a connection timeout has been posted. Returns false if it
  // was not posted or was canceled. This is intended for unit tests.
  bool timeout_posted() const { return request_timeout_task_.is_pending(); }

  // LocalAddressClient override:
  bool AllowsRandomAddressChange() const override {
    return !pending_request_ || !pending_request_->initiating;
  }

 private:
  struct PendingRequest {
    PendingRequest() = default;
    PendingRequest(const DeviceAddress& peer_address, StatusCallback status_callback);

    bool initiating = false;  // True if the HCI command has been sent.
    bool canceled = false;
    bool timed_out = false;
    DeviceAddress local_address;
    DeviceAddress peer_address;
    StatusCallback status_callback;
  };

  // Called by CreateConnection() after the local device address has been
  // obtained.
  void CreateConnectionInternal(const DeviceAddress& local_address, bool use_whitelist,
                                const DeviceAddress& peer_address, uint16_t scan_interval,
                                uint16_t scan_window,
                                const LEPreferredConnectionParameters& initial_parameters,
                                StatusCallback status_callback, zx::duration timeout);

  // Called by Cancel() and by OnCreateConnectionTimeout().
  void CancelInternal(bool timed_out = false);

  // Event handler for the HCI LE Connection Complete event.
  void OnConnectionCompleteEvent(const EventPacket& event);

  // Called when a LE Create Connection request has completed.
  void OnCreateConnectionComplete(Status status, ConnectionPtr link);

  // Called when a LE Create Connection request has timed out.
  void OnCreateConnectionTimeout();

  // Task runner for all asynchronous tasks.
  async_dispatcher_t* dispatcher_;

  // The HCI transport.
  fxl::RefPtr<Transport> hci_;

  // Used to obtain the local device address type to use during initiation.
  LocalAddressDelegate* local_addr_delegate_;  // weak

  // The delegate that gets notified when a new link layer connection gets
  // created.
  IncomingConnectionDelegate delegate_;

  // The currently pending request.
  std::optional<PendingRequest> pending_request_;

  // Task that runs when a request to create connection times out. We do not
  // rely on CommandChannel's timer since that request completes when we receive
  // the HCI Command Status event.
  async::TaskClosureMethod<LowEnergyConnector, &LowEnergyConnector::OnCreateConnectionTimeout>
      request_timeout_task_{this};

  // Our event handle ID for the LE Connection Complete event.
  CommandChannel::EventHandlerId event_handler_id_;

  fxl::ThreadChecker thread_checker_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyConnector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyConnector);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_LOW_ENERGY_CONNECTOR_H_
