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
enum class A2dpCodecType : uint32_t {
  // clang-format off
  kSbc    = (1 << 0),
  kAac    = (1 << 1),
  kAptx   = (1 << 2),
  kAptxhd = (1 << 3),
  kLdac   = (1 << 4),
  // Bits 5 - 31 are reserved
  // clang-format on
};

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

  // Bitmask: codec types supported (see A2dpCodecType for bitmask values)
  uint32_t a2dp_source_offload_capability_mask;

  // Supports reporting of Bluetooth Quality events
  GenericEnableParam bluetooth_quality_report_support;

  // Bitmask: codec types supported in dynamic audio buffer within the Bluetooth controller (see
  // A2dpCodecType for bitmask values)
  uint32_t dynamic_audio_buffer_support;
} __PACKED;

// ============================================================================
// Multiple Advertising
//
// NOTE: Multiple advertiser support is deprecated in the Google feature spec v0.98 and above. Users
// of the following vendor extension HCI commands should first ensure that the controller is using a
// compatible Google feature spec.

// The kLEMultiAdvt opcode is shared across all multiple advertising HCI commands. To differentiate
// between the multiple commands, a subopcode field is included in the command payload.
constexpr OpCode kLEMultiAdvt = VendorOpCode(0x154);

// ============================================================================
// LE Multiple Advertising Set Advertising Parameters
constexpr uint8_t kLEMultiAdvtSetAdvtParamSubopcode = 0x01;

struct LEMultiAdvtSetAdvtParamCommandParams {
  // Must always be set to kLEMultiAdvtSetAdvtParametersSubopcode
  uint8_t opcode;

  // Range: see kLEAdvertisingInterval[Min|Max] in hci_constants.h
  // Default: N = kLEAdvertisingIntervalDefault (see hci_constants.h)
  // Time: N * 0.625 ms
  // Time Range: 20 ms to 10.24 s
  uint16_t adv_interval_min;

  // Range: see kLEAdvertisingInterval[Min|Max] in hci_constants.h
  // Default: N = kLEAdvertisingIntervalDefault (see hci_constants.h)
  // Time: N * 0.625 ms
  // Time Range: 20 ms to 10.24 s
  uint16_t adv_interval_max;

  // Used to determine the packet type that is used for advertising when advertising is enabled (see
  // hci_constants.h)
  LEAdvertisingType adv_type;

  LEOwnAddressType own_address_type;
  LEPeerAddressType peer_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or Random (static)
  // Identity Address of the device to be connected.
  DeviceAddressBytes peer_address;

  // (See the constants kLEAdvertisingChannel* in hci_constants.h for possible values).
  uint8_t adv_channel_map;

  // This parameter shall be ignored when directed advertising is enabled (see hci_constants.h for
  // possible values).
  LEAdvFilterPolicy adv_filter_policy;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Transmit_Power, Unit: dBm
  // Range (-70 to +20)
  int8_t adv_tx_power;
} __PACKED;

struct LEMultiAdvtSetAdvtParamReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetAdvtParametersSubopcode
  uint8_t opcode;
} __PACKED;

// =======================================
// LE Multiple Advertising Set Advertising Data
constexpr uint8_t kLEMultiAdvtSetAdvtDataSubopcode = 0x2;

struct LEMultiAdvtSetAdvtDataCommandParams {
  // Must always be set to kLEMultiAdvtSetAdvtDataSubopcode
  uint8_t opcode;

  // Length of the advertising data included in this command packet, up to
  // kMaxLEAdvertisingDataLength bytes.
  uint8_t adv_data_length;

  // 31 octets of advertising data formatted as defined in Core Spec v5.0, Vol 3, Part C, Section 11
  uint8_t adv_data[kMaxLEAdvertisingDataLength];

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

struct LEMultiAdvtSetAdvtDataReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetAdvtDataSubopcode
  uint8_t opcode;
} __PACKED;

// =======================================
// LE Multiple Advertising Set Scan Response
constexpr uint8_t kLEMultiAdvtSetScanRespSubopcode = 0x3;

struct LEMultiAdvtSetScanRespCommandParams {
  // Must always be set to kLEMultiAdvtSetScanRespSubopcode
  uint8_t opcode;

  // Length of the scan response data included in this command packet, up to
  // kMaxLEAdvertisingDataLength bytes.
  uint8_t scan_rsp_data_length;

  // 31 octets of advertising data formatted as defined in Core Spec v5.0, Vol 3, Part C, Section 11
  uint8_t scan_rsp_data[kMaxLEAdvertisingDataLength];

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

struct LEMultiAdvtSetScanRespReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetScanRespSubopcode
  uint8_t opcode;
} __PACKED;

// =======================================
// LE Multiple Advertising Set Random Address
constexpr uint8_t kLEMultiAdvtSetRandomAddrSubopcode = 0x4;

struct LEMultiAdvtSetRandomAddrCommandParams {
  // Must always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;

  DeviceAddressBytes random_address;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

struct LEMultiAdvtSetRandomAddrReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;
} __PACKED;

// =======================================
// LE Multiple Advertising Set Advertising Enable
constexpr uint8_t kLEMultiAdvtEnableSubopcode = 0x5;

struct LEMultiAdvtEnableCommandParams {
  // Must always be set to kLEMultiAdvtEnableSubopcode
  uint8_t opcode;

  GenericEnableParam enable;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

struct LEMultiAdvtEnableReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;
} __PACKED;

// ======= Events =======

// LE multi-advertising state change sub-event
constexpr EventCode kLEMultiAdvtStateChangeSubeventCode = 0x55;

struct LEMultiAdvtStateChangeSubeventParams {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Reason for state change. Currently will always be 0x00.
  // 0x00: Connection received
  StatusCode status;

  // Handle used to identify the connection that caused the state change (i.e. advertising
  // instance to be disabled). Value will be 0xFFFF if invalid.
  ConnectionHandle connection_handle;
} __PACKED;

}  // namespace bt::hci_spec::vendor::android

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_
