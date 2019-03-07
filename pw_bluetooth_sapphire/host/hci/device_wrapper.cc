// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_wrapper.h"

#include <fuchsia/hardware/bluetooth/c/fidl.h>
#include <zircon/assert.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace btlib {
namespace hci {

// ================= FidlDeviceWrapper =================

FidlDeviceWrapper::FidlDeviceWrapper(zx::channel device)
    : device_(std::move(device)) {
  ZX_DEBUG_ASSERT(device_.is_valid());
}

zx::channel FidlDeviceWrapper::GetCommandChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create command channel: %s",
           zx_status_get_string(status));
  }

  zx_handle_t dev_handle = device_.release();
  status = fuchsia_hardware_bluetooth_HciOpenCommandChannel(dev_handle, theirs.release());
  *device_.reset_and_get_address() = dev_handle;

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind command channel: %s",
           zx_status_get_string(status));
  }
  return ours;
}

zx::channel FidlDeviceWrapper::GetACLDataChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create ACL channel: %s",
           zx_status_get_string(status));
  }

  zx_handle_t dev_handle = device_.release();
  status = fuchsia_hardware_bluetooth_HciOpenAclDataChannel(dev_handle, theirs.release());
  *device_.reset_and_get_address() = dev_handle;

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind ACL channel: %s",
           zx_status_get_string(status));
  }
  return ours;
}

// ================= DdkDeviceWrapper =================

DdkDeviceWrapper::DdkDeviceWrapper(const bt_hci_protocol_t& hci)
    : hci_proto_(hci) {}

zx::channel DdkDeviceWrapper::GetCommandChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create command channel: %s",
           zx_status_get_string(status));
  }

  status = bt_hci_open_command_channel(&hci_proto_, theirs.release());

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind command channel: %s",
           zx_status_get_string(status));
  }
  return ours;
}

zx::channel DdkDeviceWrapper::GetACLDataChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create ACL channel: %s",
           zx_status_get_string(status));
  }

  status = bt_hci_open_acl_data_channel(&hci_proto_, theirs.release());

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind ACL channel: %s",
           zx_status_get_string(status));
  }
  return ours;
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
