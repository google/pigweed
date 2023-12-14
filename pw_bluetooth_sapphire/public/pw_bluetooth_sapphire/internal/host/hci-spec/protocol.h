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
#include <pw_bluetooth/hci_common.emb.h>
#include <pw_bluetooth/hci_events.emb.h>

#include <array>
#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/device_address.h"
#include "pw_bluetooth_sapphire/internal/host/common/device_class.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/uint128.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"

// This file contains general opcode/number and static packet definitions for
// the Bluetooth Host-Controller Interface. Each packet payload structure
// contains parameter descriptions based on their respective documentation in
// the Bluetooth Core Specification version 5.0
//
// NOTE: Avoid casting raw buffer pointers to the packet payload structure types
// below; use as template parameter to PacketView::payload(),
// MutableBufferView::mutable_payload(), or CommandPacket::mutable_payload()
// instead. Take extra care when accessing flexible array members.

#pragma clang diagnostic ignored "-Wc99-extensions"

namespace bt::hci_spec {

using pw::bluetooth::emboss::ConnectionRole;
using pw::bluetooth::emboss::GenericEnableParam;
using pw::bluetooth::emboss::StatusCode;

// HCI opcode as used in command packets.
using OpCode = uint16_t;

// HCI event code as used in event packets.
using EventCode = uint8_t;

// Data Connection Handle used for ACL and SCO logical link connections.
using ConnectionHandle = uint16_t;

// Handle used to identify an advertising set used in the 5.0 Extended
// Advertising feature.
using AdvertisingHandle = uint8_t;

// Handle used to identify a periodic advertiser used in the 5.0 Periodic
// Advertising feature.
using PeriodicAdvertiserHandle = uint16_t;

// Returns the OGF (OpCode Group Field) which occupies the upper 6-bits of the
// opcode.
inline uint8_t GetOGF(const OpCode opcode) { return opcode >> 10; }

// Returns the OCF (OpCode Command Field) which occupies the lower 10-bits of
// the opcode.
inline uint16_t GetOCF(const OpCode opcode) { return opcode & 0x3FF; }

// Returns the opcode based on the given OGF and OCF fields.
constexpr OpCode DefineOpCode(const uint8_t ogf, const uint16_t ocf) {
  return static_cast<uint16_t>(((ogf & 0x3F) << 10) | (ocf & 0x03FF));
}

// ========================= HCI packet headers ==========================
// NOTE(armansito): The definitions below are incomplete since they get added as
// needed. This list will grow as we support more features.

struct CommandHeader {
  uint16_t opcode;
  uint8_t parameter_total_size;
} __attribute__((packed));

struct EventHeader {
  uint8_t event_code;
  uint8_t parameter_total_size;
} __attribute__((packed));

struct ACLDataHeader {
  // The first 16-bits contain the following fields, in order:
  //   - 12-bits: Connection Handle
  //   - 2-bits: Packet Boundary Flags
  //   - 2-bits: Broadcast Flags
  uint16_t handle_and_flags;

  // Length of data following the header.
  uint16_t data_total_length;
} __attribute__((packed));

struct SynchronousDataHeader {
  // The first 16-bits contain the following fields, in order:
  // - 12-bits: Connection Handle
  // - 2-bits: Packet Status Flag
  // - 2-bits: RFU
  uint16_t handle_and_flags;

  // Length of the data following the header.
  uint8_t data_total_length;
} __attribute__((packed));

// Generic return parameter struct for commands that only return a status. This
// can also be used to check the status of HCI commands with more complex return
// parameters.
struct SimpleReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
} __attribute__((packed));

// ============= HCI Command and Event (op)code and payloads =============

// No-Op
constexpr OpCode kNoOp = 0x0000;

// The following is a list of HCI command and event declarations sorted by OGF
// category. Within each category the commands are sorted by their OCF. Each
// declaration is preceded by the name of the command or event followed by the
// Bluetooth Core Specification version in which it was introduced. Commands
// that apply to a specific Bluetooth sub-technology
// (e.g. BR/EDR, LE, AMP) will also contain that definition.
//
// NOTE(armansito): This list is incomplete. Entries will be added as needed.

// ======= Link Control Commands =======
// Core Spec v5.0, Vol 2, Part E, Section 7.1
constexpr uint8_t kLinkControlOGF = 0x01;
constexpr OpCode LinkControlOpCode(const uint16_t ocf) {
  return DefineOpCode(kLinkControlOGF, ocf);
}

// ===============================
// Inquiry Command (v1.1) (BR/EDR)
constexpr OpCode kInquiry = LinkControlOpCode(0x0001);

// ======================================
// Inquiry Cancel Command (v1.1) (BR/EDR)
constexpr OpCode kInquiryCancel = LinkControlOpCode(0x0002);

// Inquiry Cancel Command has no command parameters.

// =================================
// Create Connection (v1.1) (BR/EDR)
constexpr OpCode kCreateConnection = LinkControlOpCode(0x0005);

// =======================================
// Disconnect Command (v1.1) (BR/EDR & LE)
constexpr OpCode kDisconnect = LinkControlOpCode(0x0006);

// ========================================
// Create Connection Cancel (v1.1) (BR/EDR)
constexpr OpCode kCreateConnectionCancel = LinkControlOpCode(0x0008);

struct CreateConnectionCancelReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the Create Connection Command Request
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// =========================================
// Accept Connection Request (v1.1) (BR/EDR)
constexpr OpCode kAcceptConnectionRequest = LinkControlOpCode(0x0009);

// =========================================
// Reject Connection Request (v1.1) (BR/EDR)
constexpr OpCode kRejectConnectionRequest = LinkControlOpCode(0x000A);

// ==============================================
// Link Key Request Reply Command (v1.1) (BR/EDR)
constexpr OpCode kLinkKeyRequestReply = LinkControlOpCode(0x000B);

constexpr size_t kBrEdrLinkKeySize = 16;

struct LinkKeyRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the device whose Link Key Request was fulfilled.
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// =======================================================
// Link Key Request Negative Reply Command (v1.1) (BR/EDR)
constexpr OpCode kLinkKeyRequestNegativeReply = LinkControlOpCode(0x000C);

struct LinkKeyRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the device whose Link Key Request was denied.
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// ================================================
// Authentication Requested Command (v1.1) (BR/EDR)
constexpr OpCode kAuthenticationRequested = LinkControlOpCode(0x0011);

// =================================================
// Set Connection Encryption Command (v1.1) (BR/EDR)
constexpr OpCode kSetConnectionEncryption = LinkControlOpCode(0x0013);

// ============================================================
// Remote Name Request Command (v1.1) (BR/EDR)
constexpr OpCode kRemoteNameRequest = LinkControlOpCode(0x0019);

// ======================================================
// Read Remote Supported Features Command (v1.1) (BR/EDR)
constexpr OpCode kReadRemoteSupportedFeatures = LinkControlOpCode(0x001B);

// =====================================================
// Read Remote Extended Features Command (v1.2) (BR/EDR)
constexpr OpCode kReadRemoteExtendedFeatures = LinkControlOpCode(0x001C);

// ============================================================
// Read Remote Version Information Command (v1.1) (BR/EDR & LE)
constexpr OpCode kReadRemoteVersionInfo = LinkControlOpCode(0x001D);

// =============================================
// Reject Synchronous Connection Command (BR/EDR)
constexpr OpCode kRejectSynchronousConnectionRequest =
    LinkControlOpCode(0x002A);

// =========================================================
// IO Capability Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kIOCapabilityRequestReply = LinkControlOpCode(0x002B);

struct IOCapabilityRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// =============================================================
// User Confirmation Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserConfirmationRequestReply = LinkControlOpCode(0x002C);

// ======================================================================
// User Confirmation Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserConfirmationRequestNegativeReply =
    LinkControlOpCode(0x002D);

// ========================================================
// User Passkey Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserPasskeyRequestReply = LinkControlOpCode(0x002E);

// =================================================================
// User Passkey Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserPasskeyRequestNegativeReply = LinkControlOpCode(0x002F);

// ==================================================================
// IO Capability Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kIOCapabilityRequestNegativeReply = LinkControlOpCode(0x0034);

struct IOCapabilityRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// ======================================================
// Enhanced Setup Synchronous Connection Command (BR/EDR)
constexpr OpCode kEnhancedSetupSynchronousConnection =
    LinkControlOpCode(0x003D);

// ===============================================================
// Enhanced Accept Synchronous Connection Request Command (BR/EDR)
constexpr OpCode kEnhancedAcceptSynchronousConnectionRequest =
    LinkControlOpCode(0x003E);

