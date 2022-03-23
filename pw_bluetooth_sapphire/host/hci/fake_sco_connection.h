// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_SCO_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_SCO_CONNECTION_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/sco_connection.h"

namespace bt::hci::testing {

class FakeScoConnection final : public ScoConnection {
 public:
  FakeScoConnection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                    const DeviceAddress& peer_address, const fxl::WeakPtr<Transport>& hci);

  void TriggerPeerDisconnectCallback() {
    peer_disconnect_callback()(this, hci_spec::StatusCode::kRemoteUserTerminatedConnection);
  }

  // ScoConnection overrides:
  void Disconnect(hci_spec::StatusCode reason) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeScoConnection);
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_SCO_CONNECTION_H_
