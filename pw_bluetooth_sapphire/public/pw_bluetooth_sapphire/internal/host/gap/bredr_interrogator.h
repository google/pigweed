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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_

#include <lib/fit/function.h>

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gap/peer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

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
  BrEdrInterrogator(Peer::WeakPtr peer,
                    hci_spec::ConnectionHandle handle,
                    hci::CommandChannel::WeakPtr cmd_channel);

  // Cancels the pending interrogation without calling the result callback.
  ~BrEdrInterrogator() = default;

  // Starts interrogation. Calls |callback| when the sequence is completed or
  // fails. Only 1 interrogation may be pending at a time.
  void Start(ResultCallback callback);

  // Abandons interrogation. The result callbacks will be called with result of
  // kCanceled. No-op if interrogation has already completed.
  void Cancel();

 private:
  void Complete(hci::Result<> result);

  // Requests the name of the remote peer.
  void QueueRemoteNameRequest();

  // Requests features of the peer, and asks for Extended Features if they
  // exist.
  void QueueReadRemoteFeatures();

  // Reads the extended feature page |page| of the peer.
  void QueueReadRemoteExtendedFeatures(uint8_t page);

  void QueueReadRemoteVersionInformation();

  Peer::WeakPtr peer_;
  const PeerId peer_id_;
  const hci_spec::ConnectionHandle handle_;

  ResultCallback callback_ = nullptr;

  hci::SequentialCommandRunner cmd_runner_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  WeakSelf<BrEdrInterrogator> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