// ======= Controller & Baseband Commands =======
// Core Spec v5.0 Vol 2, Part E, Section 7.3
constexpr uint8_t kControllerAndBasebandOGF = 0x03;
constexpr OpCode ControllerAndBasebandOpCode(const uint16_t ocf) {
  return DefineOpCode(kControllerAndBasebandOGF, ocf);
}

// =============================
// Set Event Mask Command (v1.1)
constexpr OpCode kSetEventMask = ControllerAndBasebandOpCode(0x0001);

// ====================
// Reset Command (v1.1)
constexpr OpCode kReset = ControllerAndBasebandOpCode(0x0003);

// ========================================
// Write Local Name Command (v1.1) (BR/EDR)
constexpr OpCode kWriteLocalName = ControllerAndBasebandOpCode(0x0013);

// =======================================
// Read Local Name Command (v1.1) (BR/EDR)
constexpr OpCode kReadLocalName = ControllerAndBasebandOpCode(0x0014);

struct ReadLocalNameReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // A UTF-8 encoded User Friendly Descriptive Name for the device.
  // If the name contained in the parameter is shorter than 248 octets, the end
  // of the name is indicated by a NULL octet (0x00), and the following octets
  // (to fill up 248 octets, which is the length of the parameter) do not have
  // valid values.
  uint8_t local_name[kMaxNameLength];
} __attribute__((packed));

// ==========================================
// Write Page Timeout Command (v1.1) (BR/EDR)
constexpr OpCode kWritePageTimeout = ControllerAndBasebandOpCode(0x0018);

struct WritePageTimeoutReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
} __attribute__((packed));

// ========================================
// Read Scan Enable Command (v1.1) (BR/EDR)
constexpr OpCode kReadScanEnable = ControllerAndBasebandOpCode(0x0019);

struct ReadScanEnableReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask of enabled scans. See enum class ScanEnableBit in hci_constants.h
  // for how to interpret this bitfield.
  ScanEnableType scan_enable;
} __attribute__((packed));

// =========================================
// Write Scan Enable Command (v1.1) (BR/EDR)
constexpr OpCode kWriteScanEnable = ControllerAndBasebandOpCode(0x001A);

// ===============================================
// Read Page Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kReadPageScanActivity = ControllerAndBasebandOpCode(0x001B);

struct ReadPageScanActivityReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Page_Scan_Interval, in time slices (0.625ms)
  // Range: kPageScanIntervalMin - kPageScanIntervalMax in hci_constants.h
  uint16_t page_scan_interval;

  // Page_Scan_Window, in time slices
  // Range: kPageScanWindowMin - kPageScanWindowMax in hci_constants.h
  uint16_t page_scan_window;
} __attribute__((packed));

// ================================================
// Write Page Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kWritePageScanActivity = ControllerAndBasebandOpCode(0x001C);

// ===============================================
// Read Inquiry Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kReadInquiryScanActivity = ControllerAndBasebandOpCode(0x001D);

struct ReadInquiryScanActivityReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Inquiry_Scan_Interval, in time slices (0.625ms)
  // Range: kInquiryScanIntervalMin - kInquiryScanIntervalMax in hci_constants.h
  uint16_t inquiry_scan_interval;

  // Inquiry_Scan_Window, in time slices
  // Range: kInquiryScanWindowMin - kInquiryScanWindowMax in hci_constants.h
  uint16_t inquiry_scan_window;
} __attribute__((packed));

// ================================================
// Write Inquiry Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kWriteInquiryScanActivity =
    ControllerAndBasebandOpCode(0x001E);

// ============================================
// Read Class of Device Command (v1.1) (BR/EDR)
constexpr OpCode kReadClassOfDevice = ControllerAndBasebandOpCode(0x0023);

struct ReadClassOfDeviceReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  DeviceClass class_of_device;
} __attribute__((packed));

// =============================================
// Write Class Of Device Command (v1.1) (BR/EDR)
constexpr OpCode kWriteClassOfDevice = ControllerAndBasebandOpCode(0x0024);

// =============================================
// Write Automatic Flush Timeout Command (v1.1) (BR/EDR)

constexpr OpCode kWriteAutomaticFlushTimeout =
    ControllerAndBasebandOpCode(0x0028);

// ===============================================================
// Read Transmit Transmit Power Level Command (v1.1) (BR/EDR & LE)
constexpr OpCode kReadTransmitPowerLevel = ControllerAndBasebandOpCode(0x002D);

struct ReadTransmitPowerLevelCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The type of transmit power level to read.
  ReadTransmitPowerType type;
} __attribute__((packed));

struct ReadTransmitPowerLevelReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Transmit power level.
  //
  //   Range: -30 ≤ N ≤ 20
  //   Units: dBm
  int8_t tx_power_level;
} __attribute__((packed));

// ===============================================================
// Write Synchonous Flow Control Enable Command (BR/EDR)
constexpr OpCode kWriteSynchronousFlowControlEnable =
    ControllerAndBasebandOpCode(0x002F);

// ===================================
// Read Inquiry Scan Type (v1.2) (BR/EDR)
constexpr OpCode kReadInquiryScanType = ControllerAndBasebandOpCode(0x0042);

struct ReadInquiryScanTypeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class InquiryScanType in hci_constants.h for possible values.
  InquiryScanType inquiry_scan_type;
} __attribute__((packed));

// ====================================
// Write Inquiry Scan Type (v1.2) (BR/EDR)
constexpr OpCode kWriteInquiryScanType = ControllerAndBasebandOpCode(0x0043);

// =================================
// Read Inquiry Mode (v1.2) (BR/EDR)
constexpr OpCode kReadInquiryMode = ControllerAndBasebandOpCode(0x0044);

struct ReadInquiryModeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  pw::bluetooth::emboss::InquiryMode inquiry_mode;
} __attribute__((packed));

// ==================================
// Write Inquiry Mode (v1.2) (BR/EDR)
constexpr OpCode kWriteInquiryMode = ControllerAndBasebandOpCode(0x0045);

// ===================================
// Read Page Scan Type (v1.2) (BR/EDR)
constexpr OpCode kReadPageScanType = ControllerAndBasebandOpCode(0x0046);

struct ReadPageScanTypeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class PageScanType in hci_constants.h for possible values.
  pw::bluetooth::emboss::PageScanType page_scan_type;
} __attribute__((packed));

// ====================================
// Write Page Scan Type (v1.2) (BR/EDR)
constexpr OpCode kWritePageScanType = ControllerAndBasebandOpCode(0x0047);

// =================================
// Write Extended Inquiry Response (v1.2) (BR/EDR)
constexpr OpCode kWriteExtendedInquiryResponse =
    ControllerAndBasebandOpCode(0x0052);

// ==============================================
// Read Simple Pairing Mode (v2.1 + EDR) (BR/EDR)
constexpr OpCode kReadSimplePairingMode = ControllerAndBasebandOpCode(0x0055);

struct ReadSimplePairingModeReturnParams {
  // See enum StatusCode in hci_constants.h
  StatusCode status;

  // Simple pairing Mode.
  GenericEnableParam simple_pairing_mode;
} __attribute__((packed));

// ===============================================
// Write Simple Pairing Mode (v2.1 + EDR) (BR/EDR)
constexpr OpCode kWriteSimplePairingMode = ControllerAndBasebandOpCode(0x0056);

// =========================================
// Set Event Mask Page 2 Command (v3.0 + HS)
constexpr OpCode kSetEventMaskPage2 = ControllerAndBasebandOpCode(0x0063);

struct SetEventMaskPage2CommandParams {
  // Bit mask used to control which HCI events are generated by the HCI for the
  // Host. See enum class EventMaskPage2 in hci_constants.h
  uint64_t event_mask;
} __attribute__((packed));

// =========================================================
// Read Flow Control Mode Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kReadFlowControlMode = ControllerAndBasebandOpCode(0x0066);

struct ReadFlowControlModeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class FlowControlMode in hci_constants.h for possible values.
  FlowControlMode flow_control_mode;
} __attribute__((packed));

// ==========================================================
// Write Flow Control Mode Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kWriteFlowControlMode = ControllerAndBasebandOpCode(0x0067);

struct WriteFlowControlModeCommandParams {
  // See enum class FlowControlMode in hci_constants.h for possible values.
  FlowControlMode flow_control_mode;
} __attribute__((packed));

// ============================================
// Read LE Host Support Command (v4.0) (BR/EDR)
constexpr OpCode kReadLEHostSupport = ControllerAndBasebandOpCode(0x006C);

struct ReadLEHostSupportReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  GenericEnableParam le_supported_host;

  // Core Spec v5.0, Vol 2, Part E, Section 6.35: This value is set to "disabled
  // (0x00)" by default and "shall be ignored".
  uint8_t simultaneous_le_host;
} __attribute__((packed));

// =============================================
// Write LE Host Support Command (v4.0) (BR/EDR)
constexpr OpCode kWriteLEHostSupport = ControllerAndBasebandOpCode(0x006D);

