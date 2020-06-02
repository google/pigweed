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

namespace bt {
namespace hci {

fit::result<std::unique_ptr<Transport>> Transport::Create(
    std::unique_ptr<DeviceWrapper> hci_device) {
  auto transport = std::unique_ptr<Transport>(new Transport(std::move(hci_device)));
  if (!transport->command_channel()) {
    return fit::error();
  }
  return fit::ok(std::move(transport));
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

  auto command_channel_result = CommandChannel::Create(this, std::move(channel));
  if (command_channel_result.is_error()) {
    return;
  }
  command_channel_ = command_channel_result.take_value();
  command_channel_->set_channel_timeout_cb(fit::bind_member(this, &Transport::OnChannelError));
}

Transport::~Transport() {
  ZX_ASSERT(thread_checker_.IsCreationThreadCurrent());

  bt_log(INFO, "hci", "transport shutting down");

  if (acl_data_channel_) {
    acl_data_channel_->ShutDown();
  }
  if (command_channel_) {
    command_channel_->ShutDown();
  }

  cmd_channel_wait_.Cancel();
  if (acl_data_channel_) {
    acl_channel_wait_.Cancel();
  }

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

  acl_data_channel_ = std::make_unique<ACLDataChannel>(this, std::move(channel));
  acl_data_channel_->Initialize(bredr_buffer_info, le_buffer_info);

  return true;
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
  if (acl_data_channel_) {
    acl_channel_wait_.Cancel();
  }

  bt_log(INFO, "hci", "channel(s) were closed");
  if (closed_cb_) {
    closed_cb_();
  }
}

void Transport::OnChannelError() {
  bt_log(ERROR, "hci", "channel error");

  // TODO(52588): remove calls to ShutDown() here as Host will destroy Transport in closed callback
  if (acl_data_channel_) {
    acl_data_channel_->ShutDown();
  }
  if (command_channel_) {
    command_channel_->ShutDown();
  }

  NotifyClosedCallback();
}

}  // namespace hci
}  // namespace bt
