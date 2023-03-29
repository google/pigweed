// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/android_vendor_capabilities.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/vendor_protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/emboss_control_packets.h"

namespace bt::gap {
namespace {

class AndroidVendorCapabilitiesTest : public ::testing::Test {
 public:
  void SetUp() override {
    bt::StaticPacket<pw::bluetooth::emboss::LEGetVendorCapabilitiesCommandCompleteEventWriter>
        params;
    params.SetToZeros();
    params.view().status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);

    // select values other than the zero value to ensure the results of std::memset don't propagate
    params.view().max_advt_instances().Write(1);
    params.view().offloaded_resolution_of_private_address().Write(
        pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().total_scan_results_storage().Write(2);
    params.view().max_irk_list_sz().Write(3);
    params.view().filtering_support().Write(pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().max_filter().Write(4);
    params.view().activity_energy_info_support().Write(pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().version_supported().major_number().Write(5);
    params.view().version_supported().minor_number().Write(6);
    params.view().total_num_of_advt_tracked().Write(7);
    params.view().extended_scan_support().Write(pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().debug_logging_supported().Write(pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().le_address_generation_offloading_support().Write(
        pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().a2dp_source_offload_capability_mask().BackingStorage().WriteUInt(8);
    params.view().bluetooth_quality_report_support().Write(
        pw::bluetooth::emboss::Capability::CAPABLE);
    params.view().dynamic_audio_buffer_support().sbc().Write(true);
    params.view().dynamic_audio_buffer_support().aptx_hd().Write(true);

    vendor_capabilities_.Initialize(params.view());
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
  EXPECT_EQ(5u, vendor_capabilities().version_major());
  EXPECT_EQ(6u, vendor_capabilities().version_minor());
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

  bt::StaticPacket<pw::bluetooth::emboss::LEGetVendorCapabilitiesCommandCompleteEventWriter> params;
  params.SetToZeros();
  params.view().status().Write(pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
  vendor_capabilities().Initialize(params.view());

  EXPECT_FALSE(vendor_capabilities().IsInitialized());
}

}  // namespace
}  // namespace bt::gap
