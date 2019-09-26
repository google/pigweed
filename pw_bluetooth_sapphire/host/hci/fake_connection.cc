// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_connection.h"

namespace bt {
namespace hci {
namespace testing {

FakeConnection::FakeConnection(ConnectionHandle handle, LinkType ll_type, Role role,
                               const DeviceAddress& local_address,
                               const DeviceAddress& peer_address)
    : Connection(handle, ll_type, role, local_address, peer_address),
      is_open_(true),
      weak_ptr_factory_(this) {}

void FakeConnection::TriggerEncryptionChangeCallback(Status status, bool enabled) {
  ZX_DEBUG_ASSERT(encryption_change_callback());
  encryption_change_callback()(status, enabled);
}

fxl::WeakPtr<Connection> FakeConnection::WeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

void FakeConnection::Close(bool send_disconnect, StatusCode reason) {
  // TODO(armansito): implement
}

bool FakeConnection::StartEncryption() {
  start_encryption_count_++;
  return true;
}

}  // namespace testing
}  // namespace hci
}  // namespace bt
