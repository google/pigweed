// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_

#include "lib/inspect/cpp/vmo/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

namespace bt::hci::testing {

class MockAclDataChannel final : public AclDataChannel {
 public:
  MockAclDataChannel() = default;
  ~MockAclDataChannel() override = default;

  using SendPacketsCallback = fit::function<bool(
      LinkedList<ACLDataPacket> packets, UniqueChannelId channel_id, PacketPriority priority)>;
  void set_send_packets_cb(SendPacketsCallback cb) { send_packets_cb_ = std::move(cb); }

  using DropQueuedPacketsCallback = fit::function<void(AclPacketPredicate predicate)>;
  void set_drop_queued_packets_cb(DropQueuedPacketsCallback cb) {
    drop_queued_packets_cb_ = std::move(cb);
  }

  using RequestAclPriorityCallback =
      fit::function<void(hci::AclPriority priority, hci_spec::ConnectionHandle handle,
                         fit::callback<void(fitx::result<fitx::failed>)> callback)>;
  void set_request_acl_priority_cb(RequestAclPriorityCallback cb) {
    request_acl_priority_cb_ = std::move(cb);
  }

  using SetBrEdrAutomaticFlushTimeoutCallback =
      fit::function<void(zx::duration, hci_spec::ConnectionHandle, ResultCallback<>)>;
  void set_set_bredr_automatic_flush_timeout_cb(SetBrEdrAutomaticFlushTimeoutCallback callback) {
    flush_timeout_cb_ = std::move(callback);
  }

  // AclDataChannel overrides:
  void Initialize(const DataBufferInfo& bredr_buffer_info,
                  const DataBufferInfo& le_buffer_info) override {}
  void AttachInspect(inspect::Node& /*unused*/, std::string /*unused*/) override {}
  void ShutDown() override {}
  void SetDataRxHandler(ACLPacketHandler rx_callback) override {}
  bool SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                  PacketPriority priority) override {
    return false;
  }
  bool SendPackets(LinkedList<ACLDataPacket> packets, UniqueChannelId channel_id,
                   PacketPriority priority) override {
    if (send_packets_cb_) {
      return send_packets_cb_(std::move(packets), channel_id, priority);
    }
    return true;
  }
  void RegisterLink(hci_spec::ConnectionHandle handle, bt::LinkType ll_type) override {}
  void UnregisterLink(hci_spec::ConnectionHandle handle) override {}
  void DropQueuedPackets(AclPacketPredicate predicate) override {
    if (drop_queued_packets_cb_) {
      drop_queued_packets_cb_(std::move(predicate));
    }
  }
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override {}
  const DataBufferInfo& GetBufferInfo() const override { return bredr_buffer_info_; }
  const DataBufferInfo& GetLeBufferInfo() const override { return le_buffer_info_; }
  void RequestAclPriority(hci::AclPriority priority, hci_spec::ConnectionHandle handle,
                          fit::callback<void(fitx::result<fitx::failed>)> callback) override {
    if (request_acl_priority_cb_) {
      request_acl_priority_cb_(priority, handle, std::move(callback));
    }
  }

  void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout, hci_spec::ConnectionHandle handle,
                                     ResultCallback<> callback) override {
    if (flush_timeout_cb_) {
      flush_timeout_cb_(flush_timeout, handle, std::move(callback));
    }
  }

 private:
  DataBufferInfo bredr_buffer_info_;
  DataBufferInfo le_buffer_info_;

  SendPacketsCallback send_packets_cb_;
  DropQueuedPacketsCallback drop_queued_packets_cb_;
  RequestAclPriorityCallback request_acl_priority_cb_;
  SetBrEdrAutomaticFlushTimeoutCallback flush_timeout_cb_;
};

}  // namespace bt::hci::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_MOCK_ACL_DATA_CHANNEL_H_
