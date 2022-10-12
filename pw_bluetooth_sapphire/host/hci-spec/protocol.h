// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_PROTOCOL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_PROTOCOL_H_

#include <zircon/compiler.h>

#include <array>
#include <cstdint>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_class.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

#include <src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h>

// This file contains general opcode/number and static packet definitions for
// the Bluetooth Host-Controller Interface. Each packet payload structure
// contains parameter descriptions based on their respective documentation in
// the Bluetooth Core Specification version 5.0
//
// NOTE: Avoid casting raw buffer pointers to the packet payload structure types
// below; use as template parameter to PacketView::payload(),
// MutableBufferView::mutable_payload(), or CommandPacket::mutable_payload()
// instead. Take extra care when accessing flexible array members.

namespace bt::hci_spec {

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
} __PACKED;

struct EventHeader {
  uint8_t event_code;
  uint8_t parameter_total_size;
} __PACKED;

struct ACLDataHeader {
  // The first 16-bits contain the following fields, in order:
  //   - 12-bits: Connection Handle
  //   - 2-bits: Packet Boundary Flags
  //   - 2-bits: Broadcast Flags
  uint16_t handle_and_flags;

  // Length of data following the header.
  uint16_t data_total_length;
} __PACKED;

struct SynchronousDataHeader {
  // The first 16-bits contain the following fields, in order:
  // - 12-bits: Connection Handle
  // - 2-bits: Packet Status Flag
  // - 2-bits: RFU
  uint16_t handle_and_flags;

  // Length of the data following the header.
  uint8_t data_total_length;
} __PACKED;

// Generic return parameter struct for commands that only return a status. This
// can also be used to check the status of HCI commands with more complex return
// parameters.
struct SimpleReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
} __PACKED;

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

struct CreateConnectionCommandParams {
  // BD_ADDR of the device to be connected
  DeviceAddressBytes bd_addr;

  // Mask of allowable packet types. See PacketTypeBits in hci_constants.h for
  // values.
  PacketTypeType packet_type;

  // The Page Scan Repetition Mode of the remote device as retrieved by Inquiry.
  PageScanRepetitionMode page_scan_repetition_mode;

  // Reserved, must be set to 0.
  uint8_t reserved;

  // Clock Offset.  The lower 15 bits are set to the clock offset as retrieved
  // by an Inquiry. The highest bit is set to 1 if the rest of this parameter
  // is valid.
  uint16_t clock_offset;

  // Allow Role Switch.
  // Allowed values:
  //  0x00 - No role switch allowed, this device will be the central
  //  0x01 - Role switch allowed, this device may become peripheral during
  //  connection setup
  uint8_t allow_role_switch;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Connection Complete event will indicate that this command has been
// completed.

// =======================================
// Disconnect Command (v1.1) (BR/EDR & LE)
constexpr OpCode kDisconnect = LinkControlOpCode(0x0006);

struct DisconnectCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Reason for the disconnect. See Section 7.1.6 for allowed status codes.
  StatusCode reason;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Disconnection Complete event will indicate that this command has been
// completed.

// ========================================
// Create Connection Cancel (v1.1) (BR/EDR)
constexpr OpCode kCreateConnectionCancel = LinkControlOpCode(0x0008);

struct CreateConnectionCancelCommandParams {
  // BD_ADDR of the Create Connection Command Request
  DeviceAddressBytes bd_addr;
} __PACKED;

struct CreateConnectionCancelReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the Create Connection Command Request
  DeviceAddressBytes bd_addr;
} __PACKED;

// =========================================
// Accept Connection Request (v1.1) (BR/EDR)
constexpr OpCode kAcceptConnectionRequest = LinkControlOpCode(0x0009);

struct AcceptConnectionRequestCommandParams {
  // BD_ADDR of the device to be connected
  DeviceAddressBytes bd_addr;

  // Role. Allowable values:
  //  - kCentral - Host will become the central (Link Central will role switch)
  //  - kPeripheral - Host will remain the peripheral.
  ConnectionRole role;
} __PACKED;

// =========================================
// Reject Connection Request (v1.1) (BR/EDR)
constexpr OpCode kRejectConnectionRequest = LinkControlOpCode(0x000A);

struct RejectConnectionRequestCommandParams {
  // BD_ADDR of the device to reject the connection from
  DeviceAddressBytes bd_addr;

  // Reason.
  // Must be one of kConnectionRejected* from Status in hci_constants.h
  StatusCode reason;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Connection Complete event will indicate that this command has been completed.

// ==============================================
// Link Key Request Reply Command (v1.1) (BR/EDR)
constexpr OpCode kLinkKeyRequestReply = LinkControlOpCode(0x000B);

constexpr size_t kBrEdrLinkKeySize = 16;

struct LinkKeyRequestReplyCommandParams {
  // BD_ADDR of the peer device the link key is for.
  DeviceAddressBytes bd_addr;

  // Link key to use for the connection with the peer device.
  uint8_t link_key[kBrEdrLinkKeySize];
} __PACKED;

struct LinkKeyRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the device whose Link Key Request was fulfilled.
  DeviceAddressBytes bd_addr;
} __PACKED;

// =======================================================
// Link Key Request Negative Reply Command (v1.1) (BR/EDR)
constexpr OpCode kLinkKeyRequestNegativeReply = LinkControlOpCode(0x000C);

struct LinkKeyRequestNegativeReplyCommandParams {
  // BD_ADDR of the peer device that the host does not have a link key for.
  DeviceAddressBytes bd_addr;
} __PACKED;

struct LinkKeyRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the device whose Link Key Request was denied.
  DeviceAddressBytes bd_addr;
} __PACKED;

// ================================================
// Authentication Requested Command (v1.1) (BR/EDR)
constexpr OpCode kAuthenticationRequested = LinkControlOpCode(0x0011);

struct AuthenticationRequestedCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  // Must be the handle of a connected ACL-U logical link.
  ConnectionHandle connection_handle;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Authentication Complete event will indicate that this command has been
// completed.

// =================================================
// Set Connection Encryption Command (v1.1) (BR/EDR)
constexpr OpCode kSetConnectionEncryption = LinkControlOpCode(0x0013);

struct SetConnectionEncryptionCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  // Must be the handle of a connected ACL-U logical link.
  ConnectionHandle connection_handle;

  // Whether link level encryption should be turned on or off.
  GenericEnableParam encryption_enable;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Encryption Change event will indicate that this command has been completed.

// ============================================================
// Read Remote Name Request Command (v1.1) (BR/EDR)
constexpr OpCode kRemoteNameRequest = LinkControlOpCode(0x0019);

struct RemoteNameRequestCommandParams {
  // Address of the device whose name is to be requested.
  DeviceAddressBytes bd_addr;

  // Page Scan Repetition Mode of the device, obtained by Inquiry.
  PageScanRepetitionMode page_scan_repetition_mode;

  // Reserved and must be 0x00
  uint8_t reserved;

  // Clock offset.  The lower 15 bits of this represent bits 14-2
  // of CLKNPeripheral-CLK, and the highest bit is set when the other
  // bits are valid.
  uint16_t clock_offset;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Remote Name Request Complete event will indicate that this command has been
// completed.

// ======================================================
// Read Remote Supported Features Command (v1.1) (BR/EDR)
constexpr OpCode kReadRemoteSupportedFeatures = LinkControlOpCode(0x001B);

struct ReadRemoteSupportedFeaturesCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  // Must be the handle of a connected ACL-U logical link.
  ConnectionHandle connection_handle;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Read Remote Supported Features Complete event will indicate that this
// command has been completed.

// =====================================================
// Read Remote Extended Features Command (v1.2) (BR/EDR)
constexpr OpCode kReadRemoteExtendedFeatures = LinkControlOpCode(0x001C);

struct ReadRemoteExtendedFeaturesCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  // Must be the handle of a connected ACL-U logical link.
  ConnectionHandle connection_handle;

  // Page of features to read.
  // Values:
  //  - 0x00 standard features as if requested by Read Remote Supported Features
  //  - 0x01-0xFF the corresponding features page see Vol 2, Part C, Sec 3.3)
  uint8_t page_number;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Read Remote Extended Features Complete event will indicate that this
// command has been completed.

// ============================================================
// Read Remote Version Information Command (v1.1) (BR/EDR & LE)
constexpr OpCode kReadRemoteVersionInfo = LinkControlOpCode(0x001D);

struct ReadRemoteVersionInfoCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event will be sent by the
// Controller to indicate that this command has been completed. Instead, the
// Read Remote Version Information Complete event will indicate that this
// command has been completed.

// =============================================
// Reject Synchronous Connection Command (BR/EDR)
constexpr OpCode kRejectSynchronousConnectionRequest = LinkControlOpCode(0x002A);

struct RejectSynchronousConnectionRequestCommandParams {
  // Address of the remote device that sent the request.
  DeviceAddressBytes bd_addr;

  // Reason the connection request was rejected.
  StatusCode reason;
} __PACKED;

// =========================================================
// IO Capability Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kIOCapabilityRequestReply = LinkControlOpCode(0x002B);

struct IOCapabilityRequestReplyCommandParams {
  // The BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // The IOCapabilities of this device
  IOCapability io_capability;

  // Whether there is OOB Data Present, and what type. Valid values:
  // 0x00 - OOB authentication data not present
  // 0x01 - P-192 OOB authentication data from remote device present
  // 0x02 - P-256 OOB authentication data from remote device present
  // 0x03 - P-192 and P-256 OOB authentication data from remote device present
  uint8_t oob_data_present;

