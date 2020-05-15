// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_INTERROGATOR_H_

#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <memory>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace hci {
class Transport;
}

namespace gap {

// An Interrogator abstracts over the HCI commands and events involved
// immediately after connecting to a peer.
//
// Concrete implementations must implement Interrogator::SendCommands(InterrogationRefPtr), which
// sends the HCI commands that request the information needed for interrogation. HCI command
// callbacks must include the InterrogationRefPtr in their capture list so that Interrogator can
// detect when all interrogation command callbacks have completed.
//
// Only one interregator object is expected to exist per controller.
class Interrogator {
 public:
  // |cache| must live longer than this object.
  Interrogator(PeerCache* cache, fxl::RefPtr<hci::Transport> hci, async_dispatcher_t* dispatcher);

  // Will cancel all uncompleted interrogations.
  virtual ~Interrogator();

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // fails.
  using ResultCallback = fit::callback<void(hci::Status status)>;
  void Start(PeerId peer_id, hci::ConnectionHandle handle, ResultCallback callback);

  // Abandons any interrogation of |peer_id|.  Their callbacks will be called
  // with a Status of Canceled. No-op if interrogation has already completed.
  void Cancel(PeerId peer_id);

 protected:
  // InterrogationRefPtr is passed to command callbacks to ensure correctness in waiting for all
  // callbacks to complete (i.e. interrogation to complete). When all RefPtrs to an Interrogation
  // have been dropped, |release_cb| is called, which calls Interrogation::Complete.
  class Interrogation : public fxl::RefCountedThreadSafe<Interrogation> {
   public:
    Interrogation(PeerId peer_id, hci::ConnectionHandle handle, ResultCallback result_cb);
    ~Interrogation();

    // Completes interrogation by calling |result_cb| with ||status|, possibly early in the case of
    // an error. No-op if interrogation already completed.
    void Complete(hci::Status status);

    // Returns true if the Interrogation has not yet completed.
    bool active() const { return static_cast<bool>(result_cb_); }

    PeerId peer_id() const { return peer_id_; }

    hci::ConnectionHandle handle() const { return handle_; }

    fxl::WeakPtr<Interrogation> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

   private:
    PeerId peer_id_;

    // Connection handle to |peer|
    hci::ConnectionHandle handle_;

    // Callback for results.
    ResultCallback result_cb_;

    // Used to cancel command callbacks.
    fxl::WeakPtrFactory<Interrogation> weak_ptr_factory_;
    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Interrogation);
  };

  using InterrogationRefPtr = fxl::RefPtr<Interrogation>;

  // Send initial command channel interrogation commands. Called when interrogation starts.
  // A copy of |interrogation| must be passed to all command callbacks to detect when they complete.
  virtual void SendCommands(InterrogationRefPtr interrogation) = 0;

  hci::Transport* hci() const { return hci_.get(); }

  PeerCache* peer_cache() const { return cache_; }

  async_dispatcher_t* dispatcher() const { return dispatcher_; }

  // BR/EDR & LE HCI commands:

  // Read the remote version information from the peer.
  void ReadRemoteVersionInformation(InterrogationRefPtr interrogation);

 private:
  // The hci transport to use.
  fxl::RefPtr<hci::Transport> hci_;

  // The dispatcher we use.
  async_dispatcher_t* const dispatcher_;

  // Cache to retrieve peer from.
  PeerCache* const cache_;

  // The current set of interrogations.
  // All Interrogation weak pointers are valid because they are removed on destruction of the
  // Interrogation they point to.
  std::unordered_map<PeerId, fxl::WeakPtr<Interrogation>> pending_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<Interrogator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Interrogator);
};

}  // namespace gap
}  // namespace bt
#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_INTERROGATOR_H_