// =============================================
// Write Secure Connections Host Support Command (v4.1) (BR/EDR)
constexpr OpCode kWriteSecureConnectionsHostSupport =
    ControllerAndBasebandOpCode(0x007A);

// ===============================================================
// Read Authenticated Payload Timeout Command (v4.1) (BR/EDR & LE)
constexpr OpCode kReadAuthenticatedPayloadTimeout =
    ControllerAndBasebandOpCode(0x007B);

struct ReadAuthenticatedPayloadTimeoutCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

struct ReadAuthenticatedPayloadTimeoutReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Default = 0x0BB8 (30 s)
  // Range: 0x0001 to 0xFFFF
  // Time = N * 10 ms
  // Time Range: 10 ms to 655,350 ms
  uint16_t authenticated_payload_timeout;
} __attribute__((packed));

// ================================================================
// Write Authenticated Payload Timeout Command (v4.1) (BR/EDR & LE)
constexpr OpCode kWriteAuthenticatedPayloadTimeout =
    ControllerAndBasebandOpCode(0x007C);

struct WriteAuthenticatedPayloadTimeoutCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Default = 0x0BB8 (30 s)
  // Range: 0x0001 to 0xFFFF
  // Time = N * 10 ms
  // Time Range: 10 ms to 655,350 ms
  uint16_t authenticated_payload_timeout;
} __attribute__((packed));

struct WriteAuthenticatedPayloadTimeoutReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// ======= Informational Parameters =======
// Core Spec v5.0 Vol 2, Part E, Section 7.4
constexpr uint8_t kInformationalParamsOGF = 0x04;
constexpr OpCode InformationalParamsOpCode(const uint16_t ocf) {
  return DefineOpCode(kInformationalParamsOGF, ocf);
}

// =============================================
// Read Local Version Information Command (v1.1)
constexpr OpCode kReadLocalVersionInfo = InformationalParamsOpCode(0x0001);

struct ReadLocalVersionInfoReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  pw::bluetooth::emboss::CoreSpecificationVersion hci_version;

  uint16_t hci_revision;
  uint8_t lmp_pal_version;
  uint16_t manufacturer_name;
  uint16_t lmp_pal_subversion;
} __attribute__((packed));

// ============================================
// Read Local Supported Commands Command (v1.2)
constexpr OpCode kReadLocalSupportedCommands =
    InformationalParamsOpCode(0x0002);

struct ReadLocalSupportedCommandsReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class SupportedCommand in hci_constants.h for how to interpret
  // this bitfield.
  uint8_t supported_commands[64];
} __attribute__((packed));

// ============================================
// Read Local Supported Features Command (v1.1)
constexpr OpCode kReadLocalSupportedFeatures =
    InformationalParamsOpCode(0x0003);

struct ReadLocalSupportedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask List of LMP features. See enum class LMPFeature in hci_constants.h
  // for how to interpret this bitfield.
  uint64_t lmp_features;
} __attribute__((packed));

// ====================================================
// Read Local Extended Features Command (v1.2) (BR/EDR)
constexpr OpCode kReadLocalExtendedFeatures = InformationalParamsOpCode(0x0004);

struct ReadLocalExtendedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  uint8_t page_number;
  uint8_t maximum_page_number;
  uint64_t extended_lmp_features;
} __attribute__((packed));

// ===============================
// Read Buffer Size Command (v1.1)
constexpr OpCode kReadBufferSize = InformationalParamsOpCode(0x0005);

struct ReadBufferSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t hc_acl_data_packet_length;
  uint8_t hc_synchronous_data_packet_length;
  uint16_t hc_total_num_acl_data_packets;
  uint16_t hc_total_num_synchronous_data_packets;
} __attribute__((packed));

// ========================================
// Read BD_ADDR Command (v1.1) (BR/EDR, LE)
constexpr OpCode kReadBDADDR = InformationalParamsOpCode(0x0009);

struct ReadBDADDRReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// =======================================================
// Read Data Block Size Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kReadDataBlockSize = InformationalParamsOpCode(0x000A);

struct ReadDataBlockSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t max_acl_data_packet_length;
  uint16_t data_block_length;
  uint16_t total_num_data_blocks;
} __attribute__((packed));

// ======= Events =======
// Core Spec v5.0 Vol 2, Part E, Section 7.7

// Reserved for vendor-specific debug events
// (Vol 2, Part E, Section 5.4.4)
constexpr EventCode kVendorDebugEventCode = 0xFF;

// ======================================
// Inquiry Complete Event (v1.1) (BR/EDR)
constexpr EventCode kInquiryCompleteEventCode = 0x01;

// ====================================
// Inquiry Result Event (v1.1) (BR/EDR)
constexpr EventCode kInquiryResultEventCode = 0x02;

// =========================================
// Connection Complete Event (v1.1) (BR/EDR)
constexpr EventCode kConnectionCompleteEventCode = 0x03;

// ========================================
// Connection Request Event (v1.1) (BR/EDR)
constexpr EventCode kConnectionRequestEventCode = 0x04;

// =================================================
// Disconnection Complete Event (v1.1) (BR/EDR & LE)
constexpr EventCode kDisconnectionCompleteEventCode = 0x05;

// =============================================
// Authentication Complete Event (v1.1) (BR/EDR)
constexpr EventCode kAuthenticationCompleteEventCode = 0x06;

// ==================================================
// Remote Name Request Complete Event (v1.1) (BR/EDR)
constexpr EventCode kRemoteNameRequestCompleteEventCode = 0x07;

// ============================================
// Encryption Change Event (v1.1) (BR/EDR & LE)
constexpr EventCode kEncryptionChangeEventCode = 0x08;

// =========================================================
// Change Connection Link Key Complete Event (v1.1) (BR/EDR)
constexpr EventCode kChangeConnectionLinkKeyCompleteEventCode = 0x09;

struct ChangeConnectionLinkKeyCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// =============================================================
// Read Remote Supported Features Complete Event (v1.1) (BR/EDR)
constexpr EventCode kReadRemoteSupportedFeaturesCompleteEventCode = 0x0B;

struct ReadRemoteSupportedFeaturesCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // A connection handle for an ACL connection.
  //  Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Bit Mask List of LMP features. See enum class LMPFeature in hci_constants.h
  // for how to interpret this bitfield.
  uint64_t lmp_features;
} __attribute__((packed));

// ===================================================================
// Read Remote Version Information Complete Event (v1.1) (BR/EDR & LE)
constexpr EventCode kReadRemoteVersionInfoCompleteEventCode = 0x0C;

// =============================
// Command Complete Event (v1.1)
constexpr EventCode kCommandCompleteEventCode = 0x0E;

struct CommandCompleteEventParams {
  CommandCompleteEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(CommandCompleteEventParams);

  // The Number of HCI command packets which are allowed to be sent to the
  // Controller from the Host.
  uint8_t num_hci_command_packets;

  // OpCode of the command which caused this event.
  uint16_t command_opcode;

  // This is the return parameter(s) for the command specified in the
  // |command_opcode| event parameter. Refer to the Bluetooth Core Specification
  // v5.0, Vol 2, Part E for each command’s definition for the list of return
  // parameters associated with that command.
  uint8_t return_parameters[];
} __attribute__((packed));

// ===========================
// Command Status Event (v1.1)
constexpr EventCode kCommandStatusEventCode = 0x0F;
constexpr uint8_t kCommandStatusPending = 0x00;

struct CommandStatusEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The Number of HCI command packets which are allowed to be sent to the
  // Controller from the Host.
  uint8_t num_hci_command_packets;

  // OpCode of the command which caused this event and is pending completion.
  uint16_t command_opcode;
} __attribute__((packed));

// ===========================
// Hardware Error Event (v1.1)
constexpr EventCode kHardwareErrorEventCode = 0x10;

struct HardwareErrorEventParams {
  // These Hardware_Codes will be implementation-specific, and can be assigned
  // to indicate various hardware problems.
  uint8_t hardware_code;
} __attribute__((packed));

// ========================================
// Role Change Event (BR/EDR) (v1.1)
constexpr EventCode kRoleChangeEventCode = 0x12;

// ========================================
// Number Of Completed Packets Event (v1.1)
constexpr EventCode kNumberOfCompletedPacketsEventCode = 0x13;

struct NumberOfCompletedPacketsEventData {
  uint16_t connection_handle;
  uint16_t hc_num_of_completed_packets;
} __attribute__((packed));

struct NumberOfCompletedPacketsEventParams {
  NumberOfCompletedPacketsEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(NumberOfCompletedPacketsEventParams);

  uint8_t number_of_handles;
  NumberOfCompletedPacketsEventData data[];
} __attribute__((packed));

