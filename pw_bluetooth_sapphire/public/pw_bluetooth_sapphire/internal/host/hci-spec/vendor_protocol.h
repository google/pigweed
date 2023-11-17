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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_

// This file contains general opcode/number and static packet definitions for
// extensions to the Bluetooth Host-Controller interface. These extensions
// aren't standardized through the Bluetooth SIG and their documentation is
// available separately (linked below). Each packet payload structure contains
// parameter descriptions based on their respective documentation.
//
// Documentation links:
//
//    - Android: https://source.android.com/devices/bluetooth/hci_requirements
//
// NOTE: The definitions below are incomplete. They get added as needed. This
// list will grow as we support more vendor features.
//
// NOTE: Avoid casting raw buffer pointers to the packet payload structure types
// below; use as template parameter to PacketView::payload(),
// MutableBufferView::mutable_payload(), or CommandPacket::mutable_payload()
// instead. Take extra care when accessing flexible array members.

#include <pw_bluetooth/hci_vendor.emb.h>

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"

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

// Bitmask values for Bitrate Index
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

// ============================================================================
// A2DP Offload Commands

constexpr OpCode kA2dpOffloadCommand = VendorOpCode(0x15D);
constexpr uint8_t kStartA2dpOffloadCommandSubopcode = 0x01;
constexpr uint8_t kStopA2dpOffloadCommandSubopcode = 0x02;
constexpr uint32_t kLdacVendorId = 0x0000012D;
constexpr uint16_t kLdacCodecId = 0x00AA;

struct A2dpScmsTEnable {
  GenericEnableParam enabled;
  uint8_t header;
} __attribute__((packed));

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
} __attribute__((packed));

static_assert(sizeof(SbcCodecInformation) == 32,
              "SbcCodecInformation must take up exactly 32 bytes");

struct AacCodecInformation {
  // Object type
  uint8_t object_type;

  A2dpAacEnableVariableBitRate variable_bit_rate;

  // Bytes 2 - 31 are reserved
  uint8_t reserved[30];
} __attribute__((packed));

static_assert(sizeof(AacCodecInformation) == 32,
              "AacCodecInformation must take up exactly 32 bytes");

struct LdacCodecInformation {
  // Must always be set to kLdacVendorId
  uint32_t vendor_id;

  // Must always be set to kLdacCodecId
  // All other values are reserved
  uint16_t codec_id;

  // Bitmask: bitrate index (see A2dpBitrateIndex for bitmask values)
  A2dpBitrateIndex bitrate_index;

  // Bitmask: LDAC channel mode (see A2dpLdacChannelMode for bitmask values)
  A2dpLdacChannelMode ldac_channel_mode;

  // Bytes 8 - 31 are reserved
  uint8_t reserved[24];
} __attribute__((packed));

static_assert(sizeof(LdacCodecInformation) == 32,
              "LdacCodecInformation must take up exactly 32 bytes");

struct AptxCodecInformation {
  // Bits 0 - 31 are reserved
  uint8_t reserved[32];
} __attribute__((packed));

static_assert(sizeof(AptxCodecInformation) == 32,
              "AptxCodecInformation must take up exactly 32 bytes");

// TODO(fxbug.dev/128280): Migrate A2dpOffloadCodecInformation to Emboss
union A2dpOffloadCodecInformation {
  SbcCodecInformation sbc;
  AacCodecInformation aac;
  LdacCodecInformation ldac;
  AptxCodecInformation aptx;
} __attribute__((packed));

static_assert(sizeof(A2dpOffloadCodecInformation) == 32,
              "A2dpOffloadCodecInformation must take up exactly 32 bytes");

struct StartA2dpOffloadCommandReturnParams {
  StatusCode status;

  // Will always be set to kStartA2dpOffloadCommandSubopcode
  uint8_t opcode;
} __attribute__((packed));

struct StopA2dpOffloadCommandReturnParams {
  StatusCode status;

  // Will always be set to kStopA2dpOffloadCommandSubopcode
  uint8_t opcode;
} __attribute__((packed));

