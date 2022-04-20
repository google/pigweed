// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "l2cap.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::l2cap {

class Impl final : public L2cap {
 public:
  Impl(hci::AclDataChannel* acl_data_channel, bool random_channel_ids)
      : L2cap(), dispatcher_(async_get_default_dispatcher()), acl_data_channel_(acl_data_channel) {
    const auto& acl_buffer_info = acl_data_channel_->GetBufferInfo();
    const auto& le_buffer_info = acl_data_channel_->GetLeBufferInfo();

    // LE Buffer Info is always available from ACLDataChannel.
    ZX_ASSERT(acl_buffer_info.IsAvailable());

    channel_manager_ = std::make_unique<ChannelManager>(acl_buffer_info.max_data_length(),
                                                        le_buffer_info.max_data_length(),
                                                        acl_data_channel_, random_channel_ids);
    acl_data_channel_->SetDataRxHandler(channel_manager_->MakeInboundDataHandler());

    bt_log(DEBUG, "l2cap", "initialized");
  }

  ~Impl() {
    bt_log(DEBUG, "l2cap", "shutting down");
    acl_data_channel_->SetDataRxHandler(nullptr);
    channel_manager_ = nullptr;
  }

  void AddACLConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                        LinkErrorCallback link_error_callback,
                        SecurityUpgradeCallback security_callback) override {
    channel_manager_->RegisterACL(handle, role, std::move(link_error_callback),
                                  std::move(security_callback));
  }

  void AttachInspect(inspect::Node& parent, std::string name) override {
    node_ = parent.CreateChild(name);
    channel_manager_->AttachInspect(node_);
  }

  LEFixedChannels AddLEConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
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

  void RemoveConnection(hci_spec::ConnectionHandle handle) override {
    channel_manager_->Unregister(handle);
  }

  void AssignLinkSecurityProperties(hci_spec::ConnectionHandle handle,
                                    sm::SecurityProperties security) override {
    channel_manager_->AssignLinkSecurityProperties(handle, security);
  }

  void RequestConnectionParameterUpdate(
      hci_spec::ConnectionHandle handle, hci_spec::LEPreferredConnectionParameters params,
      ConnectionParameterUpdateRequestCallback request_cb) override {
    channel_manager_->RequestConnectionParameterUpdate(handle, params, std::move(request_cb));
  }

  void OpenL2capChannel(hci_spec::ConnectionHandle handle, PSM psm, ChannelParameters params,
                        ChannelCallback cb) override {
    channel_manager_->OpenChannel(handle, psm, params, std::move(cb));
  }

  void RegisterService(PSM psm, ChannelParameters params, ChannelCallback callback) override {
    const bool result = channel_manager_->RegisterService(psm, params, std::move(callback));
    ZX_DEBUG_ASSERT(result);
  }

  void UnregisterService(PSM psm) override { channel_manager_->UnregisterService(psm); }

 private:
  async_dispatcher_t* dispatcher_;

  // Inspect hierarchy node representing the data domain.
  inspect::Node node_;

  // Handle to the underlying HCI transport.
  hci::AclDataChannel* acl_data_channel_;

  std::unique_ptr<ChannelManager> channel_manager_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Impl);
};  // namespace bt::l2cap

// static
fbl::RefPtr<L2cap> L2cap::Create(hci::AclDataChannel* acl_data_channel, bool random_channel_ids) {
  ZX_ASSERT(acl_data_channel);
  return AdoptRef(new Impl(acl_data_channel, random_channel_ids));
}

}  // namespace bt::l2cap
