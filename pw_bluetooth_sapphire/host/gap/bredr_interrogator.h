// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_

#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <memory>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace hci {
class Transport;
}

namespace gap {

// A BrEdrInterrogator abstracts over the HCI commands and events involved
// immediately after connecting to a peer over BR/EDR.
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
  BrEdrInterrogator(PeerCache* cache, fxl::RefPtr<hci::Transport> hci,
                    async_dispatcher_t* dispatcher);

  // Will cancel all uncompleted interrogations.
  ~BrEdrInterrogator();

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // abandoned.
  using ResultCallback = fit::function<void(hci::Status status)>;
  void Start(PeerId peer_id, hci::ConnectionHandle handle, ResultCallback callback);

  // Abandons any interrogation of |peer_id|.  Their callbacks will be called
  // with a Status of Canceled.
  void Cancel(PeerId peer_id);

 private:
  // Completes |peer| if there is nothing else to ask.
  void MaybeComplete(PeerId peer_id);

  // Completes interrogation on |peer| with |status|, possibly early.
  void Complete(PeerId peer_id, hci::Status status);

  // Reade the remote version information from the peer.
  void ReadRemoteVersionInformation(PeerId peer_id, hci::ConnectionHandle handle);

  // Requests the name of the remote peer.
  void MakeRemoteNameRequest(PeerId peer_id);

  // Requests features of |peer|, and asks for Extended Features if they exist.
  void ReadRemoteFeatures(PeerId peer_id, hci::ConnectionHandle handle);

  // Reads the extended feature page |page| of |peer|.
  void ReadRemoteExtendedFeatures(PeerId peer_id, hci::ConnectionHandle handle, uint8_t page);

  using CancelableCommandCallback = fxl::CancelableCallback<void(
      hci::CommandChannel::TransactionId id, const hci::EventPacket& event)>;
  struct Interrogation {
    Interrogation(hci::ConnectionHandle handle, ResultCallback callback);
    ~Interrogation();
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Interrogation);

    // Connection handle to |peer|
    hci::ConnectionHandle handle;

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

  // Cache to retrieve peer from.
  PeerCache* cache_;

  // The current set of interrogations
  std::unordered_map<PeerId, Interrogation> pending_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrInterrogator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