// ======================================
// Link Key Request Event (v1.1) (BR/EDR)
constexpr EventCode kLinkKeyRequestEventCode = 0x17;

// ===========================================
// Link Key Notification Event (v1.1) (BR/EDR)
constexpr EventCode kLinkKeyNotificationEventCode = 0x18;

// ===========================================
// Data Buffer Overflow Event (v1.1) (BR/EDR & LE)
constexpr EventCode kDataBufferOverflowEventCode = 0x1A;

// ==============================================
// Inquiry Result with RSSI Event (v1.2) (BR/EDR)
constexpr EventCode kInquiryResultWithRSSIEventCode = 0x22;

// ============================================================
// Read Remote Extended Features Complete Event (v1.1) (BR/EDR)
constexpr EventCode kReadRemoteExtendedFeaturesCompleteEventCode = 0x23;

// ============================================================
// Synchronous Connection Complete Event (BR/EDR)
constexpr EventCode kSynchronousConnectionCompleteEventCode = 0x2C;

// =============================================
// Extended Inquiry Result Event (v1.2) (BR/EDR)
constexpr EventCode kExtendedInquiryResultEventCode = 0x2F;

// ================================================================
// Encryption Key Refresh Complete Event (v2.1 + EDR) (BR/EDR & LE)
constexpr EventCode kEncryptionKeyRefreshCompleteEventCode = 0x30;

// =================================================
// IO Capability Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kIOCapabilityRequestEventCode = 0x31;

// ==================================================
// IO Capability Response Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kIOCapabilityResponseEventCode = 0x32;

// =====================================================
// User Confirmation Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserConfirmationRequestEventCode = 0x33;

struct UserConfirmationRequestEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // Numeric value to be displayed. Valid values are 0 - 999999.
  uint32_t numeric_value;
} __attribute__((packed));

// ================================================
// User Passkey Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserPasskeyRequestEventCode = 0x34;

struct UserPasskeyRequestEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// ===================================================
// Simple Pairing Complete Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kSimplePairingCompleteEventCode = 0x36;

struct SimplePairingCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __attribute__((packed));

// =====================================================
// User Passkey Notification Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserPasskeyNotificationEventCode = 0x3B;

struct UserPasskeyNotificationEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // Numeric value (passkey) entered by user. Valid values are 0 - 999999.
  uint32_t numeric_value;
} __attribute__((packed));

// =========================
// LE Meta Event (v4.0) (LE)
constexpr EventCode kLEMetaEventCode = 0x3E;

struct LEMetaEventParams {
  LEMetaEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEMetaEventParams);

  // The event code for the LE subevent.
  EventCode subevent_code;

  // Beginning of parameters that are specific to the LE subevent.
  uint8_t subevent_parameters[];
} __attribute__((packed));

// LE Connection Complete Event (v4.0) (LE)
constexpr EventCode kLEConnectionCompleteSubeventCode = 0x01;

// LE Advertising Report Event (v4.0) (LE)
constexpr EventCode kLEAdvertisingReportSubeventCode = 0x02;

struct LEAdvertisingReportData {
  LEAdvertisingReportData() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEAdvertisingReportData);

  // The event type.
  LEAdvertisingEventType event_type;

  // Type of |address| for the advertising device.
  LEAddressType address_type;

  // Public Device Address, Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the advertising device.
  DeviceAddressBytes address;

  // Length of the advertising data payload.
  uint8_t length_data;

  // The beginning of |length_data| octets of advertising or scan response data
  // formatted as defined in Core Spec v5.0, Vol 3, Part C, Section 11.
  uint8_t data[];

  // Immediately following |data| there is a single octet field containing the
  // received signal strength for this advertising report. Since |data| has a
  // variable length we do not declare it as a field within this struct.
  //
  //   Range: -127 <= N <= +20
  //   Units: dBm
  //   If N == 127: RSSI is not available.
  //
  // int8_t rssi;
} __attribute__((packed));

struct LEAdvertisingReportSubeventParams {
  LEAdvertisingReportSubeventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEAdvertisingReportSubeventParams);

  // Number of LEAdvertisingReportData instances contained in the array
  // |reports|.
  uint8_t num_reports;

  // Beginning of LEAdvertisingReportData array. Since each report data has a
  // variable length, the contents of |reports| this is declared as an array of
  // uint8_t.
  uint8_t reports[];
} __attribute__((packed));

// LE Connection Update Complete Event (v4.0) (LE)
constexpr EventCode kLEConnectionUpdateCompleteSubeventCode = 0x03;

// LE Read Remote Features Complete Event (v4.0) (LE)
constexpr EventCode kLEReadRemoteFeaturesCompleteSubeventCode = 0x04;

// LE Long Term Key Request Event (v4.0) (LE)
constexpr EventCode kLELongTermKeyRequestSubeventCode = 0x05;

struct LELongTermKeyRequestSubeventParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // 64-bit random number.
  uint64_t random_number;

  // 16-bit encrypted diversifier.
  uint16_t encrypted_diversifier;
} __attribute__((packed));

// LE Remote Connection Parameter Request Event (v4.1) (LE)
constexpr EventCode kLERemoteConnectionParameterRequestSubeventCode = 0x06;

struct LERemoteConnectionParameterRequestSubeventParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Range: see kLEConnectionInterval[Min|Max] in hci_constants.h
  // Time: N * 1.25 ms
  // Time Range: 7.5 ms to 4 s.
  uint16_t interval_min;
  uint16_t interval_max;

  // Range: 0x0000 to kLEConnectionLatencyMax in hci_constants.h
  uint16_t latency;

  // Range: see kLEConnectionSupervisionTimeout[Min|Max] in hci_constants.h
  // Time: N * 10 ms
  // Time Range: 100 ms to 32 s
  uint16_t timeout;
} __attribute__((packed));

// LE Data Length Change Event (v4.2) (LE)
constexpr EventCode kLEDataLengthChangeSubeventCode = 0x07;

struct LEDataLengthChangeSubeventParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t max_tx_time;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t max_rx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t max_rx_time;
} __attribute__((packed));

// LE Read Local P-256 Public Key Complete Event (v4.2) (LE)
constexpr EventCode kLEReadLocalP256PublicKeyCompleteSubeventCode = 0x08;

struct LEReadLOcalP256PublicKeyCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Local P-256 public key.
  uint8_t local_p256_public_key[64];
} __attribute__((packed));

// LE Generate DHKey Complete Event (v4.2) (LE)
constexpr EventCode kLEGenerateDHKeyCompleteSubeventCode = 0x09;

struct LEGenerateDHKeyCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Diffie Hellman Key.
  uint8_t dh_key[32];
} __attribute__((packed));

// LE Enhanced Connection Complete Event (v4.2) (LE)
constexpr EventCode kLEEnhancedConnectionCompleteSubeventCode = 0x0A;

struct LEEnhancedConnectionCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  ConnectionRole role;
  LEAddressType peer_address_type;

  // Public Device Address, or Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the device to be connected.
  DeviceAddressBytes peer_address;

  DeviceAddressBytes local_resolvable_private_address;
  DeviceAddressBytes peer_resolvable_private_address;

  // Range: see kLEConnectionInterval[Min|Max] in hci_constants.h
  // Time: N * 1.25 ms
  // Time Range: 7.5 ms to 4 s.
  uint16_t conn_interval;

  // Range: 0x0000 to kLEConnectionLatencyMax in hci_constants.h
  uint16_t conn_latency;

  // Range: see kLEConnectionSupervisionTimeout[Min|Max] in hci_constants.h
  // Time: N * 10 ms
  // Time Range: 100 ms to 32 s
  uint16_t supervision_timeout;

  // The Central_Clock_Accuracy parameter is only valid for a peripheral. On a
  // central, this parameter shall be set to 0x00.
  pw::bluetooth::emboss::LEClockAccuracy central_clock_accuracy;
} __attribute__((packed));

// LE Directed Advertising Report Event (v4.2) (LE)
constexpr EventCode kLEDirectedAdvertisingReportSubeventCode = 0x0B;

struct LEDirectedAdvertisingReportData {
  // The event type. This is always equal to
  // LEAdvertisingEventType::kAdvDirectInd.
  LEAdvertisingEventType event_type;

  // Type of |address| for the advertising device.
  LEAddressType address_type;

  // Public Device Address, Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the advertising device.
  DeviceAddressBytes address;

  // By default this is set to LEAddressType::kRandom and |direct_address| will
  // contain a random device address.
  LEAddressType direct_address_type;
  DeviceAddressBytes direct_address;

  // Range: -127 <= N <= +20
  // Units: dBm
  // If N == 127: RSSI is not available.
  int8_t rssi;
} __attribute__((packed));

