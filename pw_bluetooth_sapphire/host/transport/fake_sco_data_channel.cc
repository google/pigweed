// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_sco_data_channel.h"

namespace bt::hci {

void FakeScoDataChannel::RegisterConnection(WeakPtr<ConnectionInterface> connection) {
  auto [iter, inserted] =
      connections_.emplace(connection->handle(), RegisteredConnection{std::move(connection)});
  BT_ASSERT(inserted);
}

void FakeScoDataChannel::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  BT_ASSERT(connections_.erase(handle) == 1);
}

void FakeScoDataChannel::OnOutboundPacketReadable() { readable_count_++; }

}  // namespace bt::hci
