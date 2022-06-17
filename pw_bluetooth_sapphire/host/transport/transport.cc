// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "transport.h"

#include <lib/async/default.h>
#include <lib/zx/channel.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include "device_wrapper.h"
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
  ZX_ASSERT(hci_);

  bt_log(INFO, "hci", "initializing HCI");

  bool success = hci_->Initialize([this](zx_status_t /*status*/) { OnChannelError(); });
  if (!success) {
    return;
  }

  command_channel_ = std::make_unique<CommandChannel>(hci_.get());
  command_channel_->set_channel_timeout_cb(fit::bind_member<&Transport::OnChannelError>(this));
}

Transport::~Transport() {
  bt_log(INFO, "hci", "transport shutting down");

  ResetChannels();

  bt_log(INFO, "hci", "transport shut down complete");
}

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

void Transport::SetTransportClosedCallback(fit::closure callback) {
  ZX_ASSERT(callback);
  ZX_ASSERT(!closed_cb_);
  closed_cb_ = std::move(callback);
}

void Transport::NotifyClosedCallback() {
  bt_log(INFO, "hci", "channel(s) were closed");
  if (closed_cb_) {
    closed_cb_();
  }
}

void Transport::OnChannelError() {
  bt_log(ERROR, "hci", "channel error");

  // TODO(fxbug.dev/52588): remove cleanup calls here as Host will destroy Transport in
  // closed callback
  ResetChannels();
  hci_.reset();

  NotifyClosedCallback();
}

void Transport::ResetChannels() {
  acl_data_channel_.reset();
  sco_data_channel_.reset();

  // Command channel must be shut down last because the data channels unregister events on
  // destruction.
  command_channel_.reset();
}

void Transport::AttachInspect(inspect::Node& parent, const std::string& name) {
  ZX_ASSERT(acl_data_channel_);
  hci_node_ = parent.CreateChild(name);

  if (command_channel_) {
    command_channel_->AttachInspect(hci_node_);
  }

  if (acl_data_channel_) {
    acl_data_channel_->AttachInspect(hci_node_, AclDataChannel::kInspectNodeName);
  }
}

}  // namespace bt::hci