struct LEDirectedAdvertisingReportSubeventParams {
  LEDirectedAdvertisingReportSubeventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEDirectedAdvertisingReportSubeventParams);

  // Number of LEAdvertisingReportData instances contained in the array
  // |reports|.
  uint8_t num_reports;

  // The report array parameters.
  LEDirectedAdvertisingReportData reports[];
} __attribute__((packed));

// LE PHY Update Complete Event (v5.0) (LE)
constexpr EventCode kLEPHYUpdateCompleteSubeventCode = 0x0C;

struct LEPHYUpdateCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The transmitter PHY.
  LEPHY tx_phy;

  // The receiver PHY.
  LEPHY rx_phy;
} __attribute__((packed));

// LE Extended Advertising Report Event (v5.0) (LE)
constexpr EventCode kLEExtendedAdvertisingReportSubeventCode = 0x0D;

// LE Periodic Advertising Sync Established Event (v5.0) (LE)
constexpr EventCode kLEPeriodicAdvertisingSyncEstablishedSubeventCode = 0x0E;

struct LEPeriodicAdvertisingSyncEstablishedSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Handle used to identify the periodic advertiser (only the lower 12 bits are
  // meaningful).
  PeriodicAdvertiserHandle sync_handle;

  // Value of the Advertising SID subfield in the ADI field of the PDU.
  uint8_t advertising_sid;

  // Address type of the advertiser.
  LEAddressType advertiser_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the advertiser.
  DeviceAddressBytes advertiser_address;

  // Advertiser_PHY.
  LEPHY advertiser_phy;

  // Range: See kLEPeriodicAdvertisingInterval[Min|Max] in hci_constants.h
  // Time = N * 1.25 ms
  // Time Range: 7.5ms to 81.91875 s
  uint16_t periodic_adv_interval;

  // Advertiser_Clock_Accuracy.
  pw::bluetooth::emboss::LEClockAccuracy advertiser_clock_accuracy;
} __attribute__((packed));

// LE Periodic Advertising Report Event (v5.0) (LE)
constexpr EventCode kLEPeriodicAdvertisingReportSubeventCode = 0x0F;

struct LEPeriodicAdvertisingReportSubeventParams {
  LEPeriodicAdvertisingReportSubeventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEPeriodicAdvertisingReportSubeventParams);

  // (only the lower 12 bits are meaningful).
  PeriodicAdvertiserHandle sync_handle;

  // Range: -127 <= N <= +126
  // Units: dBm
  int8_t tx_power;

  // Range: -127 <= N <= +20
  // Units: dBm
  // If N == 127: RSSI is not available.
  int8_t rssi;

  // As of Core Spec v5.0 this parameter is intended to be used in a future
  // feature.
  uint8_t unused;

  // Data status of the periodic advertisement. Indicates whether or not the
  // controller has split the data into multiple reports.
  LEAdvertisingDataStatus data_status;

  // Length of the Data field.
  uint8_t data_length;

  // |data_length| octets of data received from a Periodic Advertising packet.
  uint8_t data[];
} __attribute__((packed));

// LE Periodic Advertising Sync Lost Event (v5.0) (LE)
constexpr EventCode kLEPeriodicAdvertisingSyncLostSubeventCode = 0x10;

struct LEPeriodicAdvertisingSyncLostSubeventParams {
  // Used to identify the periodic advertiser (only the lower 12 bits are
  // meaningful).
  PeriodicAdvertiserHandle sync_handle;
} __attribute__((packed));

// LE Scan Timeout Event (v5.0) (LE)
constexpr EventCode kLEScanTimeoutSubeventCode = 0x11;

// LE Advertising Set Terminated Event (v5.0) (LE)
constexpr EventCode kLEAdvertisingSetTerminatedSubeventCode = 0x012;

struct LEAdvertisingSetTerminatedSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Advertising Handle in which advertising has ended.
  AdvertisingHandle adv_handle;

  // Connection Handle of the connection whose creation ended the advertising.
  ConnectionHandle connection_handle;

  // Number of completed extended advertising events transmitted by the
  // Controller.
  uint8_t num_completed_extended_adv_events;
} __attribute__((packed));

// LE Scan Request Received Event (v5.0) (LE)
constexpr EventCode kLEScanRequestReceivedSubeventCode = 0x13;

struct LEScanRequestReceivedSubeventParams {
  // Used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Address type of the scanner address.
  LEAddressType scanner_address_type;

  // Public Device Address, Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the scanning device.
  DeviceAddressBytes scanner_address;
} __attribute__((packed));

// LE Channel Selection Algorithm Event (v5.0) (LE)
constexpr EventCode kLEChannelSelectionAlgorithmSubeventCode = 0x014;

// ================================================================
// Number Of Completed Data Blocks Event (v3.0 + HS) (BR/EDR & AMP)
constexpr EventCode kNumberOfCompletedDataBlocksEventCode = 0x48;

struct NumberOfCompletedDataBlocksEventData {
  // Handle (Connection Handle for a BR/EDR Controller or a Logical_Link Handle
  // for an AMP Controller).
  uint16_t handle;
  uint16_t num_of_completed_packets;
  uint16_t num_of_completed_blocks;
} __attribute__((packed));

struct NumberOfCompletedDataBlocksEventParams {
  NumberOfCompletedDataBlocksEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(NumberOfCompletedDataBlocksEventParams);

  uint16_t total_num_data_blocks;
  uint8_t number_of_handles;
  NumberOfCompletedDataBlocksEventData data[];
} __attribute__((packed));

// ================================================================
// Authenticated Payload Timeout Expired Event (v4.1) (BR/EDR & LE)
constexpr EventCode kAuthenticatedPayloadTimeoutExpiredEventCode = 0x57;

struct AuthenticatedPayloadTimeoutExpiredEventParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// ======= Status Parameters =======
// Core Spec v5.0, Vol 2, Part E, Section 7.5
constexpr uint8_t kStatusParamsOGF = 0x05;
constexpr OpCode StatusParamsOpCode(const uint16_t ocf) {
  return DefineOpCode(kStatusParamsOGF, ocf);
}

// ========================
// Read RSSI Command (v1.1)
constexpr OpCode kReadRSSI = StatusParamsOpCode(0x0005);

struct ReadRSSICommandParams {
  // The Handle for the connection for which the RSSI is to be read (only the
  // lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle handle;
} __attribute__((packed));

struct ReadRSSIReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The Handle for the connection for which the RSSI has been read (only the
  // lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle handle;

  // The Received Signal Strength Value.
  //
  // - BR/EDR:
  //     Range: -128 ≤ N ≤ 127 (signed integer)
  //     Units: dB
  //
  // - AMP:
  //     Range: AMP type specific (signed integer)
  //     Units: dBm
  //
  // - LE:
  //     Range: -127 to 20, 127 (signed integer)
  //     Units: dBm
  int8_t rssi;
} __attribute__((packed));

// ========================================
// Read Encryption Key Size (v1.1) (BR/EDR)
constexpr OpCode kReadEncryptionKeySize = StatusParamsOpCode(0x0008);

struct ReadEncryptionKeySizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Handle of the ACL connection whose encryption key size was read.
  ConnectionHandle connection_handle;

  // Encryption key size. See v5.0 Vol 2 Part C, Section 5.2.
  uint8_t key_size;
} __attribute__((packed));

// ======= LE Controller Commands =======
// Core Spec v5.0 Vol 2, Part E, Section 7.8
constexpr uint8_t kLEControllerCommandsOGF = 0x08;
constexpr OpCode LEControllerCommandOpCode(const uint16_t ocf) {
  return DefineOpCode(kLEControllerCommandsOGF, ocf);
}

// Returns true if the given |opcode| corresponds to a LE controller command.
inline bool IsLECommand(OpCode opcode) {
  return GetOGF(opcode) == kLEControllerCommandsOGF;
}

// =====================================
// LE Set Event Mask Command (v4.0) (LE)
constexpr OpCode kLESetEventMask = LEControllerCommandOpCode(0x0001);

// =======================================
// LE Read Buffer Size Command (v4.0) (LE)
constexpr OpCode kLEReadBufferSizeV1 = LEControllerCommandOpCode(0x0002);

struct LEReadBufferSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t hc_le_acl_data_packet_length;
  uint8_t hc_total_num_le_acl_data_packets;
} __attribute__((packed));

// ====================================================
// LE Read Local Supported Features Command (v4.0) (LE)
constexpr OpCode kLEReadLocalSupportedFeatures =
    LEControllerCommandOpCode(0x0003);

struct LEReadLocalSupportedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask List of supported LE features. See enum class LESupportedFeature
  // in hci_constants.h.
  uint64_t le_features;
} __attribute__((packed));

// =========================================
// LE Set Random Address Command (v4.0) (LE)
constexpr OpCode kLESetRandomAddress = LEControllerCommandOpCode(0x0005);

