// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_

#include "pw_bluetooth_sapphire/internal/host/hci/connection.h"

namespace bt::hci {
class ScoConnection : public Connection, public WeakSelf<ScoConnection> {
 public:
  ScoConnection(hci_spec::ConnectionHandle handle,
                const DeviceAddress& local_address,
                const DeviceAddress& peer_address,
                const hci::Transport::WeakPtr& hci);

 private:
  // This method must be static since it may be invoked after the connection
  // associated with it is destroyed.
  static void OnDisconnectionComplete(hci_spec::ConnectionHandle handle,
                                      const hci::Transport::WeakPtr& hci);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_
