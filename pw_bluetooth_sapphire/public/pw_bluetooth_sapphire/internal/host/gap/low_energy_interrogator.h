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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/gap/peer_cache.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt {
namespace hci {
class Transport;
}  // namespace hci

namespace gap {

// LowEnergyInterrogator sends HCI commands that request the controller version
// and features of a peer and handles responses by updating the Peer.
// LowEnergyInterrogator must only be used with an LE or dual mode controller.
class LowEnergyInterrogator final {
 public:
  // |peer| must outlive this object.
  LowEnergyInterrogator(Peer::WeakPtr peer,
                        hci_spec::ConnectionHandle handle,
                        hci::CommandChannel::WeakPtr hci);

  // Destroying the LowEnergyInterrogator effectively abandons an in-flight
  // interrogation, if there is one. The result callback will not be called.
  ~LowEnergyInterrogator() = default;

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // fails. Only 1 interrogation may be pending at a time.
  using ResultCallback = hci::ResultCallback<>;
  void Start(ResultCallback callback);

  // Abandons interrogation. The result callbacks will be called with result of
  // kCanceled. No-op if interrogation has already completed.
  void Cancel();

 private:
  void Complete(hci::Result<> result);

  void QueueReadLERemoteFeatures();
  void QueueReadRemoteVersionInformation();

  Peer::WeakPtr peer_;
  // Cache of the PeerId to allow for debug logging even if the WeakPtr<Peer> is
  // invalidated
  const PeerId peer_id_;
  const hci_spec::ConnectionHandle handle_;

  ResultCallback callback_ = nullptr;

  hci::SequentialCommandRunner cmd_runner_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  WeakSelf<LowEnergyInterrogator> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogator);
};

}  // namespace gap
}  // namespace bt
