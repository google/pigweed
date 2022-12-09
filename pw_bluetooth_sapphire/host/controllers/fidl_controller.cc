// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fidl_controller.h"

#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/slab_allocators.h"
#include "zircon/status.h"

namespace bt::controllers {

FidlController::FidlController(fuchsia::hardware::bluetooth::HciHandle hci,
                               async_dispatcher_t* dispatcher)
    : hci_handle_(std::move(hci)), dispatcher_(dispatcher) {
  BT_ASSERT(hci_handle_.is_valid());
}

FidlController::~FidlController() { CleanUp(); }

void FidlController::Initialize(PwStatusCallback complete_callback,
                                PwStatusCallback error_callback) {
  error_cb_ = std::move(error_callback);

  // We wait to bind hci_ until initialization because otherwise errors are dropped if the async
  // loop runs between Bind() and set_error_handle(). set_error_handler() will not be called
  // synchronously, so there is no risk that OnError() is called immediately.
  hci_ = hci_handle_.Bind();
  hci_.set_error_handler([this](zx_status_t status) {
    bt_log(ERROR, "controllers", "BtHci protocol closed: %s", zx_status_get_string(status));
    OnError(status);
  });

  zx::channel their_command_chan;
  zx_status_t status = zx::channel::create(0, &command_channel_, &their_command_chan);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to create command channel");
    hci_.Unbind();
    complete_callback(pw::Status::Internal());
    return;
  }

  hci_->OpenCommandChannel(std::move(their_command_chan));
  InitializeWait(command_wait_, command_channel_);

  zx::channel their_acl_chan;
  status = zx::channel::create(0, &acl_channel_, &their_acl_chan);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "Failed to create ACL channel");
    hci_.Unbind();
    complete_callback(pw::Status::Internal());
    return;
  }

  hci_->OpenAclDataChannel(std::move(their_acl_chan));
  InitializeWait(acl_wait_, acl_channel_);

  complete_callback(PW_STATUS_OK);
}

void FidlController::Close(PwStatusCallback callback) {
  CleanUp();
  callback(PW_STATUS_OK);
}

void FidlController::SendCommand(pw::span<const std::byte> command) {
  zx_status_t status =
      command_channel_.write(/*flags=*/0, command.data(), static_cast<uint32_t>(command.size()),
                             /*handles=*/nullptr, /*num_handles=*/0);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "failed to write command channel: %s",
           zx_status_get_string(status));
    OnError(status);
  }
}

void FidlController::SendAclData(pw::span<const std::byte> data) {
  zx_status_t status =
      acl_channel_.write(/*flags=*/0, data.data(), static_cast<uint32_t>(data.size()),
                         /*handles=*/nullptr, /*num_handles=*/0);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "failed to write ACL channel: %s", zx_status_get_string(status));
    OnError(status);
  }
}

void FidlController::OnError(zx_status_t status) {
  CleanUp();

  if (error_cb_) {
    error_cb_(ZxStatusToPwStatus(status));
  }
}

void FidlController::CleanUp() {
  // Waits need to be canceled before the underlying channels are destroyed.
  acl_wait_.Cancel();
  command_wait_.Cancel();

  acl_channel_.reset();
  command_channel_.reset();
}

void FidlController::InitializeWait(async::WaitBase& wait, zx::channel& channel) {
  BT_ASSERT(channel.is_valid());
  wait.Cancel();
  wait.set_object(channel.get());
  wait.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
  BT_ASSERT(wait.Begin(dispatcher_) == ZX_OK);
}

void FidlController::OnChannelSignal(const char* chan_name, zx_status_t status,
                                     async::WaitBase* wait, const zx_packet_signal_t* signal,
                                     pw::span<std::byte> buffer, zx::channel& channel,
                                     DataFunction& data_cb) {
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s channel error: %s", chan_name, zx_status_get_string(status));
    OnError(status);
    return;
  }

  if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
    bt_log(ERROR, "controllers", "%s channel closed", chan_name);
    OnError(ZX_ERR_PEER_CLOSED);
    return;
  }
  BT_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  uint32_t read_size;
  zx_status_t read_status = channel.read(0u, buffer.data(), /*handles=*/nullptr,
                                         static_cast<uint32_t>(buffer.size()), 0, &read_size,
                                         /*actual_handles=*/nullptr);
  if (read_status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s channel: failed to read RX bytes: %s", chan_name,
           zx_status_get_string(read_status));
    OnError(read_status);
    return;
  }
  if (data_cb) {
    data_cb(buffer.subspan(0, read_size));
  } else {
    bt_log(WARN, "controllers", "Dropping packet received on %s channel (no rx callback set)",
           chan_name);
  }

  // The wait needs to be restarted after every signal.
  status = wait->Begin(dispatcher_);
  if (status != ZX_OK) {
    bt_log(ERROR, "controllers", "%s wait error: %s", chan_name, zx_status_get_string(status));
    OnError(status);
  }
}

void FidlController::OnAclSignal(async_dispatcher_t* /*dispatcher*/, async::WaitBase* wait,
                                 zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "FidlController::OnAclSignal");

  // Allocate a buffer for the packet. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  std::byte packet[hci::allocators::kLargeACLDataPacketSize];
  OnChannelSignal("ACL", status, wait, signal, packet, acl_channel_, acl_cb_);
}

void FidlController::OnCommandSignal(async_dispatcher_t* /*dispatcher*/, async::WaitBase* wait,
                                     zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "FidlController::OnCommandSignal");

  // Allocate a buffer for the packet. Since we don't know the size beforehand we allocate the
  // largest possible buffer.
  std::byte packet[hci_spec::kMaxEventPacketPayloadSize + sizeof(hci_spec::EventHeader)];
  OnChannelSignal("command", status, wait, signal, packet, command_channel_, event_cb_);
}
}  // namespace bt::controllers