// ============================================================================
// Multiple Advertising
//
// NOTE: Multiple advertiser support is deprecated in the Google feature spec
// v0.98 and above. Users of the following vendor extension HCI commands should
// first ensure that the controller is using a compatible Google feature spec.

// The kLEMultiAdvt opcode is shared across all multiple advertising HCI
// commands. To differentiate between the multiple commands, a subopcode field
// is included in the command payload.
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

  // Used to determine the packet type that is used for advertising when
  // advertising is enabled (see hci_constants.h)
  pw::bluetooth::emboss::LEAdvertisingType adv_type;

  pw::bluetooth::emboss::LEOwnAddressType own_address_type;
  LEPeerAddressType peer_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the device to be connected.
  DeviceAddressBytes peer_address;

  // (See the constants kLEAdvertisingChannel* in hci_constants.h for possible
  // values).
  uint8_t adv_channel_map;

  // This parameter shall be ignored when directed advertising is enabled (see
  // hci_constants.h for possible values).
  LEAdvFilterPolicy adv_filter_policy;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Transmit_Power, Unit: dBm
  // Range (-70 to +20)
  int8_t adv_tx_power;
} __attribute__((packed));

struct LEMultiAdvtSetAdvtParamReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetAdvtParametersSubopcode
  uint8_t opcode;
} __attribute__((packed));

// =======================================
// LE Multiple Advertising Set Advertising Data
constexpr uint8_t kLEMultiAdvtSetAdvtDataSubopcode = 0x2;

struct LEMultiAdvtSetAdvtDataCommandParams {
  // Must always be set to kLEMultiAdvtSetAdvtDataSubopcode
  uint8_t opcode;

  // Length of the advertising data included in this command packet, up to
  // kMaxLEAdvertisingDataLength bytes.
  uint8_t adv_data_length;

  // 31 octets of advertising data formatted as defined in Core Spec v5.0, Vol
  // 3, Part C, Section 11
  uint8_t adv_data[kMaxLEAdvertisingDataLength];

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __attribute__((packed));

struct LEMultiAdvtSetAdvtDataReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetAdvtDataSubopcode
  uint8_t opcode;
} __attribute__((packed));

// =======================================
// LE Multiple Advertising Set Scan Response
constexpr uint8_t kLEMultiAdvtSetScanRespSubopcode = 0x3;

struct LEMultiAdvtSetScanRespCommandParams {
  // Must always be set to kLEMultiAdvtSetScanRespSubopcode
  uint8_t opcode;

  // Length of the scan response data included in this command packet, up to
  // kMaxLEAdvertisingDataLength bytes.
  uint8_t scan_rsp_data_length;

  // 31 octets of advertising data formatted as defined in Core Spec v5.0, Vol
  // 3, Part C, Section 11
  uint8_t scan_rsp_data[kMaxLEAdvertisingDataLength];

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __attribute__((packed));

struct LEMultiAdvtSetScanRespReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetScanRespSubopcode
  uint8_t opcode;
} __attribute__((packed));

// =======================================
// LE Multiple Advertising Set Random Address
constexpr uint8_t kLEMultiAdvtSetRandomAddrSubopcode = 0x4;

struct LEMultiAdvtSetRandomAddrCommandParams {
  // Must always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;

  DeviceAddressBytes random_address;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __attribute__((packed));

struct LEMultiAdvtSetRandomAddrReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;
} __attribute__((packed));

// =======================================
// LE Multiple Advertising Set Advertising Enable
constexpr uint8_t kLEMultiAdvtEnableSubopcode = 0x5;

struct LEMultiAdvtEnableReturnParams {
  StatusCode status;

  // Will always be set to kLEMultiAdvtSetRandomAddrSubopcode
  uint8_t opcode;
} __attribute__((packed));

// ======= Events =======

// LE multi-advertising state change sub-event
constexpr EventCode kLEMultiAdvtStateChangeSubeventCode = 0x55;

}  // namespace bt::hci_spec::vendor::android

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_VENDOR_PROTOCOL_H_
