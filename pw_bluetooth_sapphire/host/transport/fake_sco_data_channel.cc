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

#include "pw_bluetooth_sapphire/internal/host/transport/fake_sco_data_channel.h"

namespace bt::hci {

void FakeScoDataChannel::RegisterConnection(
    WeakPtr<ConnectionInterface> connection) {
  auto [iter, inserted] = connections_.emplace(
      connection->handle(), RegisteredConnection{std::move(connection)});
  BT_ASSERT(inserted);
}

void FakeScoDataChannel::UnregisterConnection(
    hci_spec::ConnectionHandle handle) {
  BT_ASSERT(connections_.erase(handle) == 1);
}

void FakeScoDataChannel::OnOutboundPacketReadable() { readable_count_++; }

}  // namespace bt::hci
