// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_

#include <memory>

#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/remote_device_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "lib/fxl/functional/cancelable_callback.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/ref_ptr.h"
#include "lib/fxl/memory/weak_ptr.h"
#include "lib/fxl/synchronization/thread_checker.h"

namespace btlib {
namespace hci {
class Transport;
}

namespace gap {

// A BrEdrInterrogator abstracts over the HCI commands and events involved
// immediately after connecting to a remote device over BR/EDR.
// It also provides a way to hold pending connections while waiting for
// confirmation and times out those connections out when we do not get a
// response.
//
// This class owns a Connection object while interrogation happens.
//
// Only one interregator object is expected to exist per controller.
class BrEdrInterrogator {
 public:
  // |cache| must live longer than this object.
  BrEdrInterrogator(RemoteDeviceCache* cache, fxl::RefPtr<hci::Transport> hci,
                    async_dispatcher_t* dispatcher);

  // Will cancel all uncompleted interrogations.
  ~BrEdrInterrogator();

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // abandoned.
  using ResultCallback =
      fit::function<void(hci::Status status, hci::ConnectionPtr conn_ptr)>;
  void Start(DeviceId device_id, hci::ConnectionPtr conn_ptr,
             ResultCallback callback);

  // Abandons any interrogation of |device_id|.  Their callbacks will be called
  // with a Status of Canceled.
  void Cancel(DeviceId device_id);

 private:
  // Completes |device| if there is nothing else to ask.
  void MaybeComplete(DeviceId device_id);

  // Completes interrogation on |device| with |status|, possibly early.
  void Complete(DeviceId device_id, hci::Status status);

  // Reade the remote version information from the device.
  void ReadRemoteVersionInformation(DeviceId device_id,
                                    hci::ConnectionHandle handle);

  // Requests the name of the remote device.
  void MakeRemoteNameRequest(DeviceId device_id);

  // Requests features of |device|, and asks for Extended Features if they
  // exist.
  void ReadRemoteFeatures(DeviceId device_id, hci::ConnectionHandle handle);

  // Reads the extended feature page |page| of |device|.
  void ReadRemoteExtendedFeatures(DeviceId device_id,
                                  hci::ConnectionHandle handle, uint8_t page);

  using CancelableCommandCallback = fxl::CancelableCallback<void(
      hci::CommandChannel::TransactionId id, const hci::EventPacket& event)>;
  struct Interrogation {
    Interrogation(hci::ConnectionPtr conn_ptr, ResultCallback callback);
    ~Interrogation();
    FXL_DISALLOW_COPY_AND_ASSIGN(Interrogation);

    // Connection to |device|
    hci::ConnectionPtr conn_ptr;

    // Callback for results.
    ResultCallback result_cb;

    // Set of callbacks we cancel if we stop the interrogation.
    std::deque<CancelableCommandCallback> callbacks;

    // Finishes the interrogation, calling the callback.
    void Finish(hci::Status status);
  };

  // The hci transport to use.
  fxl::RefPtr<hci::Transport> hci_;

  // The dispatcher we use.
  async_dispatcher_t* dispatcher_;

  // Cache to retrieve devices from.
  RemoteDeviceCache* cache_;

  // The current set of interrogations
  // TODO(BT-750): Store Interrogations by value.
  std::unordered_map<DeviceId, std::unique_ptr<Interrogation>> pending_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrInterrogator> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(BrEdrInterrogator);
};

}  // namespace gap
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
