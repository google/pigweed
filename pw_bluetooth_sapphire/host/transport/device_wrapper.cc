// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_wrapper.h"

#include <fuchsia/hardware/bluetooth/c/fidl.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt::hci {

// ================= FidlDeviceWrapper =================

FidlDeviceWrapper::FidlDeviceWrapper(zx::channel device) : device_(std::move(device)) {
  BT_DEBUG_ASSERT(device_.is_valid());
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

fitx::result<zx_status_t, zx::channel> DdkDeviceWrapper::GetScoChannel() {
  zx::channel ours, theirs;
  zx_status_t status = zx::channel::create(0, &ours, &theirs);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "Failed to create SCO channel: %s", zx_status_get_string(status));
  }

  status = bt_hci_open_sco_channel(&hci_proto_, theirs.release());

  if (status != ZX_OK) {
    bt_log(WARN, "hci", "Failed to bind SCO channel: %s", zx_status_get_string(status));
    return fitx::error(status);
  }
  return fitx::ok(std::move(ours));
}

void DdkDeviceWrapper::ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                                    sco_sample_rate_t sample_rate,
                                    bt_hci_configure_sco_callback callback, void* cookie) {
  bt_hci_configure_sco(&hci_proto_, coding_format, encoding, sample_rate, callback, cookie);
}

void DdkDeviceWrapper::ResetSco(bt_hci_reset_sco_callback callback, void* cookie) {
  bt_hci_reset_sco(&hci_proto_, callback, cookie);
}

bt_vendor_features_t DdkDeviceWrapper::GetVendorFeatures() {
  if (!vendor_proto_) {
    return 0;
  }
  return bt_vendor_get_features(&vendor_proto_.value());
}

std::optional<DynamicByteBuffer> DdkDeviceWrapper::EncodeVendorCommand(bt_vendor_command_t command,
                                                                       bt_vendor_params_t& params) {
  if (!vendor_proto_) {
    return std::nullopt;
  }

  auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[BT_VENDOR_MAX_COMMAND_BUFFER_LEN]);
  size_t actual = 0;
  auto status = bt_vendor_encode_command(&vendor_proto_.value(), command, &params, buffer.get(),
                                         BT_VENDOR_MAX_COMMAND_BUFFER_LEN, &actual);
  if (status != ZX_OK || !actual || actual > BT_VENDOR_MAX_COMMAND_BUFFER_LEN) {
    bt_log(DEBUG, "hci", "Failed to encode vendor command: %s", zx_status_get_string(status));
    return std::nullopt;
  }

  return DynamicByteBuffer(actual, std::move(buffer));
}

// ================= DummyDeviceWrapper =================

DummyDeviceWrapper::DummyDeviceWrapper(zx::channel cmd_channel, zx::channel acl_data_channel,
                                       bt_vendor_features_t vendor_features,
                                       EncodeCallback vendor_encode_cb)
    : cmd_channel_(std::move(cmd_channel)),
      acl_data_channel_(std::move(acl_data_channel)),
      vendor_features_(vendor_features),
      vendor_encode_cb_(std::move(vendor_encode_cb)) {}

fitx::result<zx_status_t, zx::channel> DummyDeviceWrapper::GetScoChannel() {
  if (!sco_channel_.is_valid()) {
    return fitx::error(ZX_ERR_NOT_SUPPORTED);
  }
  return fitx::success(std::move(sco_channel_));
}

void DummyDeviceWrapper::ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                                      sco_sample_rate_t sample_rate,
                                      bt_hci_configure_sco_callback callback, void* cookie) {
  if (!configure_sco_cb_) {
    callback(cookie, ZX_ERR_NOT_SUPPORTED);
    return;
  }
  configure_sco_cb_(coding_format, encoding, sample_rate, callback, cookie);
}
void DummyDeviceWrapper::ResetSco(bt_hci_reset_sco_callback callback, void* cookie) {
  if (!reset_sco_cb_) {
    callback(cookie, ZX_ERR_NOT_SUPPORTED);
    return;
  }
  reset_sco_cb_(callback, cookie);
}

std::optional<DynamicByteBuffer> DummyDeviceWrapper::EncodeVendorCommand(
    bt_vendor_command_t command, bt_vendor_params_t& params) {
  if (vendor_encode_cb_) {
    return vendor_encode_cb_(command, params);
  }
  return std::nullopt;
}

}  // namespace bt::hci
