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

#include "pw_bluetooth_sapphire/internal/host/hci/fake_low_energy_connection.h"

namespace bt::hci::testing {

FakeLowEnergyConnection::FakeLowEnergyConnection(
    hci_spec::ConnectionHandle handle,
    const DeviceAddress& local_address,
    const DeviceAddress& peer_address,
    pw::bluetooth::emboss::ConnectionRole role,
    const hci::Transport::WeakPtr& hci)
    : LowEnergyConnection(handle,
                          local_address,
                          peer_address,
                          hci_spec::LEConnectionParameters(),
                          role,
                          hci) {}

void FakeLowEnergyConnection::TriggerEncryptionChangeCallback(
    hci::Result<bool> result) {
  BT_ASSERT(encryption_change_callback());
  encryption_change_callback()(result);
}

bool FakeLowEnergyConnection::StartEncryption() {
  start_encryption_count_++;
  return true;
}

}  // namespace bt::hci::testing
