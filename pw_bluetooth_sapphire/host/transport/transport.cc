// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "transport.h"

#include <lib/async/default.h>
#include <lib/zx/channel.h>
#include <zircon/status.h>

#include "device_wrapper.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

namespace bt::hci {

std::unique_ptr<Transport> Transport::Create(std::unique_ptr<HciWrapper> hci) {
  auto transport = std::unique_ptr<Transport>(new Transport(std::move(hci)));
  if (!transport->command_channel()) {
    return nullptr;
  }
  return transport;
}

Transport::Transport(std::unique_ptr<HciWrapper> hci)
    : hci_(std::move(hci)), weak_ptr_factory_(this) {
  BT_ASSERT(hci_);

  bt_log(INFO, "hci", "initializing HCI");

  bool success = hci_->Initialize([this](zx_status_t /*status*/) { OnChannelError(); });
  if (!success) {
    return;
  }

  command_channel_ = std::make_unique<CommandChannel>(hci_.get());
  command_channel_->set_channel_timeout_cb(fit::bind_member<&Transport::OnChannelError>(this));
}

Transport::~Transport() { bt_log(INFO, "hci", "Transport shutting down"); }

bool Transport::InitializeACLDataChannel(const DataBufferInfo& bredr_buffer_info,
                                         const DataBufferInfo& le_buffer_info) {
  acl_data_channel_ = AclDataChannel::Create(this, hci_.get(), bredr_buffer_info, le_buffer_info);

  if (hci_node_) {
    acl_data_channel_->AttachInspect(hci_node_, AclDataChannel::kInspectNodeName);
  }

  return true;
}

bool Transport::InitializeScoDataChannel(const DataBufferInfo& buffer_info) {
  if (!buffer_info.IsAvailable()) {
    bt_log(WARN, "hci", "failed to initialize SCO data channel: buffer info is not available");
    return false;
  }

  if (!hci_->IsScoSupported()) {
    bt_log(WARN, "hci", "SCO not supported");
    return false;
  }

  sco_data_channel_ = ScoDataChannel::Create(buffer_info, command_channel_.get(), hci_.get());
  return true;
}

VendorFeaturesBits Transport::GetVendorFeatures() { return hci_->GetVendorFeatures(); }

void Transport::SetTransportErrorCallback(fit::closure callback) {
  BT_ASSERT(callback);
  BT_ASSERT(!error_cb_);
  error_cb_ = std::move(callback);
}

void Transport::OnChannelError() {
  bt_log(ERROR, "hci", "channel error, calling Transport error callback");
  // The channels should not be shut down yet. That should be left to higher layers so
  // dependent objects can be destroyed first.
  if (error_cb_) {
    error_cb_();
  }
}

void Transport::AttachInspect(inspect::Node& parent, const std::string& name) {
  BT_ASSERT(acl_data_channel_);
  hci_node_ = parent.CreateChild(name);

  if (command_channel_) {
    command_channel_->AttachInspect(hci_node_);
  }

  if (acl_data_channel_) {
    acl_data_channel_->AttachInspect(hci_node_, AclDataChannel::kInspectNodeName);
  }
}

}  // namespace bt::hci
