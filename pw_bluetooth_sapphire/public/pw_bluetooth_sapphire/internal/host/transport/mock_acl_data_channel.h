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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_

#include "pw_bluetooth_sapphire/internal/host/common/inspect.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_channel.h"
#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"

namespace bt::hci::testing {

class MockAclDataChannel final : public AclDataChannel {
 public:
  MockAclDataChannel() = default;
  ~MockAclDataChannel() override = default;

  void set_bredr_buffer_info(DataBufferInfo info) { bredr_buffer_info_ = info; }
  void set_le_buffer_info(DataBufferInfo info) { le_buffer_info_ = info; }

  using SendPacketsCallback =
      fit::function<bool(std::list<ACLDataPacketPtr> packets)>;
  // using SendPacketsCallback = fit::function<bool(WeakPtr<ConnectionInterface>
  // connection)>;
  void set_send_packets_cb(SendPacketsCallback cb) {
    send_packets_cb_ = std::move(cb);
  }

  using DropQueuedPacketsCallback =
      fit::function<void(AclPacketPredicate predicate)>;
  void set_drop_queued_packets_cb(DropQueuedPacketsCallback cb) {
    drop_queued_packets_cb_ = std::move(cb);
  }

  using RequestAclPriorityCallback = fit::function<void(
      pw::bluetooth::AclPriority priority,
      hci_spec::ConnectionHandle handle,
      fit::callback<void(fit::result<fit::failed>)> callback)>;
  void set_request_acl_priority_cb(RequestAclPriorityCallback cb) {
    request_acl_priority_cb_ = std::move(cb);
  }

  void ReceivePacket(std::unique_ptr<ACLDataPacket> packet);

  // AclDataChannel overrides:
  void AttachInspect(inspect::Node& /*unused*/,
                     const std::string& /*unused*/) override {}
  void SetDataRxHandler(ACLPacketHandler rx_callback) override;
  void RegisterConnection(WeakPtr<ConnectionInterface> connection) override;
  void UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  void OnOutboundPacketAvailable() override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override {}
  const DataBufferInfo& GetBufferInfo() const override;
  const DataBufferInfo& GetLeBufferInfo() const override;
  void RequestAclPriority(
      pw::bluetooth::AclPriority priority,
      hci_spec::ConnectionHandle handle,
      fit::callback<void(fit::result<fit::failed>)> callback) override;

 private:
  using ConnectionMap = std::unordered_map<hci_spec::ConnectionHandle,
                                           WeakPtr<ConnectionInterface>>;

  void IncrementRoundRobinIterator(ConnectionMap::iterator& conn_iter,
                                   bt::LinkType connection_type);
  void SendPackets();

  ConnectionMap registered_connections_;

  DataBufferInfo bredr_buffer_info_;
  DataBufferInfo le_buffer_info_;

  ACLPacketHandler data_rx_handler_;
  SendPacketsCallback send_packets_cb_;
  DropQueuedPacketsCallback drop_queued_packets_cb_;
  RequestAclPriorityCallback request_acl_priority_cb_;
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_
