// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/android_vendor_capabilities.h"

#include <endian.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

namespace bt::gap {

void AndroidVendorCapabilities::Initialize(
    const hci_android::LEGetVendorCapabilitiesReturnParams& c) {
  initialized_ = false;

  if (c.status != hci_spec::StatusCode::SUCCESS) {
    bt_log(INFO, "android_vendor_extensions", "refusing to parse non-success vendor capabilities");
    return;
  }

  max_simultaneous_advertisement_ = c.max_advt_instances;
  supports_offloaded_rpa_ = static_cast<bool>(c.offloaded_rpa);
  scan_results_storage_bytes_ = letoh16(c.total_scan_results_storage);
  irk_list_size_ = c.max_irk_list_size;
  supports_filtering_ = static_cast<bool>(c.filtering_support);
  max_filters_ = c.max_filter;
  supports_activity_energy_info_ = static_cast<bool>(c.activity_energy_info_support);
  version_minor_ = c.version_supported_minor;
  version_major_ = c.version_supported_major;
  max_advertisers_tracked_ = letoh16(c.total_num_of_advt_tracked);
  supports_extended_scan_ = static_cast<bool>(c.extended_scan_support);
  supports_debug_logging_ = static_cast<bool>(c.debug_logging_supported);
  supports_offloading_le_address_generation_ =
      static_cast<bool>(c.le_address_generation_offloading_support);
  a2dp_source_offload_capability_mask_ = letoh32(c.a2dp_source_offload_capability_mask);
  supports_bluetooth_quality_report_ = static_cast<bool>(c.bluetooth_quality_report_support);
  supports_dynamic_audio_buffer_ = letoh32(c.dynamic_audio_buffer_support);

  initialized_ = true;
}

}  // namespace bt::gap