struct LESetRandomAddressCommandParams {
  DeviceAddressBytes random_address;
} __attribute__((packed));

// =================================================
// LE Set Advertising Parameters Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingParameters =
    LEControllerCommandOpCode(0x0006);

// ========================================================
// LE Read Advertising Channel Tx Power Command (v4.0) (LE)
constexpr OpCode kLEReadAdvertisingChannelTxPower =
    LEControllerCommandOpCode(0x0007);

struct LEReadAdvertisingChannelTxPowerReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The transmit power level used for LE advertising channel packets.
  //
  //   Range: -20 <= N <= +10
  //   Units: dBm
  //   Accuracy: +/- 4 dB
  int8_t tx_power;
} __attribute__((packed));

// ===========================================
// LE Set Advertising Data Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingData = LEControllerCommandOpCode(0x0008);

// =============================================
// LE Set Scan Response Data Command (v4.0) (LE)
constexpr OpCode kLESetScanResponseData = LEControllerCommandOpCode(0x0009);

// =============================================
// LE Set Advertising Enable Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingEnable = LEControllerCommandOpCode(0x000A);

// ==========================================
// LE Set Scan Parameters Command (v4.0) (LE)
constexpr OpCode kLESetScanParameters = LEControllerCommandOpCode(0x000B);

// ======================================
// LE Set Scan Enable Command (v4.0) (LE)
constexpr OpCode kLESetScanEnable = LEControllerCommandOpCode(0x000C);

// ========================================
// LE Create Connection Command (v4.0) (LE)
constexpr OpCode kLECreateConnection = LEControllerCommandOpCode(0x000D);

// NOTE on ReturnParams: No Command Complete event is sent by the Controller to
// indicate that this command has been completed. Instead, the LE Connection
// Complete or LE Enhanced Connection Complete event indicates that this command
// has been completed.

// ===============================================
// LE Create Connection Cancel Command (v4.0) (LE)
constexpr OpCode kLECreateConnectionCancel = LEControllerCommandOpCode(0x000E);

// ===========================================
// LE Read Filter Accept List Size Command (v4.0) (LE)
constexpr OpCode kLEReadFilterAcceptListSize =
    LEControllerCommandOpCode(0x000F);

struct LEReadFilterAcceptListSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  uint8_t filter_accept_list_size;
} __attribute__((packed));

// =======================================
// LE Clear Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLEClearFilterAcceptList = LEControllerCommandOpCode(0x0010);

// ===============================================
// LE Add Device To Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLEAddDeviceToFilterAcceptList =
    LEControllerCommandOpCode(0x0011);

struct LEAddDeviceToFilterAcceptListCommandParams {
  // The address type of the peer. The |address| parameter will be ignored if
  // |address_type| is set to LEPeerAddressType::kAnonymous.
  LEPeerAddressType address_type;

  // Public Device Address or Random Device Address of the device to be added to
  // the Filter Accept List
  DeviceAddressBytes address;
} __attribute__((packed));

// ====================================================
// LE Remove Device From Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLERemoveDeviceFromFilterAcceptList =
    LEControllerCommandOpCode(0x0012);

struct LERemoveDeviceFromFilterAcceptListCommandParams {
  // The address type of the peer. The |address| parameter will be ignored if
  // |address_type| is set to LEPeerAddressType::kAnonymous.
  LEPeerAddressType address_type;

  // Public Device Address or Random Device Address of the device to be removed
  // from the Filter Accept List
  DeviceAddressBytes address;
} __attribute__((packed));

// ========================================
// LE Connection Update Command (v4.0) (LE)
constexpr OpCode kLEConnectionUpdate = LEControllerCommandOpCode(0x0013);

// NOTE on Return Params: A Command Complete event is not sent by the Controller
// to indicate that this command has been completed. Instead, the LE Connection
// Update Complete event indicates that this command has been completed.

// ======================================================
// LE Set Host Channel Classification Command (v4.0) (LE)
constexpr OpCode kLESetHostChannelClassification =
    LEControllerCommandOpCode(0x0014);

struct LESetHostChannelClassificationCommandParams {
  // This parameter contains 37 1-bit fields (only the lower 37-bits of the
  // 5-octet value are meaningful).
  //
  // The nth such field (in the range 0 to 36) contains the value for the link
  // layer channel index n.
  //
  // Channel n is bad = 0. Channel n is unknown = 1.
  //
  // The most significant bits are reserved and shall be set to 0 for future
  // use.
  //
  // At least one channel shall be marked as unknown.
  uint8_t channel_map[5];
} __attribute__((packed));

// =======================================
// LE Read Channel Map Command (v4.0) (LE)
constexpr OpCode kLEReadChannelMap = LEControllerCommandOpCode(0x0015);

struct LEReadChannelMapCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

struct LEReadChannelMapReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // This parameter contains 37 1-bit fields (only the lower 37-bits of the
  // 5-octet value are meaningful).
  //
  // The nth such field (in the range 0 to 36) contains the value for the link
  // layer channel index n.
  //
  // Channel n is bad = 0. Channel n is unknown = 1.
  //
  // The most significant bits are reserved and shall be set to 0 for future
  // use.
  //
  // At least one channel shall be marked as unknown.
  uint8_t channel_map[5];
} __attribute__((packed));

// ===========================================
// LE Read Remote Features Command (v4.0) (LE)
constexpr OpCode kLEReadRemoteFeatures = LEControllerCommandOpCode(0x0016);

struct LEReadRemoteFeaturesCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// Note on ReturnParams: A Command Complete event is not sent by the Controller
// to indicate that this command has been completed. Instead, the LE Read Remote
// Features Complete event indicates that this command has been completed.

// ==============================
// LE Encrypt Command (v4.0) (LE)
constexpr OpCode kLEEncrypt = LEControllerCommandOpCode(0x0017);

struct LEEncryptCommandParams {
  // 128 bit key for the encryption of the data given in the command.
  UInt128 key;

  // 128 bit data block that is requested to be encrypted.
  uint8_t plaintext_data[16];
} __attribute__((packed));

struct LEEncryptReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // 128 bit encrypted data block.
  uint8_t encrypted_data[16];
} __attribute__((packed));

// ===========================
// LE Rand Command (v4.0) (LE)
constexpr OpCode kLERand = LEControllerCommandOpCode(0x0018);

struct LERandReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Random Number
  uint64_t random_number;
} __attribute__((packed));

// =======================================
// LE Start Encryption Command (v4.0) (LE)
constexpr OpCode kLEStartEncryption = LEControllerCommandOpCode(0x0019);

// NOTE on Return Params: A Command Complete event is not sent by the Controller
// to indicate that this command has been completed. Instead, the Encryption
// Change or Encryption Key Refresh Complete events indicate that this command
// has been completed.

// ==================================================
// LE Long Term Key Request Reply Command (v4.0) (LE)
constexpr OpCode kLELongTermKeyRequestReply = LEControllerCommandOpCode(0x001A);

struct LELongTermKeyRequestReplyCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // 128-bit long term key for the current connection.
  UInt128 long_term_key;
} __attribute__((packed));

struct LELongTermKeyRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// ===========================================================
// LE Long Term Key Request Negative Reply Command (v4.0) (LE)
constexpr OpCode kLELongTermKeyRequestNegativeReply =
    LEControllerCommandOpCode(0x001B);

struct LELongTermKeyRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// ============================================
// LE Read Supported States Command (v4.0) (LE)
constexpr OpCode kLEReadSupportedStates = LEControllerCommandOpCode(0x001C);

struct LEReadSupportedStatesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit-mask of supported state or state combinations. See Core Spec v4.2,
  // Volume 2, Part E, Section 7.8.27 "LE Read Supported States Command".
  uint64_t le_states;
} __attribute__((packed));

// ====================================
// LE Receiver Test Command (v4.0) (LE)
constexpr OpCode kLEReceiverTest = LEControllerCommandOpCode(0x001D);

struct LEReceiverTestCommandParams {
  // N = (F - 2402) / 2
  // Range: 0x00 - 0x27. Frequency Range : 2402 MHz to 2480 MHz.
  uint8_t rx_channel;
} __attribute__((packed));

// ======================================
// LE Transmitter Test Command (v4.0) (LE)
constexpr OpCode kLETransmitterTest = LEControllerCommandOpCode(0x001E);

struct LETransmitterTestCommandParams {
  // N = (F - 2402) / 2
  // Range: 0x00 - 0x27. Frequency Range : 2402 MHz to 2480 MHz.
  uint8_t tx_channel;

  // Length in bytes of payload data in each packet
  uint8_t length_of_test_data;

  // The packet payload sequence. See Core Spec 5.0, Vol 2, Part E,
  // Section 7.8.29 for a description of possible values.
  uint8_t packet_payload;
} __attribute__((packed));

