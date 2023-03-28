// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

namespace bt::hci {
namespace {

constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;

class FakeAclConnection : public AclDataChannel::ConnectionInterface {
 public:
  explicit FakeAclConnection(AclDataChannel* data_channel,
                             hci_spec::ConnectionHandle handle = kTestHandle,
                             bt::LinkType type = bt::LinkType::kACL)
      : handle_(handle), type_(type), data_channel_(data_channel), weak_interface_(this) {}

  ~FakeAclConnection() override = default;

  void QueuePacket(ACLDataPacketPtr packet) {
    queued_packets_.push(std::move(packet));
    data_channel_->OnOutboundPacketAvailable();
  }

  const std::queue<ACLDataPacketPtr>& queued_packets() const { return queued_packets_; }

  WeakPtr<ConnectionInterface> GetWeakPtr() { return weak_interface_.GetWeakPtr(); }

  // AclDataChannel::ConnectionInterface overrides:
  hci_spec::ConnectionHandle handle() const override { return handle_; }

  bt::LinkType type() const override { return type_; }

  ACLDataPacketPtr GetNextOutboundPacket() override {
    if (queued_packets_.empty()) {
      return nullptr;
    }
    ACLDataPacketPtr packet = std::move(queued_packets_.front());
    queued_packets_.pop();
    return packet;
  }

  bool HasAvailablePacket() const override { return !queued_packets_.empty(); }

 private:
  hci_spec::ConnectionHandle handle_;
  bt::LinkType type_;
  AclDataChannel* data_channel_;
  std::queue<ACLDataPacketPtr> queued_packets_;
  WeakSelf<ConnectionInterface> weak_interface_;
};
}  // namespace
}  // namespace bt::hci