  // Authentication Requirements.
  // See enum class AuthRequirements in hci_constants.h
  AuthRequirements auth_requirements;
} __PACKED;

struct IOCapabilityRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __PACKED;

// =============================================================
// User Confirmation Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserConfirmationRequestReply = LinkControlOpCode(0x002C);

struct UserConfirmationRequestReplyCommandParams {
  // The BD_ADDR of the remote device involved in the simple pairing process.
  DeviceAddressBytes bd_addr;
} __PACKED;

// ======================================================================
// User Confirmation Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserConfirmationRequestNegativeReply = LinkControlOpCode(0x002D);

struct UserConfirmationRequestNegativeReplyCommandParams {
  // The BD_ADDR of the remote device involved in the simple pairing process.
  DeviceAddressBytes bd_addr;
} __PACKED;

// ========================================================
// User Passkey Request Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserPasskeyRequestReply = LinkControlOpCode(0x002E);

struct UserPasskeyRequestReplyCommandParams {
  // The BD_ADDR of the remote device involved in the simple pairing process.
  DeviceAddressBytes bd_addr;

  // Numeric value (passkey) entered by user. Valid values are 0 - 999999.
  uint32_t numeric_value;
} __PACKED;

// =================================================================
// User Passkey Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kUserPasskeyRequestNegativeReply = LinkControlOpCode(0x002F);

struct UserPasskeyRequestNegativeReplyCommandParams {
  // The BD_ADDR of the remote device involved in the simple pairing process.
  DeviceAddressBytes bd_addr;
} __PACKED;

// ==================================================================
// IO Capability Request Negative Reply Command (v2.1 + EDR) (BR/EDR)
constexpr OpCode kIOCapabilityRequestNegativeReply = LinkControlOpCode(0x0034);

struct IOCapabilityRequestNegativeReplyCommandParams {
  // The BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // Reason that Simple Pairing was rejected. See 7.1.36 for valid error codes.
  StatusCode reason;
} __PACKED;

struct IOCapabilityRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // BD_ADDR of the remote device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __PACKED;

// ======================================================
// Enhanced Setup Synchronous Connection Command (BR/EDR)
constexpr OpCode kEnhancedSetupSynchronousConnection = LinkControlOpCode(0x003D);

// ===============================================================
// Enhanced Accept Synchronous Connection Request Command (BR/EDR)
constexpr OpCode kEnhancedAcceptSynchronousConnectionRequest = LinkControlOpCode(0x003E);

// ======= Controller & Baseband Commands =======
// Core Spec v5.0 Vol 2, Part E, Section 7.3
constexpr uint8_t kControllerAndBasebandOGF = 0x03;
constexpr OpCode ControllerAndBasebandOpCode(const uint16_t ocf) {
  return DefineOpCode(kControllerAndBasebandOGF, ocf);
}

// =============================
// Set Event Mask Command (v1.1)
constexpr OpCode kSetEventMask = ControllerAndBasebandOpCode(0x0001);

struct SetEventMaskCommandParams {
  // Bit mask used to control which HCI events are generated by the HCI for the
  // Host. See enum class EventMask in hci_constants.h
  uint64_t event_mask;
} __PACKED;

// ====================
// Reset Command (v1.1)
constexpr OpCode kReset = ControllerAndBasebandOpCode(0x0003);

// ========================================
// Write Local Name Command (v1.1) (BR/EDR)
constexpr OpCode kWriteLocalName = ControllerAndBasebandOpCode(0x0013);

struct WriteLocalNameCommandParams {
  // A UTF-8 encoded User Friendly Descriptive Name for the device.
  // If the name contained in the parameter is shorter than 248 octets, the end
  // of the name is indicated by a NULL octet (0x00), and the following octets
  // (to fill up 248 octets, which is the length of the parameter) do not have
  // valid values.
  uint8_t local_name[kMaxNameLength];
} __PACKED;

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
} __PACKED;

// ==========================================
// Write Page Timeout Command (v1.1) (BR/EDR)
constexpr OpCode kWritePageTimeout = ControllerAndBasebandOpCode(0x0018);

struct WritePageTimeoutCommandParams {
  // Page_Timeout, in time slices (0.625 ms)
  // Range: kMinPageTimeoutCommandParameterValue to kMaxPageTimeoutCommandParameterValue in
  // hci-spec/constants.h
  uint16_t page_timeout;
} __PACKED;

struct WritePageTimeoutReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
} __PACKED;

// ========================================
// Read Scan Enable Command (v1.1) (BR/EDR)
constexpr OpCode kReadScanEnable = ControllerAndBasebandOpCode(0x0019);

struct ReadScanEnableReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask of enabled scans. See enum class ScanEnableBit in hci_constants.h
  // for how to interpret this bitfield.
  ScanEnableType scan_enable;
} __PACKED;

// =========================================
// Write Scan Enable Command (v1.1) (BR/EDR)
constexpr OpCode kWriteScanEnable = ControllerAndBasebandOpCode(0x001A);

struct WriteScanEnableCommandParams {
  // Bit Mask of enabled scans. See enum class ScanEnableBit in hci_constants.h
  // for how to construct this bitfield.
  ScanEnableType scan_enable;
} __PACKED;

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
} __PACKED;

// ================================================
// Write Page Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kWritePageScanActivity = ControllerAndBasebandOpCode(0x001C);

struct WritePageScanActivityCommandParams {
  // Page_Scan_Interval, in time slices (0.625ms)
  // Valid Range: kPageScanIntervalMin - kPageScanIntervalMax in hci_constants.h
  uint16_t page_scan_interval;

  // Page_Scan_Window, in time slices
  // Valid Range: kPageScanWindowMin - kPageScanWindowMax in hci_constants.h
  uint16_t page_scan_window;
} __PACKED;

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
} __PACKED;

// ================================================
// Write Inquiry Scan Activity Command (v1.1) (BR/EDR)
constexpr OpCode kWriteInquiryScanActivity = ControllerAndBasebandOpCode(0x001E);

struct WriteInquiryScanActivityCommandParams {
  // Inquiry_Scan_Interval, in time slices (0.625ms)
  // Valid Range: kInquiryScanIntervalMin - kInquiryScanIntervalMax in
  // hci_constants.h
  uint16_t inquiry_scan_interval;

  // Inquiry_Scan_Window, in time slices
  // Valid Range: kInquiryScanWindowMin - kInquiryScanWindowMax in hci_constants.h
  uint16_t inquiry_scan_window;
} __PACKED;

// ============================================
// Read Class of Device Command (v1.1) (BR/EDR)
constexpr OpCode kReadClassOfDevice = ControllerAndBasebandOpCode(0x0023);

struct ReadClassOfDeviceReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  DeviceClass class_of_device;
} __PACKED;

// =============================================
// Write Class Of Device Command (v1.1) (BR/EDR)
constexpr OpCode kWriteClassOfDevice = ControllerAndBasebandOpCode(0x0024);

struct WriteClassOfDeviceCommandParams {
  DeviceClass class_of_device;
} __PACKED;

// =============================================
// Write Automatic Flush Timeout Command (v1.1) (BR/EDR)

constexpr OpCode kWriteAutomaticFlushTimeout = ControllerAndBasebandOpCode(0x0028);

struct WriteAutomaticFlushTimeoutCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The value for the Flush_Timeout configuration parameter (Core Spec v5.2, Vol 4, Part E,
  // Sec 6.19).
  // Range: 0x0000 to 0x07FF. 0x0000 indicated infinite flush timeout (no automatic
  // flush).
  // Time = flush_timeout * 0.625ms.
  // Time Range: 0.625ms to 1279.375ms.
  uint16_t flush_timeout;
} __PACKED;

// ===============================================================
// Read Transmit Transmit Power Level Command (v1.1) (BR/EDR & LE)
constexpr OpCode kReadTransmitPowerLevel = ControllerAndBasebandOpCode(0x002D);

struct ReadTransmitPowerLevelCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The type of transmit power level to read.
  ReadTransmitPowerType type;
} __PACKED;

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
} __PACKED;

// ===============================================================
// Write Synchonous Flow Control Enable Command (BR/EDR)
constexpr OpCode kWriteSynchronousFlowControlEnable = ControllerAndBasebandOpCode(0x002F);

struct WriteSynchronousFlowControlEnableParams {
  // If enabled, HCI_Number_Of_Completed_Packets events shall be sent from the controller
  // for synchronous connection handles.
  GenericEnableParam synchronous_flow_control_enable;
} __PACKED;

// ===================================
// Read Inquiry Scan Type (v1.2) (BR/EDR)
constexpr OpCode kReadInquiryScanType = ControllerAndBasebandOpCode(0x0042);

struct ReadInquiryScanTypeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class InquiryScanType in hci_constants.h for possible values.
  InquiryScanType inquiry_scan_type;
} __PACKED;

// ====================================
// Write Inquiry Scan Type (v1.2) (BR/EDR)
constexpr OpCode kWriteInquiryScanType = ControllerAndBasebandOpCode(0x0043);

struct WriteInquiryScanTypeCommandParams {
  // See enum class InquiryScanType in hci_constants.h for possible values.
  InquiryScanType inquiry_scan_type;
} __PACKED;

// =================================
// Read Inquiry Mode (v1.2) (BR/EDR)
constexpr OpCode kReadInquiryMode = ControllerAndBasebandOpCode(0x0044);

struct ReadInquiryModeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum InquiryMode in hci_constants.h
  InquiryMode inquiry_mode;
} __PACKED;

// ==================================
// Write Inquiry Mode (v1.2) (BR/EDR)
constexpr OpCode kWriteInquiryMode = ControllerAndBasebandOpCode(0x0045);

struct WriteInquiryModeCommandParams {
  // See enum InquiryMode in hci_constants.h
  InquiryMode inquiry_mode;
} __PACKED;

// ===================================
// Read Page Scan Type (v1.2) (BR/EDR)
constexpr OpCode kReadPageScanType = ControllerAndBasebandOpCode(0x0046);

