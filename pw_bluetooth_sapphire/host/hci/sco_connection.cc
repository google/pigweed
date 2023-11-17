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

#include "pw_bluetooth_sapphire/internal/host/hci/sco_connection.h"

#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

ScoConnection::ScoConnection(hci_spec::ConnectionHandle handle,
                             const DeviceAddress& local_address,
                             const DeviceAddress& peer_address,
                             const hci::Transport::WeakPtr& hci)
    : Connection(handle,
                 local_address,
                 peer_address,
                 hci,
                 [handle, hci] {
                   ScoConnection::OnDisconnectionComplete(handle, hci);
                 }),
      WeakSelf(this) {
  // The connection is registered & unregistered with ScoDataChannel by
  // sco::ScoConnection.
}

void ScoConnection::OnDisconnectionComplete(
    hci_spec::ConnectionHandle handle, const hci::Transport::WeakPtr& hci) {
  // ScoDataChannel only exists if HCI SCO is supported by the controller.
  if (hci->sco_data_channel()) {
    // The packet count must be cleared after sco::ScoConnection unregisters the
    // connection.
    hci->sco_data_channel()->ClearControllerPacketCount(handle);
  }
}

}  // namespace bt::hci
