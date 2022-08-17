// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_low_energy_connection.h"

namespace bt::hci::testing {

FakeLowEnergyConnection::FakeLowEnergyConnection(hci_spec::ConnectionHandle handle,
                                                 const DeviceAddress& local_address,
                                                 const DeviceAddress& peer_address,
                                                 hci_spec::ConnectionRole role,
                                                 const fxl::WeakPtr<Transport>& hci)
    : LowEnergyConnection(handle, local_address, peer_address, hci_spec::LEConnectionParameters(),
                          role, hci) {}

void FakeLowEnergyConnection::TriggerEncryptionChangeCallback(hci::Result<bool> result) {
  BT_ASSERT(encryption_change_callback());
  encryption_change_callback()(result);
}

bool FakeLowEnergyConnection::StartEncryption() {
  start_encryption_count_++;
  return true;
}

}  // namespace bt::hci::testing
