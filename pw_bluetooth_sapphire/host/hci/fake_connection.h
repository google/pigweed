// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"

namespace bt {
namespace hci {
namespace testing {

class FakeConnection final : public Connection {
 public:
  FakeConnection(ConnectionHandle handle, LinkType ll_type, Role role,
                 const common::DeviceAddress& local_address,
                 const common::DeviceAddress& peer_address);

  // Triggers the encryption change callback.
  void TriggerEncryptionChangeCallback(Status status, bool enabled);

  // Connection overrides:
  fxl::WeakPtr<Connection> WeakPtr() override;
  void Close(StatusCode reason) override;
  bool StartEncryption() override;

  // Number of times StartEncryption() was called.
  int start_encryption_count() const { return start_encryption_count_; }

 private:
  int start_encryption_count_ = 0;

  fxl::WeakPtrFactory<FakeConnection> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeConnection);
};

}  // namespace testing
}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_CONNECTION_H_
