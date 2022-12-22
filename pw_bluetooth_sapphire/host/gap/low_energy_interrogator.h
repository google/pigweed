// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_

#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt {
namespace hci {
class Transport;
}  // namespace hci

namespace gap {

// LowEnergyInterrogator sends HCI commands that request the controller version and features of a
// peer and handles responses by updating the Peer. LowEnergyInterrogator must only be used with an
// LE or dual mode controller.
class LowEnergyInterrogator final {
 public:
  // |peer| must outlive this object.
  LowEnergyInterrogator(fxl::WeakPtr<Peer> peer, hci_spec::ConnectionHandle handle,
                        hci::CommandChannel::WeakPtr hci);

  // Destroying the LowEnergyInterrogator effectively abandons an in-flight interrogation, if there
  // is one. The result callback will not be called.
  ~LowEnergyInterrogator() = default;

  // Starts interrogation. Calls |callback| when the sequence is completed or fails. Only 1
  // interrogation may be pending at a time.
  using ResultCallback = hci::ResultCallback<>;
  void Start(ResultCallback callback);

  // Abandons interrogation. The result callbacks will be called with result of kCanceled. No-op if
  // interrogation has already completed.
  void Cancel();

 private:
  void Complete(hci::Result<> result);

  void QueueReadLERemoteFeatures();
  void QueueReadRemoteVersionInformation();

  fxl::WeakPtr<Peer> peer_;
  // Cache of the PeerId to allow for debug logging even if the WeakPtr<Peer> is invalidated
  const PeerId peer_id_;
  const hci_spec::ConnectionHandle handle_;

  ResultCallback callback_ = nullptr;

  hci::SequentialCommandRunner cmd_runner_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyInterrogator> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_
