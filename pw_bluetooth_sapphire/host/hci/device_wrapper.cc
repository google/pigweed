// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_wrapper.h"

#include <zircon/assert.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace btlib {
namespace hci {

IoctlDeviceWrapper::IoctlDeviceWrapper(fxl::UniqueFD device_fd)
    : device_fd_(std::move(device_fd)) {
  ZX_DEBUG_ASSERT(device_fd_.is_valid());
}

zx::channel IoctlDeviceWrapper::GetCommandChannel() {
  zx::channel channel;
  ssize_t status = ioctl_bt_hci_get_command_channel(
      device_fd_.get(), channel.reset_and_get_address());
  if (status < 0) {
    bt_log(ERROR, "hci", "Failed to obtain command channel handle: %s",
           zx_status_get_string(status));
    ZX_DEBUG_ASSERT(!channel.is_valid());
  }
  return channel;
}

zx::channel IoctlDeviceWrapper::GetACLDataChannel() {
  zx::channel channel;
  ssize_t status = ioctl_bt_hci_get_acl_data_channel(
      device_fd_.get(), channel.reset_and_get_address());
  if (status < 0) {
    bt_log(ERROR, "hci", "Failed to obtain ACL data channel handle: %s",
           zx_status_get_string(status));
    ZX_DEBUG_ASSERT(!channel.is_valid());
  }
  return channel;
}

// ================= DdkDeviceWrapper =================

DdkDeviceWrapper::DdkDeviceWrapper(const bt_hci_protocol_t& hci)
    : hci_proto_(hci) {}

zx::channel DdkDeviceWrapper::GetCommandChannel() {
  zx::channel channel;
  zx_status_t status =
      bt_hci_open_command_channel(&hci_proto_, channel.reset_and_get_address());
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to obtain command channel handle: %s",
           zx_status_get_string(status));
  }
  return channel;
}

zx::channel DdkDeviceWrapper::GetACLDataChannel() {
  zx::channel channel;
  zx_status_t status = bt_hci_open_acl_data_channel(
      &hci_proto_, channel.reset_and_get_address());
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to obtain ACL data channel handle: %s",
           zx_status_get_string(status));
  }
  return channel;
}

// ================= DummyDeviceWrapper =================

DummyDeviceWrapper::DummyDeviceWrapper(zx::channel cmd_channel,
                                       zx::channel acl_data_channel)
    : cmd_channel_(std::move(cmd_channel)),
      acl_data_channel_(std::move(acl_data_channel)) {}

zx::channel DummyDeviceWrapper::GetCommandChannel() {
  return std::move(cmd_channel_);
}

zx::channel DummyDeviceWrapper::GetACLDataChannel() {
  return std::move(acl_data_channel_);
}

}  // namespace hci
}  // namespace btlib
