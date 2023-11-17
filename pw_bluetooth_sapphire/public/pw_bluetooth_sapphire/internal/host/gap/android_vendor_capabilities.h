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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/hci-spec/vendor_protocol.h"

namespace bt::gap {

namespace hci_android = hci_spec::vendor::android;

class AndroidVendorCapabilities final {
 public:
  void Initialize(const pw::bluetooth::vendor::android_hci::
                      LEGetVendorCapabilitiesCommandCompleteEventView& c);
  bool IsInitialized() const { return initialized_; }

  // Number of advertisement instances supported.
  //
  // This parameter is deprecated in the Google feature spec v0.98 and higher in
  // favor of the LE Extended Advertising available in the BT spec version 5.0
  // and higher.
  uint8_t max_simultaneous_advertisements() const {
    return max_simultaneous_advertisement_;
  }

  // BT chip capability of resolution of private addresses. If supported by a
  // chip, it needs enablement by the host.
  //
  // This parameter is deprecated in the Google feature spec v0.98 and higher in
  // favor of the Privacy feature available in the BT spec version 4.2 and
  // higher.
  bool supports_offloaded_rpa() const { return supports_offloaded_rpa_; }

  // Storage for scan results in bytes
  uint16_t scan_results_storage_bytes() const {
    return scan_results_storage_bytes_;
  }

  // Number of IRK entries supported in the firmware
  uint8_t irk_list_size() const { return irk_list_size_; }

  // Support for filtering in the controller
  bool supports_filtering() const { return supports_filtering_; }

  // Number of filters supported
  uint8_t max_filters() const { return max_filters_; }

  // Supports reporting of activity and energy information
  bool supports_activity_energy_info() const {
    return supports_activity_energy_info_;
  }

  // Specifies the minor version of the Google feature spec supported
  uint8_t version_minor() const { return version_minor_; }

  // Specifies the major version of the Google feature spec supported
  uint8_t version_major() const { return version_major_; }

  // Total number of advertisers tracked for OnLost/OnFound purposes
  uint16_t max_advertisers_tracked() const { return max_advertisers_tracked_; }

  // Supports extended scan window and interval
  bool supports_extended_scan() const { return supports_extended_scan_; }

  // Supports logging of binary debug information from controller
  bool supports_debug_logging() const { return supports_debug_logging_; }

  // This parameter is deprecated in the Google feature spec v0.98 and higher in
  // favor of the Privacy feature available in the BT spec version 4.2 and
  // higher.
  bool supports_offloading_le_address_generation() const {
    return supports_offloading_le_address_generation_;
  }

  // Get a bitmask of the codec types supported for A2DP source offload. See
  // A2dpCodecType in
  // src/connectivity/bluetooth/core/bt-host/hci_spec/vendor_protocol.h.
  uint32_t a2dp_source_offload_capability_mask() const {
    return a2dp_source_offload_capability_mask_;
  }

  // Supports reporting of Bluetooth Quality events
  bool supports_bluetooth_quality_report() const {
    return supports_bluetooth_quality_report_;
  }

  // Get a bitmask of the codec types where dynamic audio buffering in the
  // Bluetooth controller is supported. See A2dpCodecType in
  // src/connectivity/bluetooth/core/bt-host/hci_spec/vendor_protocol.h.
  uint32_t supports_dynamic_audio_buffer() const {
    return supports_dynamic_audio_buffer_;
  }

 private:
  bool initialized_ = false;
  uint8_t max_simultaneous_advertisement_ = 0;
  bool supports_offloaded_rpa_ = false;
  uint16_t scan_results_storage_bytes_ = 0;
  uint8_t irk_list_size_ = 0;
  bool supports_filtering_ = false;
  uint8_t max_filters_ = 0;
  bool supports_activity_energy_info_ = false;
  uint8_t version_minor_ = 0;
  uint8_t version_major_ = 0;
  uint16_t max_advertisers_tracked_ = 0;
  bool supports_extended_scan_ = false;
  bool supports_debug_logging_ = false;
  bool supports_offloading_le_address_generation_ = false;
  uint32_t a2dp_source_offload_capability_mask_ = 0;
  bool supports_bluetooth_quality_report_ = false;
  uint32_t supports_dynamic_audio_buffer_ = 0;
};
}  // namespace bt::gap