struct ReadPageScanTypeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class PageScanType in hci_constants.h for possible values.
  PageScanType page_scan_type;
} __PACKED;

// ====================================
// Write Page Scan Type (v1.2) (BR/EDR)
constexpr OpCode kWritePageScanType = ControllerAndBasebandOpCode(0x0047);

struct WritePageScanTypeCommandParams {
  // See enum class PageScanType in hci_constants.h for possible values.
  PageScanType page_scan_type;
} __PACKED;

// =================================
// Write Extended Inquiry Response (v1.2) (BR/EDR)
constexpr OpCode kWriteExtendedInquiryResponse = ControllerAndBasebandOpCode(0x0052);

struct WriteExtendedInquiryResponseParams {
  // If FEC Encoding is required. (v1.2) (7.3.56)
  uint8_t fec_required;

  // Extended inquiry response data as defined in Vol 3, Part C, Sec 8
  uint8_t extended_inquiry_response[kExtendedInquiryResponseBytes];
} __PACKED;

// ==============================================
// Read Simple Pairing Mode (v2.1 + EDR) (BR/EDR)
constexpr OpCode kReadSimplePairingMode = ControllerAndBasebandOpCode(0x0055);

struct ReadSimplePairingModeReturnParams {
  // See enum StatusCode in hci_constants.h
  StatusCode status;

  // Simple pairing Mode.
  GenericEnableParam simple_pairing_mode;
} __PACKED;

// ===============================================
// Write Simple Pairing Mode (v2.1 + EDR) (BR/EDR)
constexpr OpCode kWriteSimplePairingMode = ControllerAndBasebandOpCode(0x0056);

struct WriteSimplePairingModeCommandParams {
  // Simple Pairing Mode.
  GenericEnableParam simple_pairing_mode;
} __PACKED;

// =========================================
// Set Event Mask Page 2 Command (v3.0 + HS)
constexpr OpCode kSetEventMaskPage2 = ControllerAndBasebandOpCode(0x0063);

struct SetEventMaskPage2CommandParams {
  // Bit mask used to control which HCI events are generated by the HCI for the
  // Host. See enum class EventMaskPage2 in hci_constants.h
  uint64_t event_mask;
} __PACKED;

// =========================================================
// Read Flow Control Mode Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kReadFlowControlMode = ControllerAndBasebandOpCode(0x0066);

struct ReadFlowControlModeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class FlowControlMode in hci_constants.h for possible values.
  FlowControlMode flow_control_mode;
} __PACKED;

// ==========================================================
// Write Flow Control Mode Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kWriteFlowControlMode = ControllerAndBasebandOpCode(0x0067);

struct WriteFlowControlModeCommandParams {
  // See enum class FlowControlMode in hci_constants.h for possible values.
  FlowControlMode flow_control_mode;
} __PACKED;

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
} __PACKED;

// =============================================
// Write LE Host Support Command (v4.0) (BR/EDR)
constexpr OpCode kWriteLEHostSupport = ControllerAndBasebandOpCode(0x006D);

struct WriteLEHostSupportCommandParams {
  GenericEnableParam le_supported_host;

  // Core Spec v5.0, Vol 2, Part E, Section 6.35: This value is set to "disabled
  // (0x00)" by default and "shall be ignored".
  uint8_t simultaneous_le_host;
} __PACKED;

// ===============================================================
// Read Authenticated Payload Timeout Command (v4.1) (BR/EDR & LE)
constexpr OpCode kReadAuthenticatedPayloadTimeout = ControllerAndBasebandOpCode(0x007B);

struct ReadAuthenticatedPayloadTimeoutCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

// ================================================================
// Write Authenticated Payload Timeout Command (v4.1) (BR/EDR & LE)
constexpr OpCode kWriteAuthenticatedPayloadTimeout = ControllerAndBasebandOpCode(0x007C);

struct WriteAuthenticatedPayloadTimeoutCommandParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Default = 0x0BB8 (30 s)
  // Range: 0x0001 to 0xFFFF
  // Time = N * 10 ms
  // Time Range: 10 ms to 655,350 ms
  uint16_t authenticated_payload_timeout;
} __PACKED;

struct WriteAuthenticatedPayloadTimeoutReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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

  // HCI version (see enum class HCIVersion in hci_constants.h)
  HCIVersion hci_version;

  uint16_t hci_revision;
  uint8_t lmp_pal_version;
  uint16_t manufacturer_name;
  uint16_t lmp_pal_subversion;
} __PACKED;

// ============================================
// Read Local Supported Commands Command (v1.2)
constexpr OpCode kReadLocalSupportedCommands = InformationalParamsOpCode(0x0002);

struct ReadLocalSupportedCommandsReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // See enum class SupportedCommand in hci_constants.h for how to interpret
  // this bitfield.
  uint8_t supported_commands[64];
} __PACKED;

// ============================================
// Read Local Supported Features Command (v1.1)
constexpr OpCode kReadLocalSupportedFeatures = InformationalParamsOpCode(0x0003);

struct ReadLocalSupportedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask List of LMP features. See enum class LMPFeature in hci_constants.h
  // for how to interpret this bitfield.
  uint64_t lmp_features;
} __PACKED;

// ====================================================
// Read Local Extended Features Command (v1.2) (BR/EDR)
constexpr OpCode kReadLocalExtendedFeatures = InformationalParamsOpCode(0x0004);

struct ReadLocalExtendedFeaturesCommandParams {
  // - 0x00: Requests the normal LMP features as returned by
  //   Read_Local_Supported_Features.
  //
  // - 0x01-0xFF: Return the corresponding page of features.
  uint8_t page_number;
} __PACKED;

struct ReadLocalExtendedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  uint8_t page_number;
  uint8_t maximum_page_number;
  uint64_t extended_lmp_features;
} __PACKED;

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
} __PACKED;

// ========================================
// Read BD_ADDR Command (v1.1) (BR/EDR, LE)
constexpr OpCode kReadBDADDR = InformationalParamsOpCode(0x0009);

struct ReadBDADDRReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  DeviceAddressBytes bd_addr;
} __PACKED;

// =======================================================
// Read Data Block Size Command (v3.0 + HS) (BR/EDR & AMP)
constexpr OpCode kReadDataBlockSize = InformationalParamsOpCode(0x000A);

struct ReadDataBlockSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t max_acl_data_packet_length;
  uint16_t data_block_length;
  uint16_t total_num_data_blocks;
} __PACKED;

// ======= Events =======
// Core Spec v5.0 Vol 2, Part E, Section 7.7

// Reserved for vendor-specific debug events
// (Vol 2, Part E, Section 5.4.4)
constexpr EventCode kVendorDebugEventCode = 0xFF;

struct VendorEventParams {
  VendorEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(VendorEventParams);

  // The event code for the vendor subevent.
  EventCode subevent_code;

  // Beginning of parameters that are specific to the vendor subevent.
  uint8_t subevent_parameters[];
} __PACKED;

// ======================================
// Inquiry Complete Event (v1.1) (BR/EDR)
constexpr EventCode kInquiryCompleteEventCode = 0x01;

using InquiryCompleteEventParams = SimpleReturnParams;

// ====================================
// Inquiry Result Event (v1.1) (BR/EDR)
constexpr EventCode kInquiryResultEventCode = 0x02;

struct InquiryResult {
  // The address for the device which responded.
  DeviceAddressBytes bd_addr;

  // The Page Scan Repetition Mode being used by the remote device.
  PageScanRepetitionMode page_scan_repetition_mode;

  // Reserved (no meaning as of v1.2)
  uint8_t page_scan_period_mode;

  // Reserved (no meaning as of v1.2)
  uint8_t page_scan_mode;

  // Class of device
  DeviceClass class_of_device;

  // Clock Offset
  // the 15 lower bits represent bits 16-2 of CLKNPeripheral-CLK
  // the most significant bit is reserved
  uint16_t clock_offset;
} __PACKED;

struct InquiryResultEventParams {
  InquiryResultEventParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(InquiryResultEventParams);

  // The number of responses included.
  uint8_t num_responses;

  InquiryResult responses[];
} __PACKED;

// =========================================
// Connection Complete Event (v1.1) (BR/EDR)
constexpr EventCode kConnectionCompleteEventCode = 0x03;

struct ConnectionCompleteEventParams {
  // See enum StatusCode in hci_constants.h
  StatusCode status;

  // Connection_handle (12 bits meaningful)
  // Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // The address of the connected device
  DeviceAddressBytes bd_addr;

  // See enum LinkType in hci_constants.h.
  LinkType link_type;

  // Whether Link level encryption is enabled
  // Valid values are 0x00 (not enabled) and 0x01 (enabled)
  uint8_t encryption_enabled;
} __PACKED;

// ========================================
// Connection Request Event (v1.1) (BR/EDR)
constexpr EventCode kConnectionRequestEventCode = 0x04;

struct ConnectionRequestEventParams {
  // The address of the device that's requesting the connection.
  DeviceAddressBytes bd_addr;

  // The Class of Device of the device which requests the connection.
  DeviceClass class_of_device;

  // See enum LinkType in hci_constants.h
  LinkType link_type;
} __PACKED;

// =================================================
// Disconnection Complete Event (v1.1) (BR/EDR & LE)
constexpr EventCode kDisconnectionCompleteEventCode = 0x05;

struct DisconnectionCompleteEventParams {
  // See enum StatusCode in hci_constants.h
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Reason for the disconnect.
  StatusCode reason;
} __PACKED;

// =============================================
// Authentication Complete Event (v1.1) (BR/EDR)
constexpr EventCode kAuthenticationCompleteEventCode = 0x06;

