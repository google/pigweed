// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DEVICE_WRAPPER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DEVICE_WRAPPER_H_

#include <fuchsia/hardware/bluetooth/c/fidl.h>
#include <fuchsia/hardware/bt/hci/c/banjo.h>
#include <fuchsia/hardware/bt/vendor/c/banjo.h>
#include <lib/fit/function.h>
#include <lib/fitx/result.h>
#include <lib/fpromise/result.h>
#include <lib/zx/channel.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include <optional>

#include <fbl/macros.h>
#include <fbl/unique_fd.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt::hci {

// A DeviceWrapper abstracts over a Bluetooth HCI device object and its fidl
// interface.
class DeviceWrapper {
 public:
  virtual ~DeviceWrapper() = default;

  // Returns the command/event channel handle for this device. Returns an
  // invalid handle on failure.
  virtual zx::channel GetCommandChannel() = 0;

  // Returns the ACL data channel handle for this device. Returns an invalid
  // handle on failure.
  virtual zx::channel GetACLDataChannel() = 0;

  // Returns the SCO channel handle for this device. Returns an invalid
  // handle on failure.
  virtual fitx::result<zx_status_t, zx::channel> GetScoChannel() = 0;

  virtual void ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                            sco_sample_rate_t sample_rate, bt_hci_configure_sco_callback callback,
                            void* cookie) = 0;

  virtual void ResetSco(bt_hci_reset_sco_callback callback, void* cookie) = 0;

  virtual bt_vendor_features_t GetVendorFeatures() = 0;

  virtual fpromise::result<DynamicByteBuffer> EncodeVendorCommand(bt_vendor_command_t command,
                                                                  bt_vendor_params_t& params) {
    return fpromise::error();
  }
};

// A DeviceWrapper that obtains channels by invoking bt-hci fidl requests on a
// devfs file descriptor.
class FidlDeviceWrapper : public DeviceWrapper {
 public:
  // |device| must be a valid channel connected to a Bluetooth HCI device.
  explicit FidlDeviceWrapper(zx::channel device);
  ~FidlDeviceWrapper() override = default;

  // DeviceWrapper overrides. These methods directly return the handle obtained
  // via the corresponding fidl request.
  zx::channel GetCommandChannel() override;
  zx::channel GetACLDataChannel() override;

  // SCO is currently not supported for FIDL devices.
  fitx::result<zx_status_t, zx::channel> GetScoChannel() override {
    return fitx::error(ZX_ERR_NOT_SUPPORTED);
  }
  void ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                    sco_sample_rate_t sample_rate, bt_hci_configure_sco_callback callback,
                    void* cookie) override {
    callback(cookie, ZX_ERR_NOT_SUPPORTED);
  }
  void ResetSco(bt_hci_reset_sco_callback callback, void* cookie) override {
    callback(cookie, ZX_ERR_NOT_SUPPORTED);
  }

  bt_vendor_features_t GetVendorFeatures() override { return 0; }

 private:
  zx::channel device_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FidlDeviceWrapper);
};

// A DeviceWrapper that calls bt-hci and bt-vendor DDK protocol ops.
class DdkDeviceWrapper : public DeviceWrapper {
 public:
  // The contents of |hci| must remain valid while this object is in use.
  explicit DdkDeviceWrapper(const bt_hci_protocol_t& hci,
                            std::optional<bt_vendor_protocol_t> vendor);
  ~DdkDeviceWrapper() override = default;

  // DeviceWrapper overrides:
  zx::channel GetCommandChannel() override;
  zx::channel GetACLDataChannel() override;
  fitx::result<zx_status_t, zx::channel> GetScoChannel() override;
  void ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                    sco_sample_rate_t sample_rate, bt_hci_configure_sco_callback callback,
                    void* cookie) override;
  void ResetSco(bt_hci_reset_sco_callback callback, void* cookie) override;
  bt_vendor_features_t GetVendorFeatures() override;
  fpromise::result<DynamicByteBuffer> EncodeVendorCommand(bt_vendor_command_t command,
                                                          bt_vendor_params_t& params) override;

 private:
  bt_hci_protocol_t hci_proto_;
  std::optional<bt_vendor_protocol_t> vendor_proto_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(DdkDeviceWrapper);
};

// A pass-through DeviceWrapper that returns the channel endpoints that it is
// initialized with. This is useful for test scenarios.
class DummyDeviceWrapper : public DeviceWrapper {
 public:
  // The constructor takes ownership of the provided channels and simply returns
  // them when asked for them. |vendor_features| will be returned by GetVendorFeatures() and calls
  // to EncodeVendorCommand() are forwarded to |vendor_encode_cb|.
  using EncodeCallback =
      fit::function<fpromise::result<DynamicByteBuffer>(bt_vendor_command_t, bt_vendor_params_t)>;
  DummyDeviceWrapper(zx::channel cmd_channel, zx::channel acl_data_channel,
                     bt_vendor_features_t vendor_features = 0,
                     EncodeCallback vendor_encode_cb = nullptr);
  ~DummyDeviceWrapper() override = default;

  void set_sco_channel(zx::channel chan) { sco_channel_ = std::move(chan); }

  using ConfigureScoCallback =
      fit::function<void(sco_coding_format_t, sco_encoding_t, sco_sample_rate_t,
                         bt_hci_configure_sco_callback, void*)>;
  void set_configure_sco_callback(ConfigureScoCallback cb) { configure_sco_cb_ = std::move(cb); }

  using ResetScoCallback = fit::function<void(bt_hci_reset_sco_callback, void*)>;
  void set_reset_sco_callback(ResetScoCallback cb) { reset_sco_cb_ = std::move(cb); }

  // DeviceWrapper overrides. Since these methods simply forward the handles they were initialized
  // with, the internal handles will be moved and invalidated after the first call to these methods.
  // Subsequent calls will always return an invalid handle.
  zx::channel GetCommandChannel() override { return std::move(cmd_channel_); }
  zx::channel GetACLDataChannel() override { return std::move(acl_data_channel_); }
  fitx::result<zx_status_t, zx::channel> GetScoChannel() override;
  void ConfigureSco(sco_coding_format_t coding_format, sco_encoding_t encoding,
                    sco_sample_rate_t sample_rate, bt_hci_configure_sco_callback callback,
                    void* cookie) override;
  void ResetSco(bt_hci_reset_sco_callback callback, void* cookie) override;

  bt_vendor_features_t GetVendorFeatures() override { return vendor_features_; }
  fpromise::result<DynamicByteBuffer> EncodeVendorCommand(bt_vendor_command_t command,
                                                          bt_vendor_params_t& params) override;

 private:
  zx::channel cmd_channel_;
  zx::channel acl_data_channel_;
  zx::channel sco_channel_;
  bt_vendor_features_t vendor_features_;
  EncodeCallback vendor_encode_cb_;
  ConfigureScoCallback configure_sco_cb_ = nullptr;
  ResetScoCallback reset_sco_cb_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(DummyDeviceWrapper);
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_DEVICE_WRAPPER_H_
