// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_

// This file contains general opcode/number and static packet definitions for extensions to the
// Bluetooth Host-Controller interface. These extensions aren't standardized through the Bluetooth
// SIG and their documentation is available separately (linked below). Each packet payload structure
// contains parameter descriptions based on their respective documentation.
//
// Documentation links:
//
//    - Android: https://source.android.com/devices/bluetooth/hci_requirements
//
// NOTE: The definitions below are incomplete. They get added as needed. This list will grow as we
// support more vendor features.
//
// NOTE: Avoid casting raw buffer pointers to the packet payload structure types below; use as
// template parameter to PacketView::payload(), MutableBufferView::mutable_payload(), or
// CommandPacket::mutable_payload() instead. Take extra care when accessing flexible array members.

#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"

namespace bt::hci_spec::vendor::android {

// Bitmask values for A2DP supported codecs
// clang-format off
enum class A2dpCodecType : uint32_t {
  kSbc    = (1 << 0),
  kAac    = (1 << 1),
  kAptx   = (1 << 2),
  kAptxhd = (1 << 3),
  kLdac   = (1 << 4),
  // Bits 5 - 31 are reserved
};
// clang-format on

// ============================================================================
// LE Get Vendor Capabilities Command
constexpr OpCode kLEGetVendorCapabilities = VendorOpCode(0x153);

struct LEGetVendorCapabilitiesReturnParams {
  StatusCode status;

  // Number of advertisement instances supported.
  //
  // This parameter is deprecated in the Google feature spec v0.98 and higher in favor of the LE
  // Extended Advertising available in the BT spec version 5.0 and higher.
  uint8_t max_advt_instances;

  // BT chip capability of resolution of private addresses. If supported by a chip, it needs
  // enablement by the host.
  //
  // This parameter is deprecated in the Google feature spec v0.98 and higher in favor of the
  // Privacy feature available in the BT spec version 4.2 and higher.
  GenericEnableParam offloaded_rpa;

  // Storage for scan results in bytes
  uint16_t total_scan_results_storage;

  // Number of IRK entries supported in the firmware
  uint8_t max_irk_list_size;

  // Support for filtering in the controller
  GenericEnableParam filtering_support;

  // Number of filters supported
  uint8_t max_filter;

  // Supports reporting of activity and energy information
  GenericEnableParam activity_energy_info_support;

  // Specifies the minor version of the Google feature spec supported
  uint8_t version_supported_minor;

  // Specifies the major version of the Google feature spec supported
  uint8_t version_supported_major;

  // Total number of advertisers tracked for OnLost/OnFound purposes
  uint16_t total_num_of_advt_tracked;

  // Supports extended scan window and interval
  GenericEnableParam extended_scan_support;

  // Supports logging of binary debug information from controller
  GenericEnableParam debug_logging_supported;

  // This parameter is deprecated in the Google feature spec v0.98 and higher in favor of the
  // Privacy feature available in the BT spec version 4.2 and higher.
  GenericEnableParam le_address_generation_offloading_support;

  // Bit masks for codec types supported (see A2dpCodecType)
  uint32_t a2dp_source_offload_capability_mask;

  // Supports reporting of Bluetooth Quality events
  GenericEnableParam bluetooth_quality_report_support;

  // Supports dynamic audio buffer in the Bluetooth controller. See A2dpCodecType for bitmask.
  uint32_t dynamic_audio_buffer_support;
} __PACKED;

}  // namespace bt::hci_spec::vendor::android

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_
