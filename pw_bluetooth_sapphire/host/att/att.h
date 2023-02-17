// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATT_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::att {

// v5.0, Vol 3, Part G, 5.1.2
constexpr uint16_t kLEMinMTU = 23;

// v5.0, Vol 3, Part G, 5.1.1
constexpr uint16_t kBREDRMinMTU = 48;

constexpr uint16_t kLEMaxMTU = hci_spec::kMaxLEExtendedDataLength - sizeof(l2cap::BasicHeader);

// The maximum length of an attribute value (v5.0, Vol 3, Part F, 3.2.9).
constexpr size_t kMaxAttributeValueLength = 512;

// The ATT protocol transaction timeout.
// (see v5.0, Vol 3, Part F, Section 3.3.3).
constexpr zx::duration kTransactionTimeout = zx::sec(30);

// A server identifies each attribute using a 16-bit handle.
using Handle = uint16_t;

constexpr Handle kInvalidHandle = 0x0000;
constexpr Handle kHandleMin = 0x0001;
constexpr Handle kHandleMax = 0xFFFF;

// We represent the read and write permissions of an attribute using separate
// bitfields.
// clang-format off
constexpr uint8_t kAttributePermissionBitAllowed                = (1 << 0);
constexpr uint8_t kAttributePermissionBitEncryptionRequired     = (1 << 1);
constexpr uint8_t kAttributePermissionBitAuthenticationRequired = (1 << 2);
constexpr uint8_t kAttributePermissionBitAuthorizationRequired  = (1 << 3);
// clang-format on

// The opcode identifies the protocol method being invoked.
using OpCode = uint8_t;

// The flag bits of an ATT opcode. Bits 0-5 identify the protocol method.
constexpr OpCode kAuthenticationSignatureFlag = (1 << 7);
constexpr OpCode kCommandFlag = (1 << 6);

// The length of an authentication signature used in a signed PDU.
constexpr size_t kAuthenticationSignatureLength = 12;

// The maximum number of write requests that can be queued for submission in a
// ATT Prepare Write Request.
constexpr uint8_t kPrepareQueueMaxCapacity = 20;

enum class MethodType {
  kInvalid,
  kRequest,
  kResponse,
  kCommand,
  kNotification,
  kIndication,
  kConfirmation,
};

struct Header {
  OpCode opcode;
} __attribute__((packed));

enum class ErrorCode : uint8_t {
  kInvalidHandle = 0x01,
  kReadNotPermitted = 0x02,
  kWriteNotPermitted = 0x03,
  kInvalidPDU = 0x04,
  kInsufficientAuthentication = 0x05,
  kRequestNotSupported = 0x06,
  kInvalidOffset = 0x07,
  kInsufficientAuthorization = 0x08,
  kPrepareQueueFull = 0x09,
  kAttributeNotFound = 0x0A,
  kAttributeNotLong = 0x0B,
  kInsufficientEncryptionKeySize = 0x0C,
  kInvalidAttributeValueLength = 0x0D,
  kUnlikelyError = 0x0E,
  kInsufficientEncryption = 0x0F,
  kUnsupportedGroupType = 0x10,
  kInsufficientResources = 0x11,
  kValueNotAllowed = 0x13,
};

// Many ATT protocol PDUs allow using both a 16-bit and a 128-bit representation
// for the attribute type (which is a Bluetooth UUID).
//
// The assigned values can be used in a Find Information Response.
enum class UUIDType : uint8_t {
  k16Bit = 0x01,
  k128Bit = 0x02,
};

template <UUIDType Type>
using AttributeType = typename std::conditional<Type == UUIDType::k16Bit, uint16_t, UInt128>::type;

using AttributeType16 = AttributeType<UUIDType::k16Bit>;
using AttributeType128 = AttributeType<UUIDType::k128Bit>;

enum class ExecuteWriteFlag : uint8_t {
  kCancelAll = 0x00,
  kWritePending = 0x01,
};

struct AttributeData {
  AttributeData() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(AttributeData);

  Handle handle;
  uint8_t value[];
} __attribute__((packed));

// ============== ATT PDUs ==============
constexpr OpCode kInvalidOpCode = 0x00;

// ==============
// Error Handling
constexpr OpCode kErrorResponse = 0x01;
struct ErrorResponseParams {
  OpCode request_opcode;
  Handle attribute_handle;
  ErrorCode error_code;
} __attribute__((packed));

// ============
// MTU Exchange
constexpr OpCode kExchangeMTURequest = 0x02;
constexpr OpCode kExchangeMTUResponse = 0x03;

struct ExchangeMTURequestParams {
  uint16_t client_rx_mtu;
} __attribute__((packed));

struct ExchangeMTUResponseParams {
  uint16_t server_rx_mtu;
} __attribute__((packed));

// ================
// Find Information
constexpr OpCode kFindInformationRequest = 0x04;
constexpr OpCode kFindInformationResponse = 0x05;

struct FindInformationRequestParams {
  Handle start_handle;
  Handle end_handle;
} __attribute__((packed));

struct FindInformationResponseParams {
  UUIDType format;

