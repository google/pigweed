// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_wrapper.h"

#include <gtest/gtest.h>

namespace bt::hci {
namespace {

TEST(DdkDeviceWrapperTest, NullVendorProto) {
  bt_hci_protocol_t hci_proto = {};
  DdkDeviceWrapper wrapper(hci_proto, std::nullopt);
  EXPECT_EQ(wrapper.GetVendorFeatures(), 0u);
  bt_vendor_params_t params = {};
  EXPECT_TRUE(wrapper.EncodeVendorCommand(0, params).is_error());
}

constexpr bt_vendor_features_t kVendorFeatures = 1;
bt_vendor_features_t get_vendor_features(void* ctx) { return kVendorFeatures; }

TEST(DdkDeviceWrapperTest, GetVendorFeatures) {
  bt_hci_protocol_t hci_proto = {};
  bt_vendor_protocol_ops_t vendor_ops{.get_features = get_vendor_features};
  bt_vendor_protocol_t vendor_proto = {.ops = &vendor_ops};
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  EXPECT_EQ(wrapper.GetVendorFeatures(), kVendorFeatures);
}

zx_status_t encode_command_error(void* ctx, bt_vendor_command_t command,
                                 const bt_vendor_params_t* params, uint8_t* out_encoded_buffer,
                                 size_t encoded_size, size_t* out_encoded_actual) {
  return ZX_ERR_BUFFER_TOO_SMALL;
}

TEST(DdkDeviceWrapperTest, EncodeCommandError) {
  bt_hci_protocol_t hci_proto = {};
  bt_vendor_protocol_ops_t vendor_ops{.encode_command = encode_command_error};
  bt_vendor_protocol_t vendor_proto = {.ops = &vendor_ops};
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  bt_vendor_params_t params = {};
  EXPECT_TRUE(wrapper.EncodeVendorCommand(0, params).is_error());
}

zx_status_t encode_command_actual_size_0(void* ctx, bt_vendor_command_t command,
                                         const bt_vendor_params_t* params,
                                         uint8_t* out_encoded_buffer, size_t encoded_size,
                                         size_t* out_encoded_actual) {
  *out_encoded_actual = 0;
  return ZX_OK;
}

TEST(DdkDeviceWrapperTest, EncodeCommandActualSizeZero) {
  bt_hci_protocol_t hci_proto = {};
  bt_vendor_protocol_ops_t vendor_ops{.encode_command = encode_command_actual_size_0};
  bt_vendor_protocol_t vendor_proto = {.ops = &vendor_ops};
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  bt_vendor_params_t params = {};
  EXPECT_TRUE(wrapper.EncodeVendorCommand(0, params).is_error());
}

zx_status_t encode_command_actual_size_too_large(void* ctx, bt_vendor_command_t command,
                                                 const bt_vendor_params_t* params,
                                                 uint8_t* out_encoded_buffer, size_t encoded_size,
                                                 size_t* out_encoded_actual) {
  *out_encoded_actual = BT_VENDOR_MAX_COMMAND_BUFFER_LEN + 1;
  return ZX_OK;
}

TEST(DdkDeviceWrapperTest, EncodeCommandActualSizeTooLarge) {
  bt_hci_protocol_t hci_proto = {};
  bt_vendor_protocol_ops_t vendor_ops{.encode_command = encode_command_actual_size_too_large};
  bt_vendor_protocol_t vendor_proto = {.ops = &vendor_ops};
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  bt_vendor_params_t params = {};
  EXPECT_TRUE(wrapper.EncodeVendorCommand(0, params).is_error());
}

zx_status_t encode_command_success(void* ctx, bt_vendor_command_t command,
                                   const bt_vendor_params_t* params, uint8_t* out_encoded_buffer,
                                   size_t encoded_size, size_t* out_encoded_actual) {
  *out_encoded_actual = 1;
  static_cast<uint8_t*>(out_encoded_buffer)[0] = 1;
  return ZX_OK;
}

TEST(DdkDeviceWrapperTest, EncodeCommandSuccess) {
  bt_hci_protocol_t hci_proto = {};
  bt_vendor_protocol_ops_t vendor_ops{.encode_command = encode_command_success};
  bt_vendor_protocol_t vendor_proto = {.ops = &vendor_ops};
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  bt_vendor_params_t params = {};
  auto result = wrapper.EncodeVendorCommand(0, params);
  ASSERT_TRUE(result.is_ok());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(result.value()[0], 0x01);
}

TEST(DdkDeviceWrapperTest, GetScoChannelSuccess) {
  bt_hci_protocol_ops_t hci_ops{.open_sco_channel = [](void* ctx, zx_handle_t channel) {
    EXPECT_NE(channel, ZX_HANDLE_INVALID);
    zx_handle_close(channel);
    return ZX_OK;
  }};
  bt_hci_protocol_t hci_proto = {.ops = &hci_ops};
  bt_vendor_protocol_t vendor_proto;
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  auto result = wrapper.GetScoChannel();
  ASSERT_TRUE(result.is_ok());
  EXPECT_TRUE(result.value().is_valid());
}

TEST(DdkDeviceWrapperTest, GetScoChannelFailure) {
  bt_hci_protocol_ops_t hci_ops{.open_sco_channel = [](void* ctx, zx_handle_t channel) {
    zx_handle_close(channel);
    return ZX_ERR_NOT_SUPPORTED;
  }};
  bt_hci_protocol_t hci_proto = {.ops = &hci_ops};
  bt_vendor_protocol_t vendor_proto;
  DdkDeviceWrapper wrapper(hci_proto, vendor_proto);
  auto result = wrapper.GetScoChannel();
  ASSERT_TRUE(result.is_error());
  EXPECT_EQ(result.error_value(), ZX_ERR_NOT_SUPPORTED);
}

}  // namespace
}  // namespace bt::hci