struct AuthenticationCompleteEventParams {
  // See enum StatusCode in hci_constants.h
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// ==================================================
// Remote Name Request Complete Event (v1.1) (BR/EDR)
constexpr EventCode kRemoteNameRequestCompleteEventCode = 0x07;

struct RemoteNameRequestCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Address of the device
  DeviceAddressBytes bd_addr;

  // Remote Name - UTF-8 encoded friendly name.
  // If the name is less than 248 characters, it is null terminated and
  // the remaining bytes are not valid.
  uint8_t remote_name[kMaxNameLength];
} __PACKED;

// ============================================
// Encryption Change Event (v1.1) (BR/EDR & LE)
constexpr EventCode kEncryptionChangeEventCode = 0x08;

struct EncryptionChangeEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Current Link Level Encryption status.
  EncryptionStatus encryption_enabled;
} __PACKED;

// =========================================================
// Change Connection Link Key Complete Event (v1.1) (BR/EDR)
constexpr EventCode kChangeConnectionLinkKeyCompleteEventCode = 0x09;

struct ChangeConnectionLinkKeyCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

// ===================================================================
// Read Remote Version Information Complete Event (v1.1) (BR/EDR & LE)
constexpr EventCode kReadRemoteVersionInfoCompleteEventCode = 0x0C;

struct ReadRemoteVersionInfoCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  HCIVersion lmp_version;
  // Manufacturer Name. Assigned by Bluetooth SIG. See Assigned Numbers
  // (https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers)
  uint16_t manufacturer_name;
  // See Spec 5.0 Vol 2, Part C, Sec 5.2. Defined by each company.
  uint16_t lmp_subversion;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// ===========================
// Hardware Error Event (v1.1)
constexpr EventCode kHardwareErrorEventCode = 0x10;

struct HardwareErrorEventParams {
  // These Hardware_Codes will be implementation-specific, and can be assigned
  // to indicate various hardware problems.
  uint8_t hardware_code;
} __PACKED;

// ========================================
// Role Change Event (BR/EDR) (v1.1)
constexpr EventCode kRoleChangeEventCode = 0x12;

struct RoleChangeEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The address of the device for which a role change has completed.
  DeviceAddressBytes bd_addr;

  // The new role for the specified address.
  ConnectionRole new_role;
} __PACKED;

// ========================================
// Number Of Completed Packets Event (v1.1)
constexpr EventCode kNumberOfCompletedPacketsEventCode = 0x13;

struct NumberOfCompletedPacketsEventData {
  uint16_t connection_handle;
  uint16_t hc_num_of_completed_packets;
} __PACKED;

struct NumberOfCompletedPacketsEventParams {
  NumberOfCompletedPacketsEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(NumberOfCompletedPacketsEventParams);

  uint8_t number_of_handles;
  NumberOfCompletedPacketsEventData data[];
} __PACKED;

// ======================================
// Link Key Request Event (v1.1) (BR/EDR)
constexpr EventCode kLinkKeyRequestEventCode = 0x17;

struct LinkKeyRequestParams {
  // The address for the device that a host-stored link key is being requested.
  DeviceAddressBytes bd_addr;
} __PACKED;

// ===========================================
// Link Key Notification Event (v1.1) (BR/EDR)
constexpr EventCode kLinkKeyNotificationEventCode = 0x18;

struct LinkKeyNotificationEventParams {
  // The address for the device for which a new link key has been generated.
  DeviceAddressBytes bd_addr;

  // Link key for the associated address.
  uint8_t link_key[kBrEdrLinkKeySize];

  // Type of key used when pairing.
  uint8_t key_type;
} __PACKED;

// ===========================================
// Data Buffer Overflow Event (v1.1) (BR/EDR & LE)
constexpr EventCode kDataBufferOverflowEventCode = 0x1A;

struct DataBufferOverflowEventParams {
  // The type of data that caused the overflow.
  LinkType ll_type;
} __PACKED;

// ==============================================
// Inquiry Result with RSSI Event (v1.2) (BR/EDR)
constexpr EventCode kInquiryResultWithRSSIEventCode = 0x22;

struct InquiryResultRSSI {
  // The address for the device which responded.
  DeviceAddressBytes bd_addr;

  // The Page Scan Repetition Mode being used by the remote device.
  PageScanRepetitionMode page_scan_repetition_mode;

  // Reserved (no meaning as of v1.2)
  uint8_t page_scan_period_mode;

  // Class of device
  DeviceClass class_of_device;

  // Clock Offset
  // the 15 lower bits represent bits 16-2 of CLKNPeripheral-CLK
  // the most significant bit is reserved
  uint16_t clock_offset;

  // RSSI
  // Valid range: -127 to +20
  int8_t rssi;
} __PACKED;

struct InquiryResultWithRSSIEventParams {
  InquiryResultWithRSSIEventParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(InquiryResultWithRSSIEventParams);

  // The number of responses included.
  uint8_t num_responses;

  InquiryResultRSSI responses[];
} __PACKED;

// ============================================================
// Read Remote Extended Features Complete Event (v1.1) (BR/EDR)
constexpr EventCode kReadRemoteExtendedFeaturesCompleteEventCode = 0x23;

struct ReadRemoteExtendedFeaturesCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // A connection handle for an ACL connection.
  //  Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Page number
  uint8_t page_number;

  // Maximum page number
  // The highest features page number that has non-zero bits on the remote
  // device.
  uint8_t max_page_number;

  // Bit Mask List of LMP features. See enum class LMPFeature in hci_constants.h
  // for how to interpret this bitfield.
  uint64_t lmp_features;
} __PACKED;

// ============================================================
// Synchronous Connection Complete Event (BR/EDR)
constexpr EventCode kSynchronousConnectionCompleteEventCode = 0x2C;

struct SynchronousConnectionCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // A connection handle for the newly created SCO connection.
  ConnectionHandle connection_handle;

  // BD_ADDR of the other connected device forming the connection.
  DeviceAddressBytes bd_addr;

  LinkType link_type;

  // Time between two consecutive eSCO instants measured in slots. Shall be
  // zero for SCO links.
  uint8_t transmission_interval;

  // The size of the retransmission window measured in slots. Shall be zero for
  // SCO links.
  uint8_t retransmission_window;

  // Length in bytes of the eSCO payload in the receive direction. Shall be
  // zero for SCO links.
  uint16_t rx_packet_length;

  // Length in bytes of the eSCO payload in the transmit direction. Shall be
  // zero for SCO links.
  uint16_t tx_packet_length;

  CodingFormat air_coding_format;
} __PACKED;

// =============================================
// Extended Inquiry Result Event (v1.2) (BR/EDR)
constexpr EventCode kExtendedInquiryResultEventCode = 0x2F;

struct ExtendedInquiryResultEventParams {
  // Num_Responses
  // The number of responses from the inquiry.
  // Must be 1.
  uint8_t num_responses;

  // BD_ADDR of the device that responded.
  DeviceAddressBytes bd_addr;

  // The Page Scan Repetition Mode being used by the remote device.
  PageScanRepetitionMode page_scan_repetition_mode;

  // Reserved for future use
  uint8_t reserved;

  // Class of device
  DeviceClass class_of_device;

  // Clock offset
  // the 15 lower bits represent bits 16-2 of CLKNPeripheral-CLK
  // the most significant bit is reserved
  uint16_t clock_offset;

  // RSSI in dBm.
  // Valid range: -127 to +20
  int8_t rssi;

  // Extended inquiey response data as defined in Vol 3, Part C, Sec 8
  uint8_t extended_inquiry_response[kExtendedInquiryResponseBytes];
} __PACKED;

// ================================================================
// Encryption Key Refresh Complete Event (v2.1 + EDR) (BR/EDR & LE)
constexpr EventCode kEncryptionKeyRefreshCompleteEventCode = 0x30;

struct EncryptionKeyRefreshCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// =================================================
// IO Capability Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kIOCapabilityRequestEventCode = 0x31;

struct IOCapabilityRequestEventParams {
  // The address of the remote device involved in the simple pairing process
  DeviceAddressBytes bd_addr;
} __PACKED;

// ==================================================
// IO Capability Response Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kIOCapabilityResponseEventCode = 0x32;

struct IOCapabilityResponseEventParams {
  // The address of the remote device which the IO capabilities apply
  DeviceAddressBytes bd_addr;

  // IO Capabilities of the device
  IOCapability io_capability;

  // Whether OOB Data is present.
  // Allowed values:
  //  0x00 - OOB authentication data not present
  //  0x01 - OOB authentication data from remote device present
  uint8_t oob_data_present;

  // Authentication Requirements.
  // See AuthenticationRequirements in hci_constants.h
  AuthRequirements auth_requirements;
} __PACKED;

// =====================================================
// User Confirmation Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserConfirmationRequestEventCode = 0x33;

struct UserConfirmationRequestEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // Numeric value to be displayed. Valid values are 0 - 999999.
  uint32_t numeric_value;
} __PACKED;

// ================================================
// User Passkey Request Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserPasskeyRequestEventCode = 0x34;

struct UserPasskeyRequestEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __PACKED;

// ===================================================
// Simple Pairing Complete Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kSimplePairingCompleteEventCode = 0x36;

struct SimplePairingCompleteEventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;
} __PACKED;

// =====================================================
// User Passkey Notification Event (v2.1 + EDR) (BR/EDR)
constexpr EventCode kUserPasskeyNotificationEventCode = 0x3B;

struct UserPasskeyNotificationEventParams {
  // Address of the device involved in simple pairing process
  DeviceAddressBytes bd_addr;

  // Numeric value (passkey) entered by user. Valid values are 0 - 999999.
  uint32_t numeric_value;
} __PACKED;

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
} __PACKED;

// LE Connection Complete Event (v4.0) (LE)
constexpr EventCode kLEConnectionCompleteSubeventCode = 0x01;

