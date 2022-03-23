// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_

#include "connection.h"

namespace bt::hci {
class ScoConnection : public Connection {
 public:
  ScoConnection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                const DeviceAddress& peer_address, const fxl::WeakPtr<Transport>& hci);

  fxl::WeakPtr<ScoConnection> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  static void OnDisconnectionComplete(hci_spec::ConnectionHandle handle,
                                      const fxl::WeakPtr<Transport>& hci);

  fxl::WeakPtrFactory<ScoConnection> weak_ptr_factory_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SCO_CONNECTION_H_
