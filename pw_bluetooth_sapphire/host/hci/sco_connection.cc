// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