struct LEConnectionCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  ConnectionRole role;
  LEPeerAddressType peer_address_type;

  // Public Device Address or Random Device Address of the peer device.
  DeviceAddressBytes peer_address;

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

  // The Central_Clock_Accuracy parameter is only valid for a peripheral. On a central, this
  // parameter shall be set to 0x00.
  LEClockAccuracy central_clock_accuracy;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// LE Connection Update Complete Event (v4.0) (LE)
constexpr EventCode kLEConnectionUpdateCompleteSubeventCode = 0x03;

struct LEConnectionUpdateCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

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
} __PACKED;

// LE Read Remote Features Complete Event (v4.0) (LE)
constexpr EventCode kLEReadRemoteFeaturesCompleteSubeventCode = 0x04;

struct LEReadRemoteFeaturesCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Bit Mask List of supported LE features. See enum class LEFeatures in
  // hci_constants.h.
  uint64_t le_features;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// LE Read Local P-256 Public Key Complete Event (v4.2) (LE)
constexpr EventCode kLEReadLocalP256PublicKeyCompleteSubeventCode = 0x08;

struct LEReadLOcalP256PublicKeyCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Local P-256 public key.
  uint8_t local_p256_public_key[64];
} __PACKED;

// LE Generate DHKey Complete Event (v4.2) (LE)
constexpr EventCode kLEGenerateDHKeyCompleteSubeventCode = 0x09;

struct LEGenerateDHKeyCompleteSubeventParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Diffie Hellman Key.
  uint8_t dh_key[32];
} __PACKED;

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

  // The Central_Clock_Accuracy parameter is only valid for a peripheral. On a central, this
  // parameter shall be set to 0x00.
  LEClockAccuracy central_clock_accuracy;
} __PACKED;

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
} __PACKED;

struct LEDirectedAdvertisingReportSubeventParams {
  LEDirectedAdvertisingReportSubeventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEDirectedAdvertisingReportSubeventParams);

  // Number of LEAdvertisingReportData instances contained in the array
  // |reports|.
  uint8_t num_reports;

  // The report array parameters.
  LEDirectedAdvertisingReportData reports[];
} __PACKED;

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
} __PACKED;

// LE Extended Advertising Report Event (v5.0) (LE)
constexpr EventCode kLEExtendedAdvertisingReportSubeventCode = 0x0D;

struct LEExtendedAdvertisingReportData {
  LEExtendedAdvertisingReportData() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEExtendedAdvertisingReportData);

  // The advertising event type bitfield. For more information on how to
  // interpret this see kLEExtendedAdvEventType* constants in hci_constants.h
  // and Core Spec v5.0, Vol 2, Part E, Section 7.7.65.13.
  uint16_t event_type;

  // Address type of the advertiser.
  LEAddressType address_type;

  // Public Device Address, Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the advertising device.
  DeviceAddressBytes address;

  // Indicates the PHY used to send the advertising PDU on the primary
  // advertising channel. Legacy PDUs always use LEPHY::kLE1M
  //
  // LEPHY::kNone, LEPHY::kLE2M, and LEPHY::kLECodedS2 are excluded.
  LEPHY primary_phy;

  // Indicates the PHY used to send the advertising PDU(s), if any, on the
  // secondary advertising channel. A value of LEPHY::kNone means that no
  // packets were received on the secondary advertising channel.
  LEPHY secondary_phy;

  // Value of the Advertising SID subfield in the ADI field of the PDU. A value
  // of 0x00 means no ADI field in the PDU.
  uint8_t advertising_sid;

  // Range: -127 <= N <= +126
  // Units: dBm
  int8_t tx_power;

  // Range: -127 <= N <= +20
  // Units: dBm
  // If N == 127: RSSI is not available.
  int8_t rssi;

  // 0x0000: No periodic advertising.
  // 0xXXXX:
  //   Range: See kLEPeriodicAdvertisingInterval[Min|Max] in hci_constants.h
  //   Time = N * 1.25 ms
  //   Time Range: 7.5ms to 81.91875 s
  uint16_t periodic_adv_interval;

  LEAddressType direct_address_type;

  // Public Device Address, Random Device Address, Public Identity Address or
  // Random (static) Identity Address of the target device.
  DeviceAddressBytes direct_address;

  // Length of the data field.
  uint8_t data_length;

  // The beginning of |data_length| octets of advertising or scan response data
  // formatted as defined in Core Spec v5.0, Vol 3, Part C, Section 11.
  uint8_t data[];
} __PACKED;

struct LEExtendedAdvertisingReportSubeventParams {
  LEExtendedAdvertisingReportSubeventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEExtendedAdvertisingReportSubeventParams);

  // Number of separate reports in the event.
  uint8_t num_reports;

  // Beginning of LEExtendedAdvertisingReportData array. Since each report data
  // has a variable length, the contents of |reports| this is declared as an
  // array of uint8_t.
  uint8_t reports[];
} __PACKED;

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
  LEClockAccuracy advertiser_clock_accuracy;
} __PACKED;

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
} __PACKED;

// LE Periodic Advertising Sync Lost Event (v5.0) (LE)
constexpr EventCode kLEPeriodicAdvertisingSyncLostSubeventCode = 0x10;

struct LEPeriodicAdvertisingSyncLostSubeventParams {
  // Used to identify the periodic advertiser (only the lower 12 bits are
  // meaningful).
  PeriodicAdvertiserHandle sync_handle;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// LE Channel Selection Algorithm Event (v5.0) (LE)
constexpr EventCode kLEChannelSelectionAlgorithmSubeventCode = 0x014;

struct LEChannelSelectionAlgorithmSubeventParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // Channel selection algorithm is used on the data channel connection.
  LEChannelSelectionAlgorithm channel_selection_algorithm;
} __PACKED;

// ================================================================
// Number Of Completed Data Blocks Event (v3.0 + HS) (BR/EDR & AMP)
constexpr EventCode kNumberOfCompletedDataBlocksEventCode = 0x48;

struct NumberOfCompletedDataBlocksEventData {
  // Handle (Connection Handle for a BR/EDR Controller or a Logical_Link Handle
  // for an AMP Controller).
  uint16_t handle;
  uint16_t num_of_completed_packets;
  uint16_t num_of_completed_blocks;
} __PACKED;

struct NumberOfCompletedDataBlocksEventParams {
  NumberOfCompletedDataBlocksEventParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(NumberOfCompletedDataBlocksEventParams);

  uint16_t total_num_data_blocks;
  uint8_t number_of_handles;
  NumberOfCompletedDataBlocksEventData data[];
} __PACKED;

// ================================================================
// Authenticated Payload Timeout Expired Event (v4.1) (BR/EDR & LE)
constexpr EventCode kAuthenticatedPayloadTimeoutExpiredEventCode = 0x57;

struct AuthenticatedPayloadTimeoutExpiredEventParams {
  // Connection_Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// ========================================
// Read Encryption Key Size (v1.1) (BR/EDR)
constexpr OpCode kReadEncryptionKeySize = StatusParamsOpCode(0x0008);

struct ReadEncryptionKeySizeParams {
  // Identifies an active ACL link (only the lower 12 bits are meaningful).
  ConnectionHandle connection_handle;
} __PACKED;

struct ReadEncryptionKeySizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Handle of the ACL connection whose encryption key size was read.
  ConnectionHandle connection_handle;

  // Encryption key size. See v5.0 Vol 2 Part C, Section 5.2.
  uint8_t key_size;
} __PACKED;

// ======= LE Controller Commands =======
// Core Spec v5.0 Vol 2, Part E, Section 7.8
constexpr uint8_t kLEControllerCommandsOGF = 0x08;
constexpr OpCode LEControllerCommandOpCode(const uint16_t ocf) {
  return DefineOpCode(kLEControllerCommandsOGF, ocf);
}

// Returns true if the given |opcode| corresponds to a LE controller command.
inline bool IsLECommand(OpCode opcode) { return GetOGF(opcode) == kLEControllerCommandsOGF; }

// =====================================
// LE Set Event Mask Command (v4.0) (LE)
constexpr OpCode kLESetEventMask = LEControllerCommandOpCode(0x0001);

struct LESetEventMaskCommandParams {
  // See enum LEEventMask in hci_constants.h for possible values.
  uint64_t le_event_mask;
} __PACKED;

// =======================================
// LE Read Buffer Size Command (v4.0) (LE)
constexpr OpCode kLEReadBufferSize = LEControllerCommandOpCode(0x0002);

struct LEReadBufferSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t hc_le_acl_data_packet_length;
  uint8_t hc_total_num_le_acl_data_packets;
} __PACKED;

// ====================================================
// LE Read Local Supported Features Command (v4.0) (LE)
constexpr OpCode kLEReadLocalSupportedFeatures = LEControllerCommandOpCode(0x0003);

struct LEReadLocalSupportedFeaturesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit Mask List of supported LE features. See enum class LESupportedFeature
  // in hci_constants.h.
  uint64_t le_features;
} __PACKED;

// =========================================
// LE Set Random Address Command (v4.0) (LE)
constexpr OpCode kLESetRandomAddress = LEControllerCommandOpCode(0x0005);

struct LESetRandomAddressCommandParams {
  DeviceAddressBytes random_address;
} __PACKED;

// =================================================
// LE Set Advertising Parameters Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingParameters = LEControllerCommandOpCode(0x0006);

struct LESetAdvertisingParametersCommandParams {
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
  LEAdvertisingType adv_type;

  LEOwnAddressType own_address_type;
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
} __PACKED;

// ========================================================
// LE Read Advertising Channel Tx Power Command (v4.0) (LE)
constexpr OpCode kLEReadAdvertisingChannelTxPower = LEControllerCommandOpCode(0x0007);

struct LEReadAdvertisingChannelTxPowerReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // The transmit power level used for LE advertising channel packets.
  //
  //   Range: -20 <= N <= +10
  //   Units: dBm
  //   Accuracy: +/- 4 dB
  int8_t tx_power;
} __PACKED;

// ===========================================
// LE Set Advertising Data Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingData = LEControllerCommandOpCode(0x0008);

struct LESetAdvertisingDataCommandParams {
  // The number of significant octets in |adv_data|.
  uint8_t adv_data_length;

  // 31 octets of advertising data formatted as defined in Core Spec v5.0, Vol
  // 3, Part C, Section 11.
  //
  // Default: All octets zero.
  uint8_t adv_data[kMaxLEAdvertisingDataLength];
} __PACKED;

// =============================================
// LE Set Scan Response Data Command (v4.0) (LE)
constexpr OpCode kLESetScanResponseData = LEControllerCommandOpCode(0x0009);

struct LESetScanResponseDataCommandParams {
  // The number of significant octets in |scan_rsp_data|.
  uint8_t scan_rsp_data_length;

  // 31 octets of Scan Response Data formatted as defined in Core Spec v5.0, Vol
  // 3, Part C, Section 11.
  //
  // Default: All octets zero.
  uint8_t scan_rsp_data[kMaxLEAdvertisingDataLength];
} __PACKED;

// =============================================
// LE Set Advertising Enable Command (v4.0) (LE)
constexpr OpCode kLESetAdvertisingEnable = LEControllerCommandOpCode(0x000A);

struct LESetAdvertisingEnableCommandParams {
  GenericEnableParam advertising_enable;
} __PACKED;

// ==========================================
// LE Set Scan Parameters Command (v4.0) (LE)
constexpr OpCode kLESetScanParameters = LEControllerCommandOpCode(0x000B);

struct LESetScanParametersCommandParams {
  // Controls the type of scan to perform.
  LEScanType scan_type;

  // Range: see kLEScanInterval[Min|Max] in hci_constants.h
  // Default: N = kLEScanIntervalDefault (see hci_constants.h)
  // Time: N * 0.625 ms
  // Time Range: 2.5 ms to 10.24 s
  uint16_t scan_interval;
  uint16_t scan_window;

  LEOwnAddressType own_address_type;
  LEScanFilterPolicy filter_policy;
} __PACKED;

// ======================================
// LE Set Scan Enable Command (v4.0) (LE)
constexpr OpCode kLESetScanEnable = LEControllerCommandOpCode(0x000C);

struct LESetScanEnableCommandParams {
  GenericEnableParam scanning_enabled;

  // (See Core Spec v5.0, Vol 6, Part B, Section 4.4.3.5)
  GenericEnableParam filter_duplicates;
} __PACKED;

// ========================================
// LE Create Connection Command (v4.0) (LE)
constexpr OpCode kLECreateConnection = LEControllerCommandOpCode(0x000D);

struct LECreateConnectionCommandParams {
  // Range: see kLEScanInterval[Min|Max] in hci_constants.h
  // Time: N * 0.625 ms
  // Time Range: 2.5 ms to 10.24 s
  uint16_t scan_interval;

  // Range: see kLEScanInterval[Min|Max] in hci_constants.h
  // Time: N * 0.625 ms
  // Time Range: 2.5 ms to 10.24 s
  uint16_t scan_window;

  GenericEnableParam initiator_filter_policy;
  LEAddressType peer_address_type;
  DeviceAddressBytes peer_address;
  LEOwnAddressType own_address_type;

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
} __PACKED;

// NOTE on ReturnParams: No Command Complete event is sent by the Controller to
// indicate that this command has been completed. Instead, the LE Connection
// Complete or LE Enhanced Connection Complete event indicates that this command
// has been completed.

// ===============================================
// LE Create Connection Cancel Command (v4.0) (LE)
constexpr OpCode kLECreateConnectionCancel = LEControllerCommandOpCode(0x000E);

// ===========================================
// LE Read Filter Accept List Size Command (v4.0) (LE)
constexpr OpCode kLEReadFilterAcceptListSize = LEControllerCommandOpCode(0x000F);

struct LEReadFilterAcceptListSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  uint8_t filter_accept_list_size;
} __PACKED;

// =======================================
// LE Clear Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLEClearFilterAcceptList = LEControllerCommandOpCode(0x0010);

// ===============================================
// LE Add Device To Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLEAddDeviceToFilterAcceptList = LEControllerCommandOpCode(0x0011);

struct LEAddDeviceToFilterAcceptListCommandParams {
  // The address type of the peer. The |address| parameter will be ignored if
  // |address_type| is set to LEPeerAddressType::kAnonymous.
  LEPeerAddressType address_type;

  // Public Device Address or Random Device Address of the device to be added to
  // the Filter Accept List
  DeviceAddressBytes address;
} __PACKED;

// ====================================================
// LE Remove Device From Filter Accept List Command (v4.0) (LE)
constexpr OpCode kLERemoveDeviceFromFilterAcceptList = LEControllerCommandOpCode(0x0012);

struct LERemoveDeviceFromFilterAcceptListCommandParams {
  // The address type of the peer. The |address| parameter will be ignored if
  // |address_type| is set to LEPeerAddressType::kAnonymous.
  LEPeerAddressType address_type;

  // Public Device Address or Random Device Address of the device to be removed
  // from the Filter Accept List
  DeviceAddressBytes address;
} __PACKED;

// ========================================
// LE Connection Update Command (v4.0) (LE)
constexpr OpCode kLEConnectionUpdate = LEControllerCommandOpCode(0x0013);

struct LEConnectionUpdateCommandParams {
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
} __PACKED;

// NOTE on Return Params: A Command Complete event is not sent by the Controller
// to indicate that this command has been completed. Instead, the LE Connection
// Update Complete event indicates that this command has been completed.

// ======================================================
// LE Set Host Channel Classification Command (v4.0) (LE)
constexpr OpCode kLESetHostChannelClassification = LEControllerCommandOpCode(0x0014);

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
} __PACKED;

// =======================================
// LE Read Channel Map Command (v4.0) (LE)
constexpr OpCode kLEReadChannelMap = LEControllerCommandOpCode(0x0015);

struct LEReadChannelMapCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

// ===========================================
// LE Read Remote Features Command (v4.0) (LE)
constexpr OpCode kLEReadRemoteFeatures = LEControllerCommandOpCode(0x0016);

struct LEReadRemoteFeaturesCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

struct LEEncryptReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // 128 bit encrypted data block.
  uint8_t encrypted_data[16];
} __PACKED;

// ===========================
// LE Rand Command (v4.0) (LE)
constexpr OpCode kLERand = LEControllerCommandOpCode(0x0018);

struct LERandReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Random Number
  uint64_t random_number;
} __PACKED;

// =======================================
// LE Start Encryption Command (v4.0) (LE)
constexpr OpCode kLEStartEncryption = LEControllerCommandOpCode(0x0019);

// The parameters below are as defined in Core Spec v5.0, Vol 3, Part H,
// Section 2.4.4 "Encrypted Session Setup".
struct LEStartEncryptionCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;

  // 64-bit random number.
  uint64_t random_number;

  // 16-bit encrypted diversifier.
  uint16_t encrypted_diversifier;

  // 128-bit long-term key (LTK).
  UInt128 long_term_key;
} __PACKED;

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
} __PACKED;

struct LELongTermKeyRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// ===========================================================
// LE Long Term Key Request Negative Reply Command (v4.0) (LE)
constexpr OpCode kLELongTermKeyRequestNegativeReply = LEControllerCommandOpCode(0x001B);

struct LELongTermKeyRequestNegativeReplyCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

struct LELongTermKeyRequestNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// ============================================
// LE Read Supported States Command (v4.0) (LE)
constexpr OpCode kLEReadSupportedStates = LEControllerCommandOpCode(0x001C);

struct LEReadSupportedStatesReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Bit-mask of supported state or state combinations. See Core Spec v4.2,
  // Volume 2, Part E, Section 7.8.27 "LE Read Supported States Command".
  uint64_t le_states;
} __PACKED;

// ====================================
// LE Receiver Test Command (v4.0) (LE)
constexpr OpCode kLEReceiverTest = LEControllerCommandOpCode(0x001D);

struct LEReceiverTestCommandParams {
  // N = (F - 2402) / 2
  // Range: 0x00 - 0x27. Frequency Range : 2402 MHz to 2480 MHz.
  uint8_t rx_channel;
} __PACKED;

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
} __PACKED;

// ===============================
// LE Test End Command (v4.0) (LE)
constexpr OpCode kLETestEnd = LEControllerCommandOpCode(0x001F);

struct LETestEndReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Number of packets received
  uint16_t number_of_packets;
} __PACKED;

// ================================================================
// LE Remote Connection Parameter Request Reply Command (v4.1) (LE)
constexpr OpCode kLERemoteConnectionParameterRequestReply = LEControllerCommandOpCode(0x0020);

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
} __PACKED;

struct LERemoteConnectionParameterRequestReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

struct LERemoteConnectionParamReqNegativeReplyReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

struct LESetDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

// =========================================================
// LE Read Suggested Default Data Length Command (v4.2) (LE)
constexpr OpCode kLEReadSuggestedDefaultDataLength = LEControllerCommandOpCode(0x0023);

struct LEReadSuggestedDefaultDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_time;
} __PACKED;

// ==========================================================
// LE Write Suggested Default Data Length Command (v4.2) (LE)
constexpr OpCode kLEWriteSuggestedDefaultDataLength = LEControllerCommandOpCode(0x0024);

struct LEWriteSuggestedDefaultDataLengthCommandParams {
  // Range: see kLEMaxTxOctets[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_octets;

