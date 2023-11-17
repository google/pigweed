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

#include "pw_bluetooth_sapphire/internal/host/transport/mock_acl_data_channel.h"

#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"

namespace bt::hci::testing {

void MockAclDataChannel::ReceivePacket(std::unique_ptr<ACLDataPacket> packet) {
  BT_ASSERT(data_rx_handler_);
  data_rx_handler_(std::move(packet));
}

void MockAclDataChannel::SetDataRxHandler(ACLPacketHandler rx_callback) {
  BT_ASSERT(rx_callback);
  data_rx_handler_ = std::move(rx_callback);
}

void MockAclDataChannel::RegisterConnection(
    WeakPtr<ConnectionInterface> connection) {
  bt_log(DEBUG,
         "hci",
         "ACL register connection (handle: %#.4x)",
         connection->handle());
  auto [_, inserted] =
      registered_connections_.emplace(connection->handle(), connection);
  BT_ASSERT_MSG(inserted,
                "connection with handle %#.4x already registered",
                connection->handle());
}

void MockAclDataChannel::UnregisterConnection(
    hci_spec::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "ACL unregister link (handle: %#.4x)", handle);
  auto iter = registered_connections_.find(handle);
  if (iter == registered_connections_.end()) {
    // handle not registered
    bt_log(WARN,
           "hci",
           "attempt to unregister link that is not registered (handle: %#.4x)",
           handle);
    return;
  }
  registered_connections_.erase(iter);
}

void MockAclDataChannel::OnOutboundPacketAvailable() {
  // Assume there is infinite buffer space available
  SendPackets();
}

const DataBufferInfo& MockAclDataChannel::GetBufferInfo() const {
  return bredr_buffer_info_;
}

const DataBufferInfo& MockAclDataChannel::GetLeBufferInfo() const {
  return le_buffer_info_;
}

void MockAclDataChannel::RequestAclPriority(
    pw::bluetooth::AclPriority priority,
    hci_spec::ConnectionHandle handle,
    fit::callback<void(fit::result<fit::failed>)> callback) {
  if (request_acl_priority_cb_) {
    request_acl_priority_cb_(priority, handle, std::move(callback));
  }
}

void MockAclDataChannel::SendPackets() {
  std::list<ACLDataPacketPtr> packets;
  for (auto& [_, connection] : registered_connections_) {
    while (connection->HasAvailablePacket()) {
      packets.push_back(connection->GetNextOutboundPacket());
    }
  }
  if (send_packets_cb_) {
    send_packets_cb_(std::move(packets));
  }
}

}  // namespace bt::hci::testing
