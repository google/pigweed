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

fpromise::result<std::unique_ptr<Transport>> Transport::Create(
    std::unique_ptr<DeviceWrapper> hci_device) {
  auto transport = std::unique_ptr<Transport>(new Transport(std::move(hci_device)));
  if (!transport->command_channel()) {
    return fpromise::error();
  }
  return fpromise::ok(std::move(transport));
}

Transport::Transport(std::unique_ptr<DeviceWrapper> hci_device)
    : hci_device_(std::move(hci_device)), weak_ptr_factory_(this) {
  ZX_ASSERT(hci_device_);

  bt_log(INFO, "hci", "initializing HCI");

  // Obtain command channel handle.
  zx::channel channel = hci_device_->GetCommandChannel();
  if (!channel.is_valid()) {
    bt_log(ERROR, "hci", "failed to obtain command channel handle");
    return;
  }

  // We watch for handle errors and closures to perform the necessary clean up.
  WatchChannelClosed(channel, cmd_channel_wait_);

  command_channel_ = std::make_unique<CommandChannel>(this, std::move(channel));
  command_channel_->set_channel_timeout_cb(fit::bind_member<&Transport::OnChannelError>(this));
}

Transport::~Transport() {
  ZX_ASSERT(thread_checker_.is_thread_valid());
  bt_log(INFO, "hci", "transport shutting down");

  ResetChannels();

  bt_log(INFO, "hci", "transport shut down complete");
}

bool Transport::InitializeACLDataChannel(const DataBufferInfo& bredr_buffer_info,
                                         const DataBufferInfo& le_buffer_info) {
  // Obtain ACL data channel handle.
  zx::channel channel = hci_device_->GetACLDataChannel();
  if (!channel.is_valid()) {
    bt_log(ERROR, "hci", "failed to obtain ACL data channel handle");
    return false;
  }

  // We watch for handle errors and closures to perform the necessary clean up.
  WatchChannelClosed(channel, acl_channel_wait_);

  acl_data_channel_ =
      AclDataChannel::Create(this, std::move(channel), bredr_buffer_info, le_buffer_info);

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

  fitx::result<zx_status_t, zx::channel> sco_result = hci_device_->GetScoChannel();
  if (sco_result.is_error()) {
    bt_log(WARN, "hci", "failed to obtain SCO channel handle: %s",
           zx_status_get_string(sco_result.error_value()));
    return false;
  }

  // We watch for handle errors and closures to perform the necessary clean up.
  WatchChannelClosed(sco_result.value(), sco_channel_wait_);

  sco_data_channel_ =
      ScoDataChannel::Create(std::move(sco_result.value()), buffer_info, this, hci_device_.get());
  return true;
}

bt_vendor_features_t Transport::GetVendorFeatures() { return hci_device_->GetVendorFeatures(); }

fpromise::result<DynamicByteBuffer> Transport::EncodeVendorCommand(bt_vendor_command_t command,
                                                                   bt_vendor_params_t& params) {
  return hci_device_->EncodeVendorCommand(command, params);
}

void Transport::SetTransportClosedCallback(fit::closure callback) {
  ZX_ASSERT(callback);
  ZX_ASSERT(!closed_cb_);
  closed_cb_ = std::move(callback);
}

void Transport::WatchChannelClosed(const zx::channel& channel, Waiter& wait) {
  wait.set_object(channel.get());
  wait.set_trigger(ZX_CHANNEL_PEER_CLOSED);
  zx_status_t status = wait.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "failed to set up closed handler: %s", zx_status_get_string(status));
    wait.set_object(ZX_HANDLE_INVALID);
  }
}

void Transport::OnChannelClosed(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                zx_status_t status, const zx_packet_signal_t* signal) {
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "channel error: %s", zx_status_get_string(status));
  } else {
    ZX_DEBUG_ASSERT(signal->observed & ZX_CHANNEL_PEER_CLOSED);
  }
  NotifyClosedCallback();
}

void Transport::NotifyClosedCallback() {
  // Clear the handlers so that we stop receiving events.
  cmd_channel_wait_.Cancel();
  acl_channel_wait_.Cancel();
  sco_channel_wait_.Cancel();

  bt_log(INFO, "hci", "channel(s) were closed");
  if (closed_cb_) {
    closed_cb_();
  }
}

void Transport::OnChannelError() {
  bt_log(ERROR, "hci", "channel error");

  // TODO(fxbug.dev/52588): remove call to ResetChannels() here as Host will destroy Transport in
  // closed callback
  ResetChannels();

  NotifyClosedCallback();
}

void Transport::ResetChannels() {
  // Waits must be canceled before channels are destroyed, invalidating the underlying channel
  // object.
  sco_channel_wait_.Cancel();
  cmd_channel_wait_.Cancel();
  acl_channel_wait_.Cancel();

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
