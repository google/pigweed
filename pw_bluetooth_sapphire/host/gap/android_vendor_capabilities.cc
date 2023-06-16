// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/android_vendor_capabilities.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

namespace bt::gap {

void AndroidVendorCapabilities::Initialize(
    const pw::bluetooth::vendor::android_hci::LEGetVendorCapabilitiesCommandCompleteEventView& c) {
  initialized_ = false;

  if (c.status().Read() != pw::bluetooth::emboss::StatusCode::SUCCESS) {
    bt_log(INFO, "android_vendor_extensions", "refusing to parse non-success vendor capabilities");
    return;
  }

  max_simultaneous_advertisement_ = c.max_advt_instances().Read();
  supports_offloaded_rpa_ = static_cast<bool>(c.offloaded_resolution_of_private_address().Read());
  scan_results_storage_bytes_ = c.total_scan_results_storage().Read();
  irk_list_size_ = c.max_irk_list_sz().Read();
  supports_filtering_ = static_cast<bool>(c.filtering_support().Read());
  max_filters_ = c.max_filter().Read();
  supports_activity_energy_info_ = static_cast<bool>(c.activity_energy_info_support().Read());
  version_minor_ = c.version_supported().minor_number().Read();
  version_major_ = c.version_supported().major_number().Read();
  max_advertisers_tracked_ = c.total_num_of_advt_tracked().Read();
  supports_extended_scan_ = static_cast<bool>(c.extended_scan_support().Read());
  supports_debug_logging_ = static_cast<bool>(c.debug_logging_supported().Read());
  supports_offloading_le_address_generation_ =
      static_cast<bool>(c.le_address_generation_offloading_support().Read());
  a2dp_source_offload_capability_mask_ =
      c.a2dp_source_offload_capability_mask().BackingStorage().ReadUInt();
  supports_bluetooth_quality_report_ =
      static_cast<bool>(c.bluetooth_quality_report_support().Read());
  supports_dynamic_audio_buffer_ = c.dynamic_audio_buffer_support().BackingStorage().ReadUInt();

  initialized_ = true;
}

}  // namespace bt::gap
