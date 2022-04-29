// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "controller_test_double_base.h"

#include <lib/async/cpp/task.h>
#include <lib/async/default.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"

namespace bt::testing {

ControllerTestDoubleBase::ControllerTestDoubleBase() {}

ControllerTestDoubleBase::~ControllerTestDoubleBase() {
  // When this destructor gets called any subclass state will be undefined. If
  // Stop() has not been called before reaching this point this can cause
  // runtime errors when our event loop handlers attempt to invoke the pure
  // virtual methods of this class.
}

void SignalError(zx_status_t status);

// Sets a callback that will be called when an error is signaled with SignalError().
void SetErrorCallback(fit::callback<void(zx_status_t)> callback);

void ControllerTestDoubleBase::StartCmdChannel(
    fit::function<void(std::unique_ptr<hci::EventPacket>)> packet_cb) {
  send_event_ = std::move(packet_cb);
}

void ControllerTestDoubleBase::StartAclChannel(
    fit::function<void(std::unique_ptr<hci::ACLDataPacket>)> packet_cb) {
  send_acl_packet_ = std::move(packet_cb);
}

void ControllerTestDoubleBase::StartScoChannel(
    fit::function<void(std::unique_ptr<hci::ScoDataPacket>)> packet_cb) {
  send_sco_packet_ = std::move(packet_cb);
}

bool ControllerTestDoubleBase::StartSnoopChannel(zx::channel chan) {
  if (snoop_channel_.is_valid()) {
    return false;
  }
  snoop_channel_ = std::move(chan);
  return true;
}

void ControllerTestDoubleBase::Stop(zx_status_t status) {
  send_event_ = nullptr;
  send_acl_packet_ = nullptr;
  send_sco_packet_ = nullptr;
  CloseSnoopChannel();
  SignalError(status);
}

zx_status_t ControllerTestDoubleBase::SendCommandChannelPacket(const ByteBuffer& packet) {
  if (!send_event_) {
    return ZX_ERR_IO_REFUSED;
  }

  if (packet.size() < sizeof(hci_spec::EventHeader)) {
    return ZX_ERR_IO_INVALID;
  }

  std::unique_ptr<hci::EventPacket> event =
      hci::EventPacket::New(packet.size() - sizeof(hci_spec::EventHeader));
  event->mutable_view()->mutable_data().Write(packet);

  if (event->view().header().parameter_total_size != event->view().payload_size()) {
    return ZX_ERR_IO_INVALID;
  }

  event->InitializeFromBuffer();

  send_event_(std::move(event));
  SendSnoopChannelPacket(packet, BT_HCI_SNOOP_TYPE_EVT, /*is_received=*/true);

  return ZX_OK;
}

zx_status_t ControllerTestDoubleBase::SendACLDataChannelPacket(const ByteBuffer& packet) {
  if (!send_acl_packet_) {
    return ZX_ERR_IO_REFUSED;
  }

  if (packet.size() < sizeof(hci_spec::ACLDataHeader)) {
    return ZX_ERR_IO_INVALID;
  }

  std::unique_ptr<hci::ACLDataPacket> acl_packet =
      hci::ACLDataPacket::New(packet.size() - sizeof(hci_spec::ACLDataHeader));
  acl_packet->mutable_view()->mutable_data().Write(packet);

  if (acl_packet->view().header().data_total_length != acl_packet->view().payload_size()) {
    return ZX_ERR_IO_INVALID;
  }

  acl_packet->InitializeFromBuffer();

  send_acl_packet_(std::move(acl_packet));
  SendSnoopChannelPacket(packet, BT_HCI_SNOOP_TYPE_ACL, /*is_received=*/true);

  return ZX_OK;
}

zx_status_t ControllerTestDoubleBase::SendScoDataChannelPacket(const ByteBuffer& packet) {
  if (!send_sco_packet_) {
    return ZX_ERR_IO_REFUSED;
  }

  if (packet.size() < sizeof(hci_spec::SynchronousDataHeader)) {
    return ZX_ERR_IO_INVALID;
  }

  std::unique_ptr<hci::ScoDataPacket> sco_packet =
      hci::ScoDataPacket::New(packet.size() - sizeof(hci_spec::SynchronousDataHeader));
  sco_packet->mutable_view()->mutable_data().Write(packet);

  if (sco_packet->view().header().data_total_length != sco_packet->view().payload_size()) {
    return ZX_ERR_IO_INVALID;
  }

  sco_packet->InitializeFromBuffer();

  send_sco_packet_(std::move(sco_packet));
  SendSnoopChannelPacket(packet, BT_HCI_SNOOP_TYPE_SCO, /*is_received=*/true);

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
    zx_status_t status =
        snoop_channel_.write(0, snoop_buffer, packet.size() + 1, /*handles=*/nullptr, 0);
    if (status != ZX_OK) {
      bt_log(WARN, "fake-hci", "cleaning up snoop channel after failed write: %s",
             zx_status_get_string(status));
      CloseSnoopChannel();
    }
  }
}

void ControllerTestDoubleBase::CloseSnoopChannel() {
  if (snoop_channel_.is_valid()) {
    snoop_channel_.reset();
  }
}

void ControllerTestDoubleBase::HandleCommandPacket(std::unique_ptr<hci::CommandPacket> packet) {
  // Post the packet to simulate the async channel operations in production.
  async::PostTask(async_get_default_dispatcher(), [this, packet = std::move(packet)]() {
    SendSnoopChannelPacket(packet->view().data(), BT_HCI_SNOOP_TYPE_CMD, /*is_received=*/false);
    OnCommandPacketReceived(packet->view());
  });
}

void ControllerTestDoubleBase::HandleACLPacket(std::unique_ptr<hci::ACLDataPacket> packet) {
  async::PostTask(async_get_default_dispatcher(), [this, packet = std::move(packet)]() {
    SendSnoopChannelPacket(packet->view().data(), BT_HCI_SNOOP_TYPE_ACL, /*is_received=*/false);
    OnACLDataPacketReceived(packet->view().data());
  });
}

void ControllerTestDoubleBase::HandleScoPacket(std::unique_ptr<hci::ScoDataPacket> packet) {
  async::PostTask(async_get_default_dispatcher(), [this, packet = std::move(packet)]() {
    SendSnoopChannelPacket(packet->view().data(), BT_HCI_SNOOP_TYPE_SCO, /*is_received=*/false);
    OnScoDataPacketReceived(packet->view().data());
  });
}

}  // namespace bt::testing