// ===============================
// LE Test End Command (v4.0) (LE)
constexpr OpCode kLETestEnd = LEControllerCommandOpCode(0x001F);

struct LETestEndReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Number of packets received
  uint16_t number_of_packets;
} __attribute__((packed));

// ================================================================
// LE Remote Connection Parameter Request Reply Command (v4.1) (LE)
constexpr OpCode kLERemoteConnectionParameterRequestReply =
    LEControllerCommandOpCode(0x0020);

struct LERemoteConnectionParameterRequestReplyCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Range: see kLEConnectionInterval[Min|Max] in hci_constants.h
  // Time: N * 1.25 ms
  // Time Range: 7.5 ms to 4 s.
  uint16_t conn_interval_min;
  uint16_t conn_interval_max;

  // Range: 0x0000 to kLEConnectionLatencyMax in hci_constants.h
  uint16_t conn_latency;

  // Range: see kLEConnectionSupervisionTimeout[Min|Max] in hci_constants.h
  // Time: N * 10 ms
  // Time Range: 100 ms to 32 s
  uint16_t supervision_timeout;

  // Range: 0x0000 - 0xFFFF
  // Time: N * 0x625 ms
  uint16_t minimum_ce_length;
  uint16_t maximum_ce_length;
} __attribute__((packed));

struct LERemoteConnectionParameterRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// =========================================================================
// LE Remote Connection Parameter Request Negative Reply Command (v4.1) (LE)
constexpr OpCode kLERemoteConnectionParameterRequestNegativeReply =
    LEControllerCommandOpCode(0x0021);

struct LERemoteConnectionParamReqNegativeReplyCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Reason that the connection parameter request was rejected.
  StatusCode reason;
} __attribute__((packed));

struct LERemoteConnectionParamReqNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// ======================================
// LE Set Data Length Command (v4.2) (LE)
constexpr OpCode kLESetDataLength = LEControllerCommandOpCode(0x0022);

struct LESetDataLengthCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t tx_time;
} __attribute__((packed));

struct LESetDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

// =========================================================
// LE Read Suggested Default Data Length Command (v4.2) (LE)
constexpr OpCode kLEReadSuggestedDefaultDataLength =
    LEControllerCommandOpCode(0x0023);

struct LEReadSuggestedDefaultDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_time;
} __attribute__((packed));

// ==========================================================
// LE Write Suggested Default Data Length Command (v4.2) (LE)
constexpr OpCode kLEWriteSuggestedDefaultDataLength =
    LEControllerCommandOpCode(0x0024);

struct LEWriteSuggestedDefaultDataLengthCommandParams {
  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_time;
} __attribute__((packed));

// ==================================================
// LE Read Local P-256 Public Key Command (v4.2) (LE)
constexpr OpCode kLEReadLocalP256PublicKey = LEControllerCommandOpCode(0x0025);

// NOTE on ReturnParams: When the Controller receives the
// LE_Read_Local_P-256_Public_Key command, the Controller shall send the Command
// Status event to the Host. When the local P-256 public key generation
// finishes, an LE Read Local P-256 Public Key Complete event shall be
// generated.
//
// No Command Complete event is sent by the Controller to indicate that this
// command has been completed.

// ======================================
// LE Generate DH Key Command (v4.2) (LE)
constexpr OpCode kLEGenerateDHKey = LEControllerCommandOpCode(0x0026);

struct LEGenerateDHKeyCommandParams {
  // The remote P-256 public key:
  //   X, Y format
  //   Octets 31-0: X co-ordinate
  //   Octets 63-32: Y co-ordinate Little Endian Format
  uint8_t remote_p256_public_key[64];
} __attribute__((packed));

// NOTE on ReturnParams: When the Controller receives the LE_Generate_DHKey
// command, the Controller shall send the Command Status event to the Host. When
// the DHKey generation finishes, an LE DHKey Generation Complete event shall be
// generated.
//
// No Command Complete event is sent by the Controller to indicate that this
// command has been completed.

// ===================================================
// LE Add Device To Resolving List Command (v4.2) (LE)
constexpr OpCode kLEAddDeviceToResolvingList =
    LEControllerCommandOpCode(0x0027);

struct LEAddDeviceToResolvingListCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;

  // IRK (Identity Resolving Key) of the peer device
  UInt128 peer_irk;

  // IRK (Identity Resolving Key) of the local device
  UInt128 local_irk;
} __attribute__((packed));

// ========================================================
// LE Remove Device From Resolving List Command (v4.2) (LE)
constexpr OpCode kLERemoveDeviceFromResolvingList =
    LEControllerCommandOpCode(0x0028);

struct LERemoveDeviceFromResolvingListCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;
} __attribute__((packed));

// ===========================================
// LE Clear Resolving List Command (v4.2) (LE)
constexpr OpCode kLEClearResolvingList = LEControllerCommandOpCode(0x0029);

// ===============================================
// LE Read Resolving List Size Command (v4.2) (LE)
constexpr OpCode kLEReadResolvingListSize = LEControllerCommandOpCode(0x002A);

struct LEReadResolvingListReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Number of address translation entries in the resolving list.
  uint8_t resolving_list_size;
} __attribute__((packed));

// ===================================================
// LE Read Peer Resolvable Address Command (v4.2) (LE)
constexpr OpCode kLEReadPeerResolvableAddress =
    LEControllerCommandOpCode(0x002B);

struct LEReadPeerResolvableAddressCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device.
  DeviceAddressBytes peer_identity_address;
} __attribute__((packed));

struct LEReadPeerResolvableAddressReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Resolvable Private Address being used by the peer device.
  DeviceAddressBytes peer_resolvable_address;
} __attribute__((packed));

// ====================================================
// LE Read Local Resolvable Address Command (v4.2) (LE)
constexpr OpCode kLEReadLocalResolvableAddress =
    LEControllerCommandOpCode(0x002C);

struct LEReadLocalResolvableAddressCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;
} __attribute__((packed));

struct LEReadLocalResolvableAddressReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Resolvable Private Address being used by the local device.
  DeviceAddressBytes local_resolvable_address;
} __attribute__((packed));

// ====================================================
// LE Set Address Resolution Enable Command (v4.2) (LE)
constexpr OpCode kLESetAddressResolutionEnable =
    LEControllerCommandOpCode(0x002D);

struct LESetAddressResolutionEnableCommandParams {
  GenericEnableParam address_resolution_enable;
} __attribute__((packed));

// =============================================================
// LE Set Resolvable Private Address Timeout Command (v4.2) (LE)
constexpr OpCode kLESetResolvablePrivateAddressTimeout =
    LEControllerCommandOpCode(0x002E);

struct LESetResolvablePrivateAddressTimeoutCommandParams {
  // Range: See kLERPATimeout[Min|Max] in hci_constants.h
  // Default: See kLERPATimeoutDefault in hci_constants.h
  uint16_t rpa_timeout;
} __attribute__((packed));

// ===============================================
// LE Read Maximum Data Length Command (v4.2) (LE)
constexpr OpCode kLEReadMaximumDataLength = LEControllerCommandOpCode(0x002F);

struct LEReadMaximumDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t supported_max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t supported_max_tx_time;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t supported_max_rx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t supported_max_rx_time;
} __attribute__((packed));

// ===============================
// LE Read PHY Command (v5.0) (LE)
constexpr OpCode kLEReadPHY = LEControllerCommandOpCode(0x0030);

struct LEReadPHYCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __attribute__((packed));

struct LEReadPHYReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The transmitter PHY.
  LEPHY tx_phy;

  // The receiver PHY.
  LEPHY rx_phy;
} __attribute__((packed));

// ======================================
// LE Set Default PHY Command (v5.0) (LE)
constexpr OpCode kLESetDefaultPHY = LEControllerCommandOpCode(0x0031);

struct LESetDefaultPHYCommandParams {
  // See the kLEAllPHYSBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t all_phys;

  // See the kLEPHYBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t tx_phys;

  // See the kLEPHYBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t rx_phys;
} __attribute__((packed));

// ==============================
// LE Set PHY Command (v5.0) (LE)
constexpr OpCode kLESetPHY = LEControllerCommandOpCode(0x0032);

struct LESetPHYCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // See the kLEAllPHYSBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t all_phys;

  // See the kLEPHYBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t tx_phys;

  // See the kLEPHYBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t rx_phys;

  LEPHYOptions phy_options;
} __attribute__((packed));

// NOTE on ReturnParams: A Command Complete event is not sent by the Controller
// to indicate that this command has been completed. Instead, the LE PHY Update
// Complete event indicates that this command has been completed. The LE PHY
// Update Complete event may also be issued autonomously by the Link Layer.

