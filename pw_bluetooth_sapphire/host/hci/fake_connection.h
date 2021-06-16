// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

namespace bt::hci::testing {

class FakeConnection final : public Connection {
 public:
  FakeConnection(ConnectionHandle handle, bt::LinkType ll_type, Role role,
                 const DeviceAddress& local_address, const DeviceAddress& peer_address);

  // Triggers the encryption change callback.
  void TriggerEncryptionChangeCallback(Status status, bool enabled);

  void TriggerPeerDisconnectCallback() {
    peer_disconnect_callback()(this, hci::StatusCode::kRemoteUserTerminatedConnection);
  }

  // Connection overrides:
  fxl::WeakPtr<Connection> WeakPtr() override;
  void Disconnect(StatusCode reason) override;
  bool StartEncryption() override;

  // Number of times StartEncryption() was called.
  int start_encryption_count() const { return start_encryption_count_; }

 private:
  int start_encryption_count_ = 0;
  State conn_state_;

  fxl::WeakPtrFactory<FakeConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeConnection);
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_
