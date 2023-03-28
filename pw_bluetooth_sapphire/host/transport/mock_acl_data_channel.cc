// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/mock_acl_data_channel.h"

#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"

namespace bt::hci::testing {

void MockAclDataChannel::ReceivePacket(std::unique_ptr<ACLDataPacket> packet) {
  BT_ASSERT(data_rx_handler_);
  data_rx_handler_(std::move(packet));
}

void MockAclDataChannel::SetDataRxHandler(ACLPacketHandler rx_callback) {
  BT_ASSERT(rx_callback);
  data_rx_handler_ = std::move(rx_callback);
}

void MockAclDataChannel::RegisterConnection(WeakPtr<ConnectionInterface> connection) {
  bt_log(DEBUG, "hci", "ACL register connection (handle: %#.4x)", connection->handle());
  auto [_, inserted] = registered_connections_.emplace(connection->handle(), connection);
  BT_ASSERT_MSG(inserted, "connection with handle %#.4x already registered", connection->handle());
}

void MockAclDataChannel::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "ACL unregister link (handle: %#.4x)", handle);
  auto iter = registered_connections_.find(handle);
  if (iter == registered_connections_.end()) {
    // handle not registered
    bt_log(WARN, "hci", "attempt to unregister link that is not registered (handle: %#.4x)",
           handle);
    return;
  }
  registered_connections_.erase(iter);
}

void MockAclDataChannel::OnOutboundPacketAvailable() {
  // Assume there is infinite buffer space available
  SendPackets();
}

const DataBufferInfo& MockAclDataChannel::GetBufferInfo() const { return bredr_buffer_info_; }

const DataBufferInfo& MockAclDataChannel::GetLeBufferInfo() const { return le_buffer_info_; }

void MockAclDataChannel::RequestAclPriority(
    pw::bluetooth::AclPriority priority, hci_spec::ConnectionHandle handle,
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