// =============================================
// LE Enhanced Receiver Test Command (v5.0) (LE)
constexpr OpCode kLEEnhancedReceiverText = LEControllerCommandOpCode(0x0033);

struct LEEnhancedReceiverTestCommandParams {
  // N = (F - 2402) / 2
  // Range: 0x00 - 0x27. Frequency Range : 2402 MHz to 2480 MHz.
  uint8_t rx_channel;

  // Receiver PHY.
  LEPHY phy;

  // Transmitter modulation index that should be assumed.
  LETestModulationIndex modulation_index;
} __attribute__((packed));

// ================================================
// LE Enhanced Transmitter Test Command (v5.0) (LE)
constexpr OpCode kLEEnhancedTransmitterTest = LEControllerCommandOpCode(0x0034);

struct LEEnhancedTransmitterTestCommandParams {
  // N = (F - 2402) / 2
  // Range: 0x00 - 0x27. Frequency Range : 2402 MHz to 2480 MHz.
  uint8_t tx_channel;

  // Length in bytes of payload data in each packet
  uint8_t length_of_test_data;

  // The packet payload sequence. See Core Spec 5.0, Vol 2, Part E,
  // Section 7.8.51 for a description of possible values.
  uint8_t packet_payload;

  // Transmitter PHY.
  LEPHY phy;
} __attribute__((packed));

// =========================================================
// LE Set Advertising Set Random Address Command (v5.0) (LE)
constexpr OpCode kLESetAdvertisingSetRandomAddress =
    LEControllerCommandOpCode(0x0035);

// ==========================================================
// LE Set Extended Advertising Parameters Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingParameters =
    LEControllerCommandOpCode(0x0036);

struct LESetExtendedAdvertisingParametersReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  int8_t selected_tx_power;
} __attribute__((packed));

// ====================================================
// LE Set Extended Advertising Data Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingData =
    LEControllerCommandOpCode(0x0037);

// ======================================================
// LE Set Extended Scan Response Data Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanResponseData =
    LEControllerCommandOpCode(0x0038);

// ======================================================
// LE Set Extended Advertising Enable Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingEnable =
    LEControllerCommandOpCode(0x0039);

// ===========================================================
// LE Read Maximum Advertising Data Length Command (v5.0) (LE)
constexpr OpCode kLEReadMaxAdvertisingDataLength =
    LEControllerCommandOpCode(0x003A);

struct LEReadMaxAdvertisingDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t max_adv_data_length;
} __attribute__((packed));

// ================================================================
// LE Read Number of Supported Advertising Sets Command (v5.0) (LE)
constexpr OpCode kLEReadNumSupportedAdvertisingSets =
    LEControllerCommandOpCode(0x003B);

struct LEReadNumSupportedAdvertisingSetsReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint8_t num_supported_adv_sets;
} __attribute__((packed));

// =============================================
// LE Remove Advertising Set Command (v5.0) (LE)
constexpr OpCode kLERemoveAdvertisingSet = LEControllerCommandOpCode(0x003C);

struct LERemoveAdvertisingSetCommandParams {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __attribute__((packed));

// =============================================
// LE Clear Advertising Sets Command (v5.0) (LE)
constexpr OpCode kLEClearAdvertisingSets = LEControllerCommandOpCode(0x003D);

// ==========================================================
// LE Set Periodic Advertising Parameters Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingParameters =
    LEControllerCommandOpCode(0x003E);

struct LESetPeriodicAdvertisingParametersCommandParams {
  // Identifies the advertising set whose periodic advertising parameters are
  // being configured.
  AdvertisingHandle adv_handle;

  // Range: See kLEPeriodicAdvertisingInterval[Min|Max] in hci_constants.h
  // Time = N * 1.25 ms
  // Time Range: 7.5ms to 81.91875 s
  uint16_t periodic_adv_interval_min;
  uint16_t periodic_adv_interval_max;

  // See the kLEPeriodicAdvPropBit* constants in hci_constants.h for possible
  // bit values.
  uint16_t periodic_adv_properties;
} __attribute__((packed));

// ====================================================
// LE Set Periodic Advertising Data Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingData =
    LEControllerCommandOpCode(0x003F);

struct LESetPeriodicAdvertisingDataCommandParams {
  LESetPeriodicAdvertisingDataCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LESetPeriodicAdvertisingDataCommandParams);

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // See hci_constants.h for possible values.
  // LESetExtendedAdvDataOp::kUnchangedData is excluded for this command.
  LESetExtendedAdvDataOp operation;

  // Length of the advertising data included in this command packet, up to
  // kMaxLEExtendedAdvertisingDataLength bytes.
  uint8_t adv_data_length;

  // Variable length advertising data.
  uint8_t adv_data[];
} __attribute__((packed));

// ======================================================
// LE Set Periodic Advertising Enable Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingEnable =
    LEControllerCommandOpCode(0x0040);

struct LESetPeriodicAdvertisingEnableCommandParams {
  // Enable or Disable periodic advertising.
  GenericEnableParam enable;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __attribute__((packed));

// ===================================================
// LE Set Extended Scan Parameters Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanParameters =
    LEControllerCommandOpCode(0x0041);

// ===============================================
// LE Set Extended Scan Enable Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanEnable = LEControllerCommandOpCode(0x0042);

// =================================================
// LE Extended Create Connection Command (v5.0) (LE)
constexpr OpCode kLEExtendedCreateConnection =
    LEControllerCommandOpCode(0x0043);

// =======================================================
// LE Periodic Advertising Create Sync Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingCreateSync =
    LEControllerCommandOpCode(0x0044);

// NOTE on ReturnParams: No Command Complete event is sent by the Controller to
// indicate that this command has been completed. Instead, the LE Periodic
// Advertising Sync Established event indicates that this command has been
// completed.

// ==============================================================
// LE Periodic Advertising Create Sync Cancel Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingCreateSyncCancel =
    LEControllerCommandOpCode(0x0045);

// ==========================================================
// LE Periodic Advertising Terminate Sync Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingTerminateSync =
    LEControllerCommandOpCode(0x0046);

// =============================================================
// LE Add Device To Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLEAddDeviceToPeriodicAdvertiserList =
    LEControllerCommandOpCode(0x0047);

// ==================================================================
// LE Remove Device From Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLERemoveDeviceFromPeriodicAdvertiserList =
    LEControllerCommandOpCode(0x0048);

// =====================================================
// LE Clear Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLEClearPeriodicAdvertiserList =
    LEControllerCommandOpCode(0x0049);

// =========================================================
// LE Read Periodic Advertiser List Size Command (v5.0) (LE)
constexpr OpCode kLEReadPeriodicAdvertiserListSize =
    LEControllerCommandOpCode(0x004A);

struct LEReadPeriodicAdvertiserListSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Total number of Periodic Advertiser list entries that can be stored in the
  // Controller.
  uint8_t periodic_advertiser_list_size;
} __attribute__((packed));

// ==========================================
// LE Read Transmit Power Command (v5.0) (LE)
constexpr OpCode kLEReadTransmitPower = LEControllerCommandOpCode(0x004B);

struct LEReadTransmitPowerReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Range: -127 <= N <= +126
  // Units: dBm
  int8_t min_tx_power;
  int8_t max_tx_power;
} __attribute__((packed));

// ================================================
// LE Read RF Path Compensation Command (v5.0) (LE)
constexpr OpCode kLEReadRFPathCompensation = LEControllerCommandOpCode(0x004C);

struct LEReadRFPathCompensationReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The RF Path Compensation Values parameters used in the Tx Power Level and
  // RSSI calculation.
  //   Range: -128.0 dB (0xFB00) ≤ N ≤ 128.0 dB (0x0500)
  //   Units: 0.1 dB
  int16_t rf_tx_path_comp_value;
  int16_t rf_rx_path_comp_value;
} __attribute__((packed));

// =================================================
// LE Write RF Path Compensation Command (v5.0) (LE)
constexpr OpCode kLEWriteRFPathCompensation = LEControllerCommandOpCode(0x004D);

// =======================================
// LE Set Privacy Mode Command (v5.0) (LE)
constexpr OpCode kLESetPrivacyMode = LEControllerCommandOpCode(0x004E);

// =======================================
// LE Set Host Feature Command (v5.2) (LE)
constexpr OpCode kLESetHostFeature = LEControllerCommandOpCode(0x0074);

// ======= Vendor Command =======
// The OGF of 0x3F is reserved for vendor-specific debug commands (see Core Spec
// v5.0, Vol 2, Part E, Section 5.4.1).
constexpr uint8_t kVendorOGF = 0x3F;
constexpr OpCode VendorOpCode(const uint16_t ocf) {
  return DefineOpCode(kVendorOGF, ocf);
}

}  // namespace bt::hci_spec
