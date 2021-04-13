// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller_test_double_base.h"

#include <lib/async/default.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"

namespace bt::testing {

ControllerTestDoubleBase::ControllerTestDoubleBase() {}

ControllerTestDoubleBase::~ControllerTestDoubleBase() {
  // When this destructor gets called any subclass state will be undefined. If
  // Stop() has not been called before reaching this point this can cause
  // runtime errors when our event loop handlers attempt to invoke the pure
  // virtual methods of this class.
}

bool ControllerTestDoubleBase::StartCmdChannel(zx::channel chan) {
  if (cmd_channel_.is_valid()) {
    return false;
  }

  cmd_channel_ = std::move(chan);
  cmd_channel_wait_.set_object(cmd_channel_.get());
  cmd_channel_wait_.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
  zx_status_t status = cmd_channel_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    cmd_channel_.reset();
    bt_log(WARN, "fake-hci", "failed to start command channel: %s", zx_status_get_string(status));
    return false;
  }
  return true;
}

bool ControllerTestDoubleBase::StartAclChannel(zx::channel chan) {
  if (acl_channel_.is_valid()) {
    return false;
  }

  acl_channel_ = std::move(chan);
  acl_channel_wait_.set_object(acl_channel_.get());
  acl_channel_wait_.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
  zx_status_t status = acl_channel_wait_.Begin(async_get_default_dispatcher());
  if (status != ZX_OK) {
    acl_channel_.reset();
    bt_log(WARN, "fake-hci", "failed to start ACL channel: %s", zx_status_get_string(status));
    return false;
  }
  return true;
}

bool ControllerTestDoubleBase::StartSnoopChannel(zx::channel chan) {
  if (snoop_channel_.is_valid()) {
    return false;
  }
  snoop_channel_ = std::move(chan);
  return true;
}

void ControllerTestDoubleBase::Stop() {
  CloseCommandChannel();
  CloseACLDataChannel();
  CloseSnoopChannel();
}

zx_status_t ControllerTestDoubleBase::SendCommandChannelPacket(const ByteBuffer& packet) {
  zx_status_t status = cmd_channel_.write(0, packet.data(), packet.size(), nullptr, 0);
  if (status != ZX_OK) {
    bt_log(WARN, "fake-hci", "failed to write to control channel: %s",
           zx_status_get_string(status));
    return status;
  }

  SendSnoopChannelPacket(packet, BT_HCI_SNOOP_TYPE_EVT, true);

  return ZX_OK;
}

zx_status_t ControllerTestDoubleBase::SendACLDataChannelPacket(const ByteBuffer& packet) {
  zx_status_t status = acl_channel_.write(0, packet.data(), packet.size(), nullptr, 0);
  if (status != ZX_OK) {
    bt_log(WARN, "fake-hci", "failed to write to ACL data channel: %s",
           zx_status_get_string(status));
    return status;
  }

  SendSnoopChannelPacket(packet, BT_HCI_SNOOP_TYPE_ACL, true);

  return ZX_OK;
}

void ControllerTestDoubleBase::SendSnoopChannelPacket(const ByteBuffer& packet,
                                                      bt_hci_snoop_type_t packet_type,
                                                      bool is_received) {
  if (snoop_channel_.is_valid()) {
    uint8_t snoop_buffer[packet.size() + 1];
    uint8_t flags = bt_hci_snoop_flags(packet_type, is_received);

    snoop_buffer[0] = flags;
    memcpy(snoop_buffer + 1, packet.data(), packet.size());
    zx_status_t status = snoop_channel_.write(0, snoop_buffer, packet.size() + 1, nullptr, 0);
    if (status != ZX_OK) {
      bt_log(WARN, "fake-hci", "cleaning up snoop channel after failed write: %s",
             zx_status_get_string(status));
      CloseSnoopChannel();
    }
  }
}

void ControllerTestDoubleBase::CloseCommandChannel() {
  if (cmd_channel_.is_valid()) {
    cmd_channel_wait_.Cancel();
    cmd_channel_wait_.set_object(ZX_HANDLE_INVALID);
    cmd_channel_.reset();
  }
}

void ControllerTestDoubleBase::CloseACLDataChannel() {
  if (acl_channel_.is_valid()) {
    acl_channel_wait_.Cancel();
    acl_channel_wait_.set_object(ZX_HANDLE_INVALID);
    acl_channel_.reset();
  }
}

void ControllerTestDoubleBase::CloseSnoopChannel() {
  if (snoop_channel_.is_valid()) {
    snoop_channel_.reset();
  }
}

void ControllerTestDoubleBase::HandleCommandPacket(async_dispatcher_t* dispatcher,
                                                   async::WaitBase* wait, zx_status_t wait_status,
                                                   const zx_packet_signal_t* signal) {
  StaticByteBuffer<hci::kMaxCommandPacketPayloadSize> buffer;
  uint32_t read_size;
  zx_status_t status = cmd_channel_.read(0u, buffer.mutable_data(), nullptr,
                                         hci::kMaxCommandPacketPayloadSize, 0, &read_size, nullptr);
  ZX_DEBUG_ASSERT(status == ZX_OK || status == ZX_ERR_PEER_CLOSED);
  if (status < 0) {
    if (status == ZX_ERR_PEER_CLOSED) {
      bt_log(INFO, "fake-hci", "command channel was closed");
    } else {
      bt_log(ERROR, "fake-hci", "failed to read on cmd channel: %s", zx_status_get_string(status));
    }
    CloseCommandChannel();
    return;
  }

  if (read_size < sizeof(hci::CommandHeader)) {
    bt_log(ERROR, "fake-hci", "malformed command packet received");
  } else {
    MutableBufferView view(buffer.mutable_data(), read_size);
    PacketView<hci::CommandHeader> packet(&view, read_size - sizeof(hci::CommandHeader));
    SendSnoopChannelPacket(packet.data(), BT_HCI_SNOOP_TYPE_CMD, false);
    OnCommandPacketReceived(packet);
  }

  status = wait->Begin(dispatcher);
  if (status != ZX_OK) {
    bt_log(ERROR, "fake-hci", "failed to wait on cmd channel: %s", zx_status_get_string(status));
    CloseCommandChannel();
  }
}

void ControllerTestDoubleBase::HandleACLPacket(async_dispatcher_t* dispatcher,
                                               async::WaitBase* wait, zx_status_t wait_status,
                                               const zx_packet_signal_t* signal) {
  StaticByteBuffer<hci::kMaxACLPayloadSize + sizeof(hci::ACLDataHeader)> buffer;
  uint32_t read_size;
  zx_status_t status =
      acl_channel_.read(0u, buffer.mutable_data(), nullptr, buffer.size(), 0, &read_size, nullptr);
  ZX_DEBUG_ASSERT(status == ZX_OK || status == ZX_ERR_PEER_CLOSED);
  if (status < 0) {
    if (status == ZX_ERR_PEER_CLOSED) {
      bt_log(INFO, "fake-hci", "ACL channel was closed");
    } else {
      bt_log(ERROR, "fake-hci", "failed to read on ACL channel: %s", zx_status_get_string(status));
    }

    CloseACLDataChannel();
    return;
  }

  BufferView view(buffer.data(), read_size);
  SendSnoopChannelPacket(view, BT_HCI_SNOOP_TYPE_ACL, false);
  OnACLDataPacketReceived(view);

  status = wait->Begin(dispatcher);
  if (status != ZX_OK) {
    bt_log(ERROR, "fake-hci", "failed to wait on ACL channel: %s", zx_status_get_string(status));
    CloseACLDataChannel();
  }
}

}  // namespace bt::testing
