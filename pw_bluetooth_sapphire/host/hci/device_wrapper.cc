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

namespace bt {
namespace hci {

// ================= FidlDeviceWrapper =================

FidlDeviceWrapper::FidlDeviceWrapper(zx::channel device) : device_(std::move(device)) {
  ZX_DEBUG_ASSERT(device_.is_valid());
}

zx::channel FidlDeviceWrapper::GetCommandChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create command channel: %s", zx_status_get_string(status));
  }

  zx_handle_t dev_handle = device_.release();
  status = fuchsia_hardware_bluetooth_HciOpenCommandChannel(dev_handle, theirs.release());
  *device_.reset_and_get_address() = dev_handle;

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind command channel: %s", zx_status_get_string(status));
  }
  return ours;
}

zx::channel FidlDeviceWrapper::GetACLDataChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create ACL channel: %s", zx_status_get_string(status));
  }

  zx_handle_t dev_handle = device_.release();
  status = fuchsia_hardware_bluetooth_HciOpenAclDataChannel(dev_handle, theirs.release());
  *device_.reset_and_get_address() = dev_handle;

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind ACL channel: %s", zx_status_get_string(status));
  }
  return ours;
}

// ================= DdkDeviceWrapper =================

DdkDeviceWrapper::DdkDeviceWrapper(const bt_hci_protocol_t& hci,
                                   std::optional<bt_vendor_protocol_t> vendor)
    : hci_proto_(hci), vendor_proto_(vendor) {}

zx::channel DdkDeviceWrapper::GetCommandChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create command channel: %s", zx_status_get_string(status));
  }

  status = bt_hci_open_command_channel(&hci_proto_, theirs.release());

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind command channel: %s", zx_status_get_string(status));
  }
  return ours;
}

zx::channel DdkDeviceWrapper::GetACLDataChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create ACL channel: %s", zx_status_get_string(status));
  }

  status = bt_hci_open_acl_data_channel(&hci_proto_, theirs.release());

  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to bind ACL channel: %s", zx_status_get_string(status));
  }
  return ours;
}

bt_vendor_features_t DdkDeviceWrapper::GetVendorFeatures() {
  if (!vendor_proto_) {
    return 0;
  }
  return bt_vendor_get_features(&vendor_proto_.value());
};

fit::result<DynamicByteBuffer> DdkDeviceWrapper::EncodeVendorCommand(bt_vendor_command_t command,
                                                                     bt_vendor_params_t& params) {
  if (!vendor_proto_) {
    return fit::error();
  }

  auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[BT_VENDOR_MAX_COMMAND_BUFFER_LEN]);
  size_t actual = 0;
  auto status = bt_vendor_encode_command(&vendor_proto_.value(), command, &params, buffer.get(),
                                         BT_VENDOR_MAX_COMMAND_BUFFER_LEN, &actual);
  if (status != ZX_OK || !actual || actual > BT_VENDOR_MAX_COMMAND_BUFFER_LEN) {
    bt_log(DEBUG, "hci", "Failed to encode vendor command: %s", zx_status_get_string(status));
    return fit::error();
  }

  return fit::ok(DynamicByteBuffer(actual, std::move(buffer)));
};

// ================= DummyDeviceWrapper =================

DummyDeviceWrapper::DummyDeviceWrapper(zx::channel cmd_channel, zx::channel acl_data_channel,
                                       bt_vendor_features_t vendor_features,
                                       EncodeCallback vendor_encode_cb)
    : cmd_channel_(std::move(cmd_channel)),
      acl_data_channel_(std::move(acl_data_channel)),
      vendor_features_(vendor_features),
      vendor_encode_cb_(std::move(vendor_encode_cb)) {}

zx::channel DummyDeviceWrapper::GetCommandChannel() { return std::move(cmd_channel_); }

zx::channel DummyDeviceWrapper::GetACLDataChannel() { return std::move(acl_data_channel_); }

fit::result<DynamicByteBuffer> DummyDeviceWrapper::EncodeVendorCommand(bt_vendor_command_t command,
                                                                       bt_vendor_params_t& params) {
  if (vendor_encode_cb_) {
    return vendor_encode_cb_(command, params);
  }
  return fit::error();
}

}  // namespace hci
}  // namespace bt