  // Range: see kLEMaxTxTime[Min|Max] in hci_constants.h
  uint16_t suggested_max_tx_time;
} __PACKED;

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
} __PACKED;

// NOTE on ReturnParams: When the Controller receives the LE_Generate_DHKey
// command, the Controller shall send the Command Status event to the Host. When
// the DHKey generation finishes, an LE DHKey Generation Complete event shall be
// generated.
//
// No Command Complete event is sent by the Controller to indicate that this
// command has been completed.

// ===================================================
// LE Add Device To Resolving List Command (v4.2) (LE)
constexpr OpCode kLEAddDeviceToResolvingList = LEControllerCommandOpCode(0x0027);

struct LEAddDeviceToResolvingListCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;

  // IRK (Identity Resolving Key) of the peer device
  UInt128 peer_irk;

  // IRK (Identity Resolving Key) of the local device
  UInt128 local_irk;
} __PACKED;

// ========================================================
// LE Remove Device From Resolving List Command (v4.2) (LE)
constexpr OpCode kLERemoveDeviceFromResolvingList = LEControllerCommandOpCode(0x0028);

struct LERemoveDeviceFromResolvingListCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;
} __PACKED;

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
} __PACKED;

// ===================================================
// LE Read Peer Resolvable Address Command (v4.2) (LE)
constexpr OpCode kLEReadPeerResolvableAddress = LEControllerCommandOpCode(0x002B);

struct LEReadPeerResolvableAddressCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device.
  DeviceAddressBytes peer_identity_address;
} __PACKED;

struct LEReadPeerResolvableAddressReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Resolvable Private Address being used by the peer device.
  DeviceAddressBytes peer_resolvable_address;
} __PACKED;

// ====================================================
// LE Read Local Resolvable Address Command (v4.2) (LE)
constexpr OpCode kLEReadLocalResolvableAddress = LEControllerCommandOpCode(0x002C);

struct LEReadLocalResolvableAddressCommandParams {
  // The peer device's identity address type.
  LEPeerAddressType peer_identity_address_type;

  // Public or Random (static) Identity address of the peer device
  DeviceAddressBytes peer_identity_address;
} __PACKED;

struct LEReadLocalResolvableAddressReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Resolvable Private Address being used by the local device.
  DeviceAddressBytes local_resolvable_address;
} __PACKED;

// ====================================================
// LE Set Address Resolution Enable Command (v4.2) (LE)
constexpr OpCode kLESetAddressResolutionEnable = LEControllerCommandOpCode(0x002D);

struct LESetAddressResolutionEnableCommandParams {
  GenericEnableParam address_resolution_enable;
} __PACKED;

// =============================================================
// LE Set Resolvable Private Address Timeout Command (v4.2) (LE)
constexpr OpCode kLESetResolvablePrivateAddressTimeout = LEControllerCommandOpCode(0x002E);

struct LESetResolvablePrivateAddressTimeoutCommandParams {
  // Range: See kLERPATimeout[Min|Max] in hci_constants.h
  // Default: See kLERPATimeoutDefault in hci_constants.h
  uint16_t rpa_timeout;
} __PACKED;

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
} __PACKED;

// ===============================
// LE Read PHY Command (v5.0) (LE)
constexpr OpCode kLEReadPHY = LEControllerCommandOpCode(0x0030);

struct LEReadPHYCommandParams {
  // Connection Handle (only the lower 12-bits are meaningful).
  //   Range: 0x0000 to kConnectionHandleMax in hci_constants.h
  ConnectionHandle connection_handle;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

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
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// =========================================================
// LE Set Advertising Set Random Address Command (v5.0) (LE)
constexpr OpCode kLESetAdvertisingSetRandomAddress = LEControllerCommandOpCode(0x0035);

struct LESetAdvertisingSetRandomAddressCommandParams {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Random Device Address.
  DeviceAddressBytes adv_random_address;
} __PACKED;

// ==========================================================
// LE Set Extended Advertising Parameters Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingParameters = LEControllerCommandOpCode(0x0036);

struct LESetExtendedAdvertisingParametersCommandParams {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // See the kLEAdvEventPropBit* constants in hci_constants.h for possible bit
  // values.
  uint16_t adv_event_properties;

  // Range: See kLEExtendedAdvertisingInterval[Min|Max] in hci_constants.h
  // Time = N * 0.625 s
  // Time Range: 20 ms to 10,485.759375 s
  uint8_t primary_adv_interval_min[3];
  uint8_t primary_adv_interval_max[3];

  // (see the constants kLEAdvertisingChannel* in hci_constants.h for possible
  // values).
  uint8_t primary_adv_channel_map;

  LEOwnAddressType own_address_type;
  LEPeerAddressType peer_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the device to be connected.
  DeviceAddressBytes peer_address;

  LEAdvFilterPolicy adv_filter_policy;

  // Range: -127 <= N <= +126
  // Units: dBm
  // If N = 127: Host has no preference.
  int8_t adv_tx_power;

  // LEPHY::kLE2M and LEPHY::kLECodedS2 are excluded.
  LEPHY primary_adv_phy;

  uint8_t secondary_adv_max_skip;
  LEPHY secondary_adv_phy;
  uint8_t advertising_sid;
  GenericEnableParam scan_request_notification_enable;
} __PACKED;

struct LESetExtendedAdvertisingParametersReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;
  int8_t selected_tx_power;
} __PACKED;

// ====================================================
// LE Set Extended Advertising Data Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingData = LEControllerCommandOpCode(0x0037);

struct LESetExtendedAdvertisingDataCommandParams {
  LESetExtendedAdvertisingDataCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LESetExtendedAdvertisingDataCommandParams);

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // See hci_constants.h for possible values.
  LESetExtendedAdvDataOp operation;

  // The Fragment_Preference parameter provides a hint to the Controller as to
  // whether advertising data should be fragmented.
  LEExtendedAdvFragmentPreference fragment_preference;

  // Length of the advertising data included in this command packet, up to
  // kMaxLEExtendedAdvertisingDataLength bytes. If the advertising set uses
  // legacy advertising PDUs that support advertising data then this shall not
  // exceed kMaxLEAdvertisingDataLength bytes.
  uint8_t adv_data_length;

  // Variable length advertising data.
  uint8_t adv_data[];
} __PACKED;

// ======================================================
// LE Set Extended Scan Response Data Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanResponseData = LEControllerCommandOpCode(0x0038);

struct LESetExtendedScanResponseDataCommandParams {
  LESetExtendedScanResponseDataCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LESetExtendedScanResponseDataCommandParams);

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // See hci_constants.h for possible values.
  // LESetExtendedAdvDataOp::kUnchangedData is excluded for scan response data.
  LESetExtendedAdvDataOp operation;

  LEExtendedAdvFragmentPreference fragment_preference;

  // Length of the scan response data included in this command packet, up to
  // kMaxLEExtendedAdvertisingDataLength bytes. If the advertising set uses
  // scannable legacy advertising PDUs then this shall not exceed
  // kMaxLEAdvertisingDataLength bytes.
  uint8_t scan_rsp_data_length;

  // Variable length scan response data.
  uint8_t scan_rsp_data[];
} __PACKED;

// ======================================================
// LE Set Extended Advertising Enable Command (v5.0) (LE)
constexpr OpCode kLESetExtendedAdvertisingEnable = LEControllerCommandOpCode(0x0039);

struct LESetExtendedAdvertisingEnableData {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;

  // Possible values:
  //   0x0000: No advertising duration. Advertising to continue until the Host
  //   disables it. 0x0001-0xFFFF: Advertising duration, where:
  //     Time = N * 10 ms
  //     Time Range: 10 ms to 655,350 ms
  uint16_t duration;

  // Possible values:
  //   0x00: No maximum number of advertising events.
  //   0xXX: Maximum number of extended advertising events the Controller shall
  //   attempt to send
  //         prior to terminating the extended advertising
  uint8_t max_extended_adv_events;
} __PACKED;

struct LESetExtendedAdvertisingEnableCommandParams {
  LESetExtendedAdvertisingEnableCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LESetExtendedAdvertisingEnableCommandParams);

  // Enable or Disable extended advertising.
  GenericEnableParam enable;

  // The number of advertising sets contained in the parameter arrays. If Enable
  // and Number_of_Sets are both set to 0x00, then all advertising sets are
  // disabled.
  uint8_t number_of_sets;

  // The parameter array containing |number_of_sets| entries for each
  // advertising set included in this command.
  LESetExtendedAdvertisingEnableData data[];
} __PACKED;

// ===========================================================
// LE Read Maximum Advertising Data Length Command (v5.0) (LE)
constexpr OpCode kLEReadMaxAdvertisingDataLength = LEControllerCommandOpCode(0x003A);

struct LEReadMaxAdvertisingDataLengthReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint16_t max_adv_data_length;
} __PACKED;

// ================================================================
// LE Read Number of Supported Advertising Sets Command (v5.0) (LE)
constexpr OpCode kLEReadNumSupportedAdvertisingSets = LEControllerCommandOpCode(0x003B);

struct LEReadNumSupportedAdvertisingSetsReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  uint8_t num_supported_adv_sets;
} __PACKED;

// =============================================
// LE Remove Advertising Set Command (v5.0) (LE)
constexpr OpCode kLERemoveAdvertisingSet = LEControllerCommandOpCode(0x003C);

struct LERemoveAdvertisingSetCommandParams {
  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

// =============================================
// LE Clear Advertising Sets Command (v5.0) (LE)
constexpr OpCode kLEClearAdvertisingSets = LEControllerCommandOpCode(0x003D);

// ==========================================================
// LE Set Periodic Advertising Parameters Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingParameters = LEControllerCommandOpCode(0x003E);

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
} __PACKED;

