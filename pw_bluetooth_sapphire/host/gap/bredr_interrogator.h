// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_

#include <lib/fit/function.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace hci {
class Transport;
}

namespace gap {

// A BrEdrInterrogator abstracts over the HCI commands and events involved
// immediately after connecting to a peer over BR/EDR.
class BrEdrInterrogator final {
 public:
  using ResultCallback = hci::ResultCallback<>;

  // |peer| must live longer than this object.
  BrEdrInterrogator(fxl::WeakPtr<Peer>, hci_spec::ConnectionHandle handle,
                    hci::Transport::WeakPtr hci);

  // Cancels the pending interrogation without calling the result callback.
  ~BrEdrInterrogator() = default;

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // fails. Only 1 interrogation may be pending at a time.
  void Start(ResultCallback callback);

  // Abandons interrogation. The result callbacks will be called with result of kCanceled. No-op if
  // interrogation has already completed.
  void Cancel();

 private:
  void Complete(hci::Result<> result);

  // Requests the name of the remote peer.
  void QueueRemoteNameRequest();

  // Requests features of the peer, and asks for Extended Features if they exist.
  void QueueReadRemoteFeatures();

  // Reads the extended feature page |page| of the peer.
  void QueueReadRemoteExtendedFeatures(uint8_t page);

  void QueueReadRemoteVersionInformation();

  // The hci transport to use.
  hci::Transport::WeakPtr hci_;

  fxl::WeakPtr<Peer> peer_;
  const PeerId peer_id_;
  const hci_spec::ConnectionHandle handle_;

  ResultCallback callback_ = nullptr;

  hci::SequentialCommandRunner cmd_runner_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrInterrogator> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
