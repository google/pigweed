// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOW_ENERGY_CONNECTION_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOW_ENERGY_CONNECTION_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/low_energy_connection.h"

namespace bt::hci::testing {

class FakeLowEnergyConnection final : public LowEnergyConnection {
 public:
  FakeLowEnergyConnection(hci_spec::ConnectionHandle handle, const DeviceAddress& local_address,
                          const DeviceAddress& peer_address,
                          pw::bluetooth::emboss::ConnectionRole role,
                          const Transport::WeakPtr& hci);

  // Triggers the encryption change callback.
  void TriggerEncryptionChangeCallback(hci::Result<bool> result);

  void TriggerPeerDisconnectCallback() {
    peer_disconnect_callback()(
        *this, pw::bluetooth::emboss::StatusCode::REMOTE_USER_TERMINATED_CONNECTION);
  }

  // LowEnergyConnection overrides:
  void Disconnect(pw::bluetooth::emboss::StatusCode reason) override {}
  bool StartEncryption() override;

  // Number of times StartEncryption() was called.
  int start_encryption_count() const { return start_encryption_count_; }

 private:
  int start_encryption_count_ = 0;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeLowEnergyConnection);
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_FAKE_LOW_ENERGY_CONNECTION_H_