// ====================================================
// LE Set Periodic Advertising Data Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingData = LEControllerCommandOpCode(0x003F);

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
} __PACKED;

// ======================================================
// LE Set Periodic Advertising Enable Command (v5.0) (LE)
constexpr OpCode kLESetPeriodicAdvertisingEnable = LEControllerCommandOpCode(0x0040);

struct LESetPeriodicAdvertisingEnableCommandParams {
  // Enable or Disable periodic advertising.
  GenericEnableParam enable;

  // Handle used to identify an advertising set.
  AdvertisingHandle adv_handle;
} __PACKED;

// ===================================================
// LE Set Extended Scan Parameters Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanParameters = LEControllerCommandOpCode(0x0041);

struct LESetExtendedScanParametersData {
  // Controls the type of scan to perform.
  LEScanType scan_type;

  // Range: see kLEExtendedScanInterval[Min|Max] in hci_constants.h
  // Time: N * 0.625 ms
  // Time Range: 2.5 ms to 40.959375 s
  uint16_t scan_interval;
  uint16_t scan_window;
} __PACKED;

struct LESetExtendedScanParametersCommandParams {
  LESetExtendedScanParametersCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LESetExtendedScanParametersCommandParams);

  // Indicates the type of address being used in the scan request packets (for
  // active scanning).
  LEOwnAddressType own_address_type;

  // The LE filter accept list and privacy filter policy that should be used while scanning for
  // directed and undirected advertisements.
  LEScanFilterPolicy filter_policy;

  // See kLEPHYBit* constants in hci_constants.h for possible values. kLEPHYBit2M is excluded for
  // this command.
  uint8_t scan_phys;

  // The number of array elements is determined by the number of bits set in the scan_phys
  // parameter.
  LESetExtendedScanParametersData data[];
} __PACKED;

// ===============================================
// LE Set Extended Scan Enable Command (v5.0) (LE)
constexpr OpCode kLESetExtendedScanEnable = LEControllerCommandOpCode(0x0042);

struct LESetExtendedScanEnableCommandParams {
  GenericEnableParam scanning_enabled;
  LEExtendedDuplicateFilteringOption filter_duplicates;

  // Possible values:
  //   0x0000: Scan continuously until explicitly disabled
  //   0x0001-0xFFFF: Scan duration, where:
  //     Time = N * 10 ms
  //     Time Range: 10 ms to 655.35 s
  uint16_t duration;

  // Possible values:
  //   0x0000: Periodic scanning disabled
  //   0xXXXX: Time interval from when the Controller started its last
  //   Scan_Duration until it begins the subsequent Scan_Duration, where:
  //     Range: 0x0001 – 0xFFFF
  //     Time = N * 1.28 sec
  //     Time Range: 1.28 s to 83,884.8 s
  uint16_t period;
} __PACKED;

// =================================================
// LE Extended Create Connection Command (v5.0) (LE)
constexpr OpCode kLEExtendedCreateConnection = LEControllerCommandOpCode(0x0043);

struct LEExtendedCreateConnectionData {
  // Range: see kLEExtendedScanInterval[Min|Max] in hci_constants.h
  // Time: N * 0.625 ms
  // Time Range: 2.5 ms to 40.959375 s
  uint16_t scan_interval;
  uint16_t scan_window;

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
} __PACKED;

struct LEExtendedCreateConnectionCommandParams {
  LEExtendedCreateConnectionCommandParams() = delete;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(LEExtendedCreateConnectionCommandParams);

  GenericEnableParam initiator_filter_policy;
  LEOwnAddressType own_address_type;
  LEPeerAddressType peer_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the device to be connected.
  DeviceAddressBytes peer_address;

  // See the kLEPHYBit* constants in hci_constants.h for possible bitfield
  // values.
  uint8_t initiating_phys;

  // The number of array elements is determined by the number of bits set in the
  // |initiating_phys| parameter.
  LEExtendedCreateConnectionData data[];
} __PACKED;

// NOTE on ReturnParams: No Command Complete event is sent by the Controller to
// indicate that this command has been completed. Instead, the LE Enhanced
// Connection Complete event indicates that this command has been completed.

// =======================================================
// LE Periodic Advertising Create Sync Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingCreateSync = LEControllerCommandOpCode(0x0044);

struct LEPeriodicAdvertisingCreateSyncCommandParams {
  LEPeriodicAdvFilterPolicy filter_policy;

  // Advertising SID subfield in the ADI field used to identify the Periodic
  // Advertising.
  uint8_t advertising_sid;

  // Address type of the advertiser. The LEAddressType::kPublicIdentity and
  // LEAddressType::kRandomIdentity values are excluded for this command.
  LEAddressType advertiser_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the advertiser.
  DeviceAddressBytes advertiser_address;

  // The number of periodic advertising packets that can be skipped after a
  // successful receive.
  //
  //   Range: 0x0000 to 0x01F3
  uint16_t skip;

  // Synchronization timeout for the periodic advertising.
  //
  //   Range: 0x000A to 0x4000
  //   Time = N * 10 ms
  //   Time Range: 100 ms to 163.84 s
  uint16_t sync_timeout;

  // As of Core Spec v5.0 this parameter is intended to be used in a future
  // feature. The Host must set this value to 0x00.
  uint8_t unused;
} __PACKED;

// NOTE on ReturnParams: No Command Complete event is sent by the Controller to
// indicate that this command has been completed. Instead, the LE Periodic
// Advertising Sync Established event indicates that this command has been
// completed.

// ==============================================================
// LE Periodic Advertising Create Sync Cancel Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingCreateSyncCancel = LEControllerCommandOpCode(0x0045);

// ==========================================================
// LE Periodic Advertising Terminate Sync Command (v5.0) (LE)
constexpr OpCode kLEPeriodicAdvertisingTerminateSync = LEControllerCommandOpCode(0x0046);

struct LEPeriodicAdvertisingTerminateSyncCommandParams {
  // Handle used to identify the periodic advertiser (only the lower 12 bits are
  // meaningful).
  PeriodicAdvertiserHandle sync_handle;
} __PACKED;

// =============================================================
// LE Add Device To Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLEAddDeviceToPeriodicAdvertiserList = LEControllerCommandOpCode(0x0047);

struct LEAddDeviceToPeriodicAdvertiserListCommandParams {
  // Address type of the advertiser. The LEAddressType::kPublicIdentity and
  // LEAddressType::kRandomIdentity values are excluded for this command.
  LEAddressType advertiser_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the advertiser.
  DeviceAddressBytes advertiser_address;

  // Advertising SID subfield in the ADI field used to identify the Periodic
  // Advertising.
  uint8_t advertising_sid;
} __PACKED;

// ==================================================================
// LE Remove Device From Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLERemoveDeviceFromPeriodicAdvertiserList = LEControllerCommandOpCode(0x0048);

struct LERemoveDeviceFromPeriodicAdvertiserListCommandParams {
  // Address type of the advertiser. The LEAddressType::kPublicIdentity and
  // LEAddressType::kRandomIdentity values are excluded for this command.
  LEAddressType advertiser_address_type;

  // Public Device Address, Random Device Address, Public Identity Address, or
  // Random (static) Identity Address of the advertiser.
  DeviceAddressBytes advertiser_address;

  // Advertising SID subfield in the ADI field used to identify the Periodic
  // Advertising.
  uint8_t advertising_sid;
} __PACKED;

// =====================================================
// LE Clear Periodic Advertiser List Command (v5.0) (LE)
constexpr OpCode kLEClearPeriodicAdvertiserList = LEControllerCommandOpCode(0x0049);

// =========================================================
// LE Read Periodic Advertiser List Size Command (v5.0) (LE)
constexpr OpCode kLEReadPeriodicAdvertiserListSize = LEControllerCommandOpCode(0x004A);

struct LEReadPeriodicAdvertiserListSizeReturnParams {
  // See enum StatusCode in hci_constants.h.
  StatusCode status;

  // Total number of Periodic Advertiser list entries that can be stored in the
  // Controller.
  uint8_t periodic_advertiser_list_size;
} __PACKED;

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
} __PACKED;

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
} __PACKED;

// =================================================
// LE Write RF Path Compensation Command (v5.0) (LE)
constexpr OpCode kLEWriteRFPathCompensation = LEControllerCommandOpCode(0x004D);

struct LEWriteRFPathCompensationCommandParams {
  // The RF Path Compensation Values parameters used in the Tx Power Level and
  // RSSI calculation.
  //   Range: -128.0 dB (0xFB00) ≤ N ≤ 128.0 dB (0x0500)
  //   Units: 0.1 dB
  int16_t rf_tx_path_comp_value;
  int16_t rf_rx_path_comp_value;
} __PACKED;

// =======================================
// LE Set Privacy Mode Command (v5.0) (LE)
constexpr OpCode kLESetPrivacyMode = LEControllerCommandOpCode(0x004E);

struct LESetPrivacyModeCommandParams {
  // The peer identity address type (either Public Identity or Private
  // Identity).
  LEPeerAddressType peer_identity_address_type;

  // Public Identity Address or Random (static) Identity Address of the
  // advertiser.
  DeviceAddressBytes peer_identity_address;

  // The privacy mode to be used for the given entry on the resolving list.
  LEPrivacyMode privacy_mode;
} __PACKED;

// ======= Vendor Command =======
// The OGF of 0x3F is reserved for vendor-specific debug commands (see Core Spec
// v5.0, Vol 2, Part E, Section 5.4.1).
constexpr uint8_t kVendorOGF = 0x3F;
constexpr OpCode VendorOpCode(const uint16_t ocf) { return DefineOpCode(kVendorOGF, ocf); }

}  // namespace bt::hci_spec

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_SPEC_PROTOCOL_H_
