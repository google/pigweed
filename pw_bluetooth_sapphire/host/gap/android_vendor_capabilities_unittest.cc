// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/android_vendor_capabilities.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"

namespace bt::gap {
namespace {

class AndroidVendorCapabilitiesTest : public ::testing::Test {
 public:
  void SetUp() override {
    hci_android::LEGetVendorCapabilitiesReturnParams params;
    std::memset(&params, 0, sizeof(params));

    params.status = hci_spec::StatusCode::kSuccess;

    // select values other than the zero value to ensure the results of std::memset don't propagate
    params.max_advt_instances = 1;
    params.offloaded_rpa = hci_spec::GenericEnableParam::kEnable;
    params.total_scan_results_storage = 2;
    params.max_irk_list_size = 3;
    params.filtering_support = hci_spec::GenericEnableParam::kEnable;
    params.max_filter = 4;
    params.activity_energy_info_support = hci_spec::GenericEnableParam::kEnable;
    params.version_supported_minor = 5;
    params.version_supported_major = 6;
    params.total_num_of_advt_tracked = 7;
    params.extended_scan_support = hci_spec::GenericEnableParam::kEnable;
    params.debug_logging_supported = hci_spec::GenericEnableParam::kEnable;
    params.le_address_generation_offloading_support = hci_spec::GenericEnableParam::kEnable;
    params.a2dp_source_offload_capability_mask = 8;
    params.bluetooth_quality_report_support = hci_spec::GenericEnableParam::kEnable;
    params.dynamic_audio_buffer_support = 9;

    vendor_capabilities_.Initialize(params);
  }

 protected:
  AndroidVendorCapabilities& vendor_capabilities() { return vendor_capabilities_; }

 private:
  AndroidVendorCapabilities vendor_capabilities_;
};

TEST_F(AndroidVendorCapabilitiesTest, CorrectExtraction) {
  EXPECT_TRUE(vendor_capabilities().IsInitialized());

  EXPECT_EQ(1u, vendor_capabilities().max_simultaneous_advertisements());
  EXPECT_EQ(true, vendor_capabilities().supports_offloaded_rpa());
  EXPECT_EQ(2u, vendor_capabilities().scan_results_storage_bytes());
  EXPECT_EQ(3u, vendor_capabilities().irk_list_size());
  EXPECT_EQ(true, vendor_capabilities().supports_filtering());
  EXPECT_EQ(4u, vendor_capabilities().max_filters());
  EXPECT_EQ(true, vendor_capabilities().supports_activity_energy_info());
  EXPECT_EQ(5u, vendor_capabilities().version_minor());
  EXPECT_EQ(6u, vendor_capabilities().version_major());
  EXPECT_EQ(7u, vendor_capabilities().max_advertisers_tracked());
  EXPECT_EQ(true, vendor_capabilities().supports_extended_scan());
  EXPECT_EQ(true, vendor_capabilities().supports_debug_logging());
  EXPECT_EQ(true, vendor_capabilities().supports_offloading_le_address_generation());
  EXPECT_EQ(8u, vendor_capabilities().a2dp_source_offload_capability_mask());
  EXPECT_EQ(true, vendor_capabilities().supports_bluetooth_quality_report());
  EXPECT_EQ(9u, vendor_capabilities().supports_dynamic_audio_buffer());
}

TEST_F(AndroidVendorCapabilitiesTest, InitializeFailure) {
  EXPECT_TRUE(vendor_capabilities().IsInitialized());

  hci_android::LEGetVendorCapabilitiesReturnParams params;
  std::memset(&params, 0, sizeof(params));
  params.status = hci_spec::StatusCode::kUnknownCommand;
  vendor_capabilities().Initialize(params);

  EXPECT_FALSE(vendor_capabilities().IsInitialized());
}

}  // namespace
}  // namespace bt::gap
