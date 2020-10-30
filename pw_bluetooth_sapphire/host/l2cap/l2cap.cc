// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "l2cap.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/task_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"

namespace bt::l2cap {

class Impl final : public L2cap {
 public:
  Impl(fxl::WeakPtr<hci::Transport> hci, bool random_channel_ids)
      : L2cap(), dispatcher_(async_get_default_dispatcher()), hci_(std::move(hci)) {
    ZX_ASSERT(hci_);
    ZX_ASSERT(hci_->acl_data_channel());
    const auto& acl_buffer_info = hci_->acl_data_channel()->GetBufferInfo();
    const auto& le_buffer_info = hci_->acl_data_channel()->GetLEBufferInfo();

    // LE Buffer Info is always available from ACLDataChannel.
    ZX_ASSERT(acl_buffer_info.IsAvailable());
    auto send_packets =
        fit::bind_member(hci_->acl_data_channel(), &hci::ACLDataChannel::SendPackets);
    auto drop_queued_acl =
        fit::bind_member(hci_->acl_data_channel(), &hci::ACLDataChannel::DropQueuedPackets);
    auto acl_priority = fit::bind_member(this, &Impl::RequestAclPriority);

    channel_manager_ = std::make_unique<ChannelManager>(
        acl_buffer_info.max_data_length(), le_buffer_info.max_data_length(),
        std::move(send_packets), std::move(drop_queued_acl), std::move(acl_priority),
        random_channel_ids);
    hci_->acl_data_channel()->SetDataRxHandler(channel_manager_->MakeInboundDataHandler());

    bt_log(DEBUG, "l2cap", "initialized");
  }

  ~Impl() {
    bt_log(DEBUG, "l2cap", "shutting down");

    ZX_ASSERT(hci_->acl_data_channel());
    hci_->acl_data_channel()->SetDataRxHandler(nullptr);

    channel_manager_ = nullptr;
  }

  void AddACLConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                        LinkErrorCallback link_error_callback,
                        SecurityUpgradeCallback security_callback) override {
    channel_manager_->RegisterACL(handle, role, std::move(link_error_callback),
                                  std::move(security_callback));
  }

  void AttachInspect(inspect::Node& parent, std::string name) override {
    node_ = parent.CreateChild(name);
  }

  LEFixedChannels AddLEConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                                  LinkErrorCallback link_error_callback,
                                  LEConnectionParameterUpdateCallback conn_param_callback,
                                  SecurityUpgradeCallback security_callback) override {
    channel_manager_->RegisterLE(handle, role, std::move(conn_param_callback),
                                 std::move(link_error_callback), std::move(security_callback));

    auto att = channel_manager_->OpenFixedChannel(handle, kATTChannelId);
    auto smp = channel_manager_->OpenFixedChannel(handle, kLESMPChannelId);
    ZX_ASSERT(att);
    ZX_ASSERT(smp);
    return LEFixedChannels{.att = std::move(att), .smp = std::move(smp)};
  }

  void RemoveConnection(hci::ConnectionHandle handle) override {
    channel_manager_->Unregister(handle);
  }

  void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                    sm::SecurityProperties security) override {
    channel_manager_->AssignLinkSecurityProperties(handle, security);
  }

  void RequestConnectionParameterUpdate(
      hci::ConnectionHandle handle, hci::LEPreferredConnectionParameters params,
      ConnectionParameterUpdateRequestCallback request_cb) override {
    channel_manager_->RequestConnectionParameterUpdate(handle, params, std::move(request_cb));
  }

  void OpenL2capChannel(hci::ConnectionHandle handle, PSM psm, ChannelParameters params,
                        ChannelCallback cb) override {
    channel_manager_->OpenChannel(handle, psm, params, std::move(cb));
  }

  void RegisterService(PSM psm, ChannelParameters params, ChannelCallback callback) override {
    const bool result = channel_manager_->RegisterService(psm, params, std::move(callback));
    ZX_DEBUG_ASSERT(result);
  }

  void UnregisterService(PSM psm) override { channel_manager_->UnregisterService(psm); }

 private:
  void RequestAclPriority(AclPriority priority, hci::ConnectionHandle handle,
                          fit::callback<void(fit::result<>)> cb) {
    bt_log(TRACE, "l2cap", "sending ACL priority command");

    bt_vendor_set_acl_priority_params_t priority_params = {
        .connection_handle = handle,
        .priority = static_cast<bt_vendor_acl_priority_t>((priority == AclPriority::kNormal)
                                                              ? BT_VENDOR_ACL_PRIORITY_NORMAL
                                                              : BT_VENDOR_ACL_PRIORITY_HIGH),
        .direction = static_cast<bt_vendor_acl_direction_t>((priority == AclPriority::kSource)
                                                                ? BT_VENDOR_ACL_DIRECTION_SOURCE
                                                                : BT_VENDOR_ACL_DIRECTION_SINK)};
    bt_vendor_params_t cmd_params = {.set_acl_priority = priority_params};
    auto encode_result = hci_->EncodeVendorCommand(BT_VENDOR_COMMAND_SET_ACL_PRIORITY, cmd_params);
    if (encode_result.is_error()) {
      bt_log(TRACE, "l2cap", "encoding ACL priority command failed");
      cb(fit::error());
      return;
    }
    auto encoded = encode_result.take_value();
    if (encoded.size() < sizeof(hci::CommandHeader)) {
      bt_log(TRACE, "l2cap", "encoded ACL priority command too small (size: %zu)", encoded.size());
      cb(fit::error());
      return;
    }

    hci::OpCode op_code = letoh16(encoded.As<hci::CommandHeader>().opcode);
    auto packet = bt::hci::CommandPacket::New(op_code, encoded.size() - sizeof(hci::CommandHeader));
    auto packet_view = packet->mutable_view()->mutable_data();
    encoded.Copy(&packet_view);

    hci_->command_channel()->SendCommand(
        std::move(packet),
        [cb = std::move(cb), priority](auto id, const hci::EventPacket& event) mutable {
          if (hci_is_error(event, WARN, "l2cap", "acl priority failed")) {
            cb(fit::error());
            return;
          }

          bt_log(DEBUG, "l2cap", "acl priority updated (priority: %#.8x)",
                 static_cast<uint32_t>(priority));
          cb(fit::ok());
        });
  }

  async_dispatcher_t* dispatcher_;

  // Inspect hierarchy node representing the data domain.
  inspect::Node node_;

  // Handle to the underlying HCI transport.
  fxl::WeakPtr<hci::Transport> hci_;

  std::unique_ptr<ChannelManager> channel_manager_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Impl);
};  // namespace bt::l2cap

// static
fbl::RefPtr<L2cap> L2cap::Create(fxl::WeakPtr<hci::Transport> hci, bool random_channel_ids) {
  ZX_DEBUG_ASSERT(hci);
  return AdoptRef(new Impl(hci, random_channel_ids));
}

}  // namespace bt::l2cap
