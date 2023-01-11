// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_bredr_connection.h"

namespace bt::hci::testing {

FakeBrEdrConnection::FakeBrEdrConnection(hci_spec::ConnectionHandle handle,
                                         const DeviceAddress& local_address,
                                         const DeviceAddress& peer_address,
                                         pw::bluetooth::emboss::ConnectionRole role,
                                         const hci::Transport::WeakPtr& hci)
    : BrEdrConnection(handle, local_address, peer_address, role, hci) {}

void FakeBrEdrConnection::TriggerEncryptionChangeCallback(hci::Result<bool> result) {
  BT_ASSERT(encryption_change_callback());
  encryption_change_callback()(result);
}

void FakeBrEdrConnection::Disconnect(pw::bluetooth::emboss::StatusCode reason) {}

bool FakeBrEdrConnection::StartEncryption() {
  start_encryption_count_++;
  return true;
}

}  // namespace bt::hci::testing
