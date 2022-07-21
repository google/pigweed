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
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

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

// Bitmask values for Sampling Frequency
enum class A2dpSamplingFrequency : uint32_t {
  // clang-format off
  k44100Hz  = (1 << 0),
  k48000Hz  = (1 << 1),
  k88200Hz  = (1 << 2),
  k96000Hz  = (1 << 3),
  // clang-format on
};

// Bitmask values for Bits per Sample
enum class A2dpBitsPerSample : uint8_t {
  // clang-format off
  k16BitsPerSample  = (1 << 0),
  k24BitsPerSample  = (1 << 1),
  k32BitsPerSample  = (1 << 2),
  // clang-format on
};

// Bitmask values for Channel Mode
enum class A2dpChannelMode : uint8_t {
  // clang-format off
  kMono   = (1 << 0),
  kStereo = (1 << 1),
  // clang-format on
};

// Bitmask values for Channel Mode
enum class A2dpBitrateIndex : uint8_t {
  // clang-format off
  kHigh             = 0x00,
  kMild             = 0x01,
  kLow              = 0x02,
  // Values 0x03 - 0x7E are reserved
  kAdaptiveBitrate  = 0x7F,
  // Values 0x80 - 0xFF are reserved
  // clang-format on
};

// Bitmask values for LDAC Channel Mode
enum class A2dpLdacChannelMode : uint8_t {
  // clang-format off
  kStereo = (1 << 0),
  kDual   = (1 << 1),
  kMono   = (1 << 2),
  // clang-format on
};

// 1-octet boolean "enable"/"disable" parameter for AAC variable bitrate
enum class A2dpAacEnableVariableBitRate : uint8_t {
  kDisable = 0x00,
  kEnable = 0x80,
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
// A2DP Offload Commands

constexpr OpCode kA2dpOffloadCommand = VendorOpCode(0x15D);
constexpr uint8_t kStartA2dpOffloadCommandSubopcode = 0x01;
constexpr uint8_t kStopA2dpOffloadCommandSubopcode = 0x02;
constexpr uint32_t kLdacVendorId = 0x0000012D;
constexpr uint16_t kLdacCodeId = 0x00AA;

struct A2dpScmsTEnable {
  GenericEnableParam enabled;
  uint8_t header;
} __PACKED;

struct SbcCodecInformation {
  // Bitmask: block length | subbands | allocation method
  // Block length: bits 7-4
  // Subbands: bits 3-2
  // Allocation method: bits 1-0
  uint8_t blocklen_subbands_alloc_method;

  uint8_t min_bitpool_value;

  uint8_t max_bitpool_value;

  // Bitmask: sampling frequency | channel mode
  // Sampling frequency: bits 7-4
  // Channel mode: bits 3-0
  uint8_t sampling_freq_channel_mode;

  // Bytes 4 - 31 are reserved
  uint8_t reserved[28];
} __PACKED;

static_assert(sizeof(SbcCodecInformation) == 32,
              "SbcCodecInformation must take up exactly 32 bytes");

struct AacCodecInformation {
  // Object type
  uint8_t object_type;

  A2dpAacEnableVariableBitRate variable_bit_rate;

  // Bytes 2 - 31 are reserved
  uint8_t reserved[30];
} __PACKED;

static_assert(sizeof(AacCodecInformation) == 32,
              "AacCodecInformation must take up exactly 32 bytes");

struct LdacCodecInformation {
  // Must always be set to kLdacVendorId
  uint32_t vendor_id;

  // Must always be set to kLdacCodeId
  // All other values are reserved
  uint16_t codec_id;

  // Bitmask: bitrate index (see BitrateIndex for bitmask values)
  A2dpBitrateIndex bitrate_index;

  // Bitmask: LDAC channel mode (see LdacChannelMode for bitmask values)
  A2dpLdacChannelMode ldac_channel_mode;

  // Bytes 8 - 31 are reserved
  uint8_t reserved[24];
} __PACKED;

static_assert(sizeof(LdacCodecInformation) == 32,
              "LdacCodecInformation must take up exactly 32 bytes");

struct AptxCodecInformation {
  // Bits 0 - 31 are reserved
  uint8_t reserved[32];
} __PACKED;

static_assert(sizeof(AptxCodecInformation) == 32,
              "AptxCodecInformation must take up exactly 32 bytes");

union A2dpOffloadCodecInformation {
  SbcCodecInformation sbc;
  AacCodecInformation aac;
  LdacCodecInformation ldac;
  AptxCodecInformation aptx;
} __PACKED;

static_assert(sizeof(A2dpOffloadCodecInformation) == 32,
              "A2dpOffloadCodecInformation must take up exactly 32 bytes");

struct StartA2dpOffloadCommand {
  // Must always be set to kStartA2dpOffloadCommandSubopcode
  uint8_t opcode;

  // Bitmask: codec types supported (see A2dpCodecType for bitmask values)
  A2dpCodecType codec;

  // Max latency allowed in ms. A value of zero disables flush.
  uint16_t max_latency;

  A2dpScmsTEnable scms_t_enable;

  // Bitmask: sampling frequency (see SamplingFrequency for bitmask values)
  A2dpSamplingFrequency sampling_frequency;

  // Bitmask: bits per sample (see BitsPerSample for bitmask values)
  A2dpBitsPerSample bits_per_sample;

  // Bitmask: channel mode (see ChannelMode for bitmask values)
  A2dpChannelMode channel_mode;

  // The encoded audio bitrate in bits per second
  // 0x00000000 - The audio bitrate is not specified / unused
  // 0x00000001 - 0x00FFFFFF - Encoded audio bitrate in bits per second
  // 0x01000000 - 0xFFFFFFFF - Reserved
  uint32_t encoded_audio_bitrate;

  // Connection handle of A2DP connection being configured
  ConnectionHandle connection_handle;

  // L2CAP channel ID to be used for this A2DP connection
  l2cap::ChannelId l2cap_channel_id;

  // Maximum size of L2CAP MTY containing encoded audio packets
  uint16_t l2cap_mtu_size;

  // Codec-specific information
  A2dpOffloadCodecInformation codec_information;
} __PACKED;

struct StartA2dpOffloadCommandReturnParams {
  StatusCode status;

  // Will always be set to kStartA2dpOffloadCommandSubopcode
  uint8_t opcode;
} __PACKED;

struct StopA2dpOffloadCommandReturnParams {
  StatusCode status;

  // Will always be set to kStopA2dpOffloadCommandSubopcode
  uint8_t opcode;
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