  // The type of the next member depends on the type of |format|.
  // If type == InformationDataFormat::kUUID16:
  // InformationData16 information_data[];
  //
  // If type == InformationDataFormat::kUUID28:
  // InformationData128 information_data[];
} __attribute__((packed));

template <UUIDType Format>
struct InformationData {
  Handle handle;
  AttributeType<Format> uuid;
} __attribute__((packed));

using InformationData16 = InformationData<UUIDType::k16Bit>;
using InformationData128 = InformationData<UUIDType::k128Bit>;

// ==================
// Find By Type Value
constexpr OpCode kFindByTypeValueRequest = 0x06;
constexpr OpCode kFindByTypeValueResponse = 0x07;

struct FindByTypeValueRequestParams {
  FindByTypeValueRequestParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(FindByTypeValueRequestParams);

  Handle start_handle;
  Handle end_handle;
  AttributeType16 type;
  uint8_t value[];
} __attribute__((packed));

struct HandlesInformationList {
  Handle handle;
  Handle group_end_handle;
} __attribute__((packed));

struct FindByTypeValueResponseParams {
  // Contains at least 1 entry
  HandlesInformationList handles_information_list[1];
} __attribute__((packed));

// ============
// Read By Type
constexpr OpCode kReadByTypeRequest = 0x08;
constexpr OpCode kReadByTypeResponse = 0x09;

// (see Vol 3, Part F, 3.4.4.2)
constexpr uint8_t kMaxReadByTypeValueLength = 253;

template <UUIDType Format>
struct ReadByTypeRequestParams {
  Handle start_handle;
  Handle end_handle;
  AttributeType<Format> type;
} __attribute__((packed));

using ReadByTypeRequestParams16 = ReadByTypeRequestParams<UUIDType::k16Bit>;
using ReadByTypeRequestParams128 = ReadByTypeRequestParams<UUIDType::k128Bit>;

struct ReadByTypeResponseParams {
  ReadByTypeResponseParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ReadByTypeResponseParams);

  uint8_t length;
  AttributeData attribute_data_list[];
} __attribute__((packed));

// ====
// Read
constexpr OpCode kReadRequest = 0x0A;
constexpr OpCode kReadResponse = 0x0B;

struct ReadRequestParams {
  Handle handle;
} __attribute__((packed));

// The Read Response PDU contains the attribute value requested.

// =========
// Read Blob
constexpr OpCode kReadBlobRequest = 0x0C;
constexpr OpCode kReadBlobResponse = 0x0D;

struct ReadBlobRequestParams {
  Handle handle;
  uint16_t offset;
} __attribute__((packed));

// The Read Blob Response PDU contains the partial attribute value requested.

// =============
// Read Multiple
constexpr OpCode kReadMultipleRequest = 0x0E;
constexpr OpCode kReadMultipleResponse = 0x0F;

// The Read Multiple Request PDU contains 2 or more attribute handles.
// The Read Multiple Response PDU contains attribute values concatenated in the
// order requested.

// ==================
// Read By Group Type
constexpr OpCode kReadByGroupTypeRequest = 0x10;
constexpr OpCode kReadByGroupTypeResponse = 0x11;

// (see Vol 3, Part F, 3.4.4.10)
constexpr uint8_t kMaxReadByGroupTypeValueLength = 251;

// The Read By Group Type and Read By Type requests use identical payloads.
using ReadByGroupTypeRequestParams16 = ReadByTypeRequestParams16;
using ReadByGroupTypeRequestParams128 = ReadByTypeRequestParams128;

struct AttributeGroupDataEntry {
  AttributeGroupDataEntry() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(AttributeGroupDataEntry);

  Handle start_handle;
  Handle group_end_handle;
  uint8_t value[];
} __attribute__((packed));

struct ReadByGroupTypeResponseParams {
  ReadByGroupTypeResponseParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ReadByGroupTypeResponseParams);

  uint8_t length;
  AttributeGroupDataEntry attribute_data_list[];
} __attribute__((packed));

// =====
// Write
constexpr OpCode kWriteRequest = 0x12;
constexpr OpCode kWriteCommand = 0x52;
constexpr OpCode kSignedWriteCommand = 0xD2;
constexpr OpCode kWriteResponse = 0x13;

using WriteRequestParams = AttributeData;

// =============
// Prepare Write
constexpr OpCode kPrepareWriteRequest = 0x16;
constexpr OpCode kPrepareWriteResponse = 0x17;

struct PrepareWriteRequestParams {
  PrepareWriteRequestParams() = default;
  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(PrepareWriteRequestParams);

  Handle handle;
  uint16_t offset;
  uint8_t part_value[];
} __attribute__((packed));

using PrepareWriteResponseParams = PrepareWriteRequestParams;

// =============
// Execute Write
constexpr OpCode kExecuteWriteRequest = 0x18;
constexpr OpCode kExecuteWriteResponse = 0x19;

struct ExecuteWriteRequestParams {
  ExecuteWriteFlag flags;
} __attribute__((packed));

// =========================
// Handle Value Notification
constexpr OpCode kNotification = 0x1B;
using NotificationParams = AttributeData;

// =========================
// Handle Value Indication
constexpr OpCode kIndication = 0x1D;
constexpr OpCode kConfirmation = 0x1E;
using IndicationParams = NotificationParams;

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ATT_H_
