// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/gap/android_vendor_capabilities.h"

#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"

namespace bt::gap {

void AndroidVendorCapabilities::Initialize(
    const pw::bluetooth::vendor::android_hci::
        LEGetVendorCapabilitiesCommandCompleteEventView& c) {
  initialized_ = false;

  if (c.status().Read() != pw::bluetooth::emboss::StatusCode::SUCCESS) {
    bt_log(INFO,
           "android_vendor_extensions",
           "refusing to parse non-success vendor capabilities");
    return;
  }

  max_simultaneous_advertisement_ = c.max_advt_instances().Read();
  supports_offloaded_rpa_ =
      static_cast<bool>(c.offloaded_resolution_of_private_address().Read());
  scan_results_storage_bytes_ = c.total_scan_results_storage().Read();
  irk_list_size_ = c.max_irk_list_sz().Read();
  supports_filtering_ = static_cast<bool>(c.filtering_support().Read());
  max_filters_ = c.max_filter().Read();
  supports_activity_energy_info_ =
      static_cast<bool>(c.activity_energy_info_support().Read());
  version_minor_ = c.version_supported().minor_number().Read();
  version_major_ = c.version_supported().major_number().Read();
  max_advertisers_tracked_ = c.total_num_of_advt_tracked().Read();
  supports_extended_scan_ = static_cast<bool>(c.extended_scan_support().Read());
  supports_debug_logging_ =
      static_cast<bool>(c.debug_logging_supported().Read());
  supports_offloading_le_address_generation_ =
      static_cast<bool>(c.le_address_generation_offloading_support().Read());
  a2dp_source_offload_capability_mask_ =
      c.a2dp_source_offload_capability_mask().BackingStorage().ReadUInt();
  supports_bluetooth_quality_report_ =
      static_cast<bool>(c.bluetooth_quality_report_support().Read());
  supports_dynamic_audio_buffer_ =
      c.dynamic_audio_buffer_support().BackingStorage().ReadUInt();

  initialized_ = true;
}

}  // namespace bt::gap
