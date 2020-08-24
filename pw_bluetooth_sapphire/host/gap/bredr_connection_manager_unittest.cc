// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_connection_manager.h"

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/lib/fxl/memory/ref_ptr.h"

namespace bt {
namespace gap {
namespace {

using bt::hci::AuthRequirements;
using bt::hci::IOCapability;
using bt::testing::CommandTransaction;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

constexpr hci::ConnectionHandle kConnectionHandle = 0x0BAA;
constexpr hci::ConnectionHandle kConnectionHandle2 = 0x0BAB;
const DeviceAddress kLocalDevAddr(DeviceAddress::Type::kBREDR, {0});
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kBREDR, {1});
const DeviceAddress kTestDevAddrLe(DeviceAddress::Type::kLEPublic, {2});
const DeviceAddress kTestDevAddr2(DeviceAddress::Type::kBREDR, {3});
constexpr uint32_t kPasskey = 123456;
const hci::LinkKey kRawKey({0xc0, 0xde, 0xfa, 0x57, 0x4b, 0xad, 0xf0, 0x0d, 0xa7, 0x60, 0x06, 0x1e,
                            0xca, 0x1e, 0xca, 0xfe},
                           0, 0);
const sm::LTK kLinkKey(sm::SecurityProperties(hci::LinkKeyType::kAuthenticatedCombination192),
                       kRawKey);

constexpr BrEdrSecurityRequirements kNoSecurityRequirements{.authentication = false,
                                                            .secure_connections = false};

// A default size for PDUs when generating responses for testing.
const uint16_t PDU_MAX = 0xFFF;

#define TEST_DEV_ADDR_BYTES_LE 0x01, 0x00, 0x00, 0x00, 0x00, 0x00

// clang-format off

const auto kReadScanEnable = CreateStaticByteBuffer(
    LowerBits(hci::kReadScanEnable), UpperBits(hci::kReadScanEnable),
    0x00  // No parameters
);

#define READ_SCAN_ENABLE_RSP(scan_enable)                                    \
  CreateStaticByteBuffer(hci::kCommandCompleteEventCode, 0x05, 0xF0,         \
                                 LowerBits(hci::kReadScanEnable),            \
                                 UpperBits(hci::kReadScanEnable),            \
                                 hci::kSuccess, (scan_enable))

const auto kReadScanEnableRspNone = READ_SCAN_ENABLE_RSP(0x00);
const auto kReadScanEnableRspInquiry = READ_SCAN_ENABLE_RSP(0x01);
const auto kReadScanEnableRspPage = READ_SCAN_ENABLE_RSP(0x02);
const auto kReadScanEnableRspBoth = READ_SCAN_ENABLE_RSP(0x03);

#undef READ_SCAN_ENABLE_RSP

#define WRITE_SCAN_ENABLE_CMD(scan_enable)                               \
  CreateStaticByteBuffer(LowerBits(hci::kWriteScanEnable),               \
                                 UpperBits(hci::kWriteScanEnable), 0x01, \
                                 (scan_enable))

const auto kWriteScanEnableNone = WRITE_SCAN_ENABLE_CMD(0x00);
const auto kWriteScanEnableInq = WRITE_SCAN_ENABLE_CMD(0x01);
const auto kWriteScanEnablePage = WRITE_SCAN_ENABLE_CMD(0x02);
const auto kWriteScanEnableBoth = WRITE_SCAN_ENABLE_CMD(0x03);

#undef WRITE_SCAN_ENABLE_CMD

#define COMMAND_COMPLETE_RSP(opcode)                                         \
  CreateStaticByteBuffer(hci::kCommandCompleteEventCode, 0x04, 0xF0,         \
                                 LowerBits((opcode)), UpperBits((opcode)),   \
                                 hci::kSuccess);

const auto kWriteScanEnableRsp = COMMAND_COMPLETE_RSP(hci::kWriteScanEnable);

const auto kWritePageScanActivity = CreateStaticByteBuffer(
    LowerBits(hci::kWritePageScanActivity),
    UpperBits(hci::kWritePageScanActivity),
    0x04,  // parameter_total_size (4 bytes)
    0x00, 0x08,  // 1.28s interval (R1)
    0x11, 0x00  // 10.625ms window (R1)
);

const auto kWritePageScanActivityRsp =
    COMMAND_COMPLETE_RSP(hci::kWritePageScanActivity);

const auto kWritePageScanType = CreateStaticByteBuffer(
    LowerBits(hci::kWritePageScanType), UpperBits(hci::kWritePageScanType),
    0x01,  // parameter_total_size (1 byte)
    0x01   // Interlaced scan
);

const auto kWritePageScanTypeRsp =
    COMMAND_COMPLETE_RSP(hci::kWritePageScanType);


#define COMMAND_STATUS_RSP(opcode, statuscode)                       \
  CreateStaticByteBuffer(hci::kCommandStatusEventCode, 0x04,         \
                                 (statuscode), 0xF0,                 \
                                 LowerBits((opcode)), UpperBits((opcode)));

// clang-format on

const auto kConnectionRequest = testing::ConnectionRequestPacket(kTestDevAddr);

const auto kAcceptConnectionRequest = testing::AcceptConnectionRequestPacket(kTestDevAddr);

const auto kAcceptConnectionRequestRsp =
    COMMAND_STATUS_RSP(hci::kAcceptConnectionRequest, hci::StatusCode::kSuccess);

const auto kConnectionComplete = testing::ConnectionCompletePacket(kTestDevAddr, kConnectionHandle);

const auto kConnectionCompleteError =
    CreateStaticByteBuffer(hci::kConnectionCompleteEventCode,
                           0x0B,  // parameter_total_size (11 byte payload)
                           hci::StatusCode::kConnectionFailedToBeEstablished,  // status
                           0x00, 0x00,                                         // connection_handle
                           TEST_DEV_ADDR_BYTES_LE,                             // peer address
                           0x01,                                               // link_type (ACL)
                           0x00  // encryption not enabled
    );

const auto kConnectionCompleteCanceled =
    CreateStaticByteBuffer(hci::kConnectionCompleteEventCode,
                           0x0B,  // parameter_total_size (11 byte payload)
                           hci::StatusCode::kUnknownConnectionId,  // status
                           0x00, 0x00,                             // connection_handle
                           TEST_DEV_ADDR_BYTES_LE,                 // peer address
                           0x01,                                   // link_type (ACL)
                           0x00                                    // encryption not enabled
    );

const auto kCreateConnection =
    CreateStaticByteBuffer(LowerBits(hci::kCreateConnection), UpperBits(hci::kCreateConnection),
                           0x0d,                    // parameter_total_size (13 bytes)
                           TEST_DEV_ADDR_BYTES_LE,  // peer address
                           LowerBits(hci::kEnableAllPacketTypes),  // allowable packet types
                           UpperBits(hci::kEnableAllPacketTypes),  // allowable packet types
                           0x02,                                   // page_scan_repetition_mode (R2)
                           0x00,                                   // reserved
                           0x00, 0x00,                             // clock_offset
                           0x00                                    // allow_role_switch (don't)
    );

const auto kCreateConnectionRsp =
    COMMAND_STATUS_RSP(hci::kCreateConnection, hci::StatusCode::kSuccess);

const auto kCreateConnectionRspError =
    COMMAND_STATUS_RSP(hci::kCreateConnection, hci::StatusCode::kConnectionFailedToBeEstablished);

const auto kCreateConnectionCancel = CreateStaticByteBuffer(LowerBits(hci::kCreateConnectionCancel),
                                                            UpperBits(hci::kCreateConnectionCancel),
                                                            0x06,  // parameter_total_size (6 bytes)
                                                            TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kCreateConnectionCancelRsp = COMMAND_COMPLETE_RSP(hci::kCreateConnectionCancel);

const auto kRemoteNameRequest =
    CreateStaticByteBuffer(LowerBits(hci::kRemoteNameRequest), UpperBits(hci::kRemoteNameRequest),
                           0x0a,                    // parameter_total_size (10 bytes)
                           TEST_DEV_ADDR_BYTES_LE,  // peer address
                           0x00,                    // page_scan_repetition_mode (R0)
                           0x00,                    // reserved
                           0x00, 0x00               // clock_offset
    );
const auto kRemoteNameRequestRsp =
    COMMAND_STATUS_RSP(hci::kRemoteNameRequest, hci::StatusCode::kSuccess);

const auto kRemoteNameRequestComplete = testing::RemoteNameRequestCompletePacket(
    kTestDevAddr, {'F',    'u',    'c',    'h',    's',    'i',    'a',    '\xF0', '\x9F',
                   '\x92', '\x96', '\x00', '\x14', '\x15', '\x16', '\x17', '\x18', '\x19',
                   '\x1a', '\x1b', '\x1c', '\x1d', '\x1e', '\x1f', '\x20'}
    // remote name (Fuchsia💖)
    // Everything after the 0x00 should be ignored.
);
const auto kReadRemoteVersionInfo = CreateStaticByteBuffer(LowerBits(hci::kReadRemoteVersionInfo),
                                                           UpperBits(hci::kReadRemoteVersionInfo),
                                                           0x02,  // Parameter_total_size (2 bytes)
                                                           0xAA, 0x0B  // connection_handle
);

const auto kReadRemoteVersionInfoRsp =
    COMMAND_STATUS_RSP(hci::kReadRemoteVersionInfo, hci::StatusCode::kSuccess);

const auto kRemoteVersionInfoComplete =
    CreateStaticByteBuffer(hci::kReadRemoteVersionInfoCompleteEventCode,
                           0x08,                       // parameter_total_size (8 bytes)
                           hci::StatusCode::kSuccess,  // status
                           0xAA, 0x0B,                 // connection_handle
                           hci::HCIVersion::k4_2,      // lmp_version
                           0xE0, 0x00,                 // manufacturer_name (Google)
                           0xAD, 0xDE                  // lmp_subversion (anything)
    );

const auto kReadRemoteSupportedFeatures = CreateStaticByteBuffer(
    LowerBits(hci::kReadRemoteSupportedFeatures), UpperBits(hci::kReadRemoteSupportedFeatures),
    0x02,       // parameter_total_size (2 bytes)
    0xAA, 0x0B  // connection_handle
);

const auto kReadRemoteSupportedFeaturesRsp =
    COMMAND_STATUS_RSP(hci::kReadRemoteSupportedFeatures, hci::StatusCode::kSuccess);

const auto kReadRemoteSupportedFeaturesComplete =
    CreateStaticByteBuffer(hci::kReadRemoteSupportedFeaturesCompleteEventCode,
                           0x0B,                       // parameter_total_size (11 bytes)
                           hci::StatusCode::kSuccess,  // status
                           0xAA, 0x0B,                 // connection_handle,
                           0xFF, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x80
                           // lmp_features
                           // Set: 3 slot packets, 5 slot packets, Encryption, Timing Accuracy,
                           // Role Switch, Hold Mode, Sniff Mode, LE Supported, Extended Features
    );

const auto kReadRemoteExtended1 = CreateStaticByteBuffer(
    LowerBits(hci::kReadRemoteExtendedFeatures), UpperBits(hci::kReadRemoteExtendedFeatures),
    0x03,        // parameter_total_size (3 bytes)
    0xAA, 0x0B,  // connection_handle
    0x01         // page_number (1)
);

const auto kReadRemoteExtendedFeaturesRsp =
    COMMAND_STATUS_RSP(hci::kReadRemoteExtendedFeatures, hci::StatusCode::kSuccess);

const auto kReadRemoteExtended1Complete =
    CreateStaticByteBuffer(hci::kReadRemoteExtendedFeaturesCompleteEventCode,
                           0x0D,                       // parameter_total_size (13 bytes)
                           hci::StatusCode::kSuccess,  // status
                           0xAA, 0x0B,                 // connection_handle,
                           0x01,                       // page_number
                           0x03,                       // max_page_number (3 pages)
                           0x0F, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00
                           // lmp_features
                           // Set: Secure Simple Pairing (Host Support), LE Supported (Host),
                           //  SimultaneousLEAndBREDR, Secure Connections (Host Support)
    );

const auto kReadRemoteExtended2 = CreateStaticByteBuffer(
    LowerBits(hci::kReadRemoteExtendedFeatures), UpperBits(hci::kReadRemoteExtendedFeatures),
    0x03,        // parameter_total_size (3 bytes)
    0xAA, 0x0B,  // connection_handle
    0x02         // page_number (2)
);

const auto kReadRemoteExtended2Complete =
    CreateStaticByteBuffer(hci::kReadRemoteExtendedFeaturesCompleteEventCode,
                           0x0D,                       // parameter_total_size (13 bytes)
                           hci::StatusCode::kSuccess,  // status
                           0xAA, 0x0B,                 // connection_handle,
                           0x02,                       // page_number
                           0x03,                       // max_page_number (3 pages)
                           0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0xFF, 0x00
                           // lmp_features  - All the bits should be ignored.
    );

const auto kDisconnect = testing::DisconnectPacket(kConnectionHandle);

const auto kDisconnectRsp = COMMAND_STATUS_RSP(hci::kDisconnect, hci::StatusCode::kSuccess);

const auto kDisconnectionComplete =
    CreateStaticByteBuffer(hci::kDisconnectionCompleteEventCode,
                           0x04,                       // parameter_total_size (4 bytes)
                           hci::StatusCode::kSuccess,  // status
                           0xAA, 0x0B,                 // connection_handle
                           0x13                        // Reason (Remote User Terminated Connection)
    );

const auto kAuthenticationRequested = testing::AuthenticationRequestedPacket(kConnectionHandle);

const auto kAuthenticationRequestedStatus =
    COMMAND_STATUS_RSP(hci::kAuthenticationRequested, hci::StatusCode::kSuccess);

const auto kAuthenticationComplete = CreateStaticByteBuffer(hci::kAuthenticationCompleteEventCode,
                                                            0x03,  // parameter_total_size (3 bytes)
                                                            hci::StatusCode::kSuccess,  // status
                                                            0xAA, 0x0B  // connection_handle
);

const auto kAuthenticationCompleteFailed =
    CreateStaticByteBuffer(hci::kAuthenticationCompleteEventCode,
                           0x03,                                 // parameter_total_size (3 bytes)
                           hci::StatusCode::kPairingNotAllowed,  // status
                           0xAA, 0x0B                            // connection_handle
    );

const auto kLinkKeyRequest = CreateStaticByteBuffer(hci::kLinkKeyRequestEventCode,
                                                    0x06,  // parameter_total_size (6 bytes)
                                                    TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kLinkKeyRequestNegativeReply = CreateStaticByteBuffer(
    LowerBits(hci::kLinkKeyRequestNegativeReply), UpperBits(hci::kLinkKeyRequestNegativeReply),
    0x06,                   // parameter_total_size (6 bytes)
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kLinkKeyRequestNegativeReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kLinkKeyRequestNegativeReply),
    UpperBits(hci::kLinkKeyRequestNegativeReply),
    hci::kSuccess,          // status
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

auto MakeIoCapabilityResponse(IOCapability io_cap, AuthRequirements auth_req) {
  return CreateStaticByteBuffer(hci::kIOCapabilityResponseEventCode,
                                0x09,                    // parameter_total_size (9 bytes)
                                TEST_DEV_ADDR_BYTES_LE,  // address
                                io_cap,
                                0x00,  // OOB authentication data not present
                                auth_req);
}

const auto kIoCapabilityRequest = CreateStaticByteBuffer(hci::kIOCapabilityRequestEventCode,
                                                         0x06,  // parameter_total_size (6 bytes)
                                                         TEST_DEV_ADDR_BYTES_LE  // address
);

auto MakeIoCapabilityRequestReply(IOCapability io_cap, AuthRequirements auth_req) {
  return CreateStaticByteBuffer(LowerBits(hci::kIOCapabilityRequestReply),
                                UpperBits(hci::kIOCapabilityRequestReply),
                                0x09,                    // parameter_total_size (9 bytes)
                                TEST_DEV_ADDR_BYTES_LE,  // peer address
                                io_cap,
                                0x00,  // No OOB data present
                                auth_req);
}

const auto kIoCapabilityRequestReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kIOCapabilityRequestReply),
    UpperBits(hci::kIOCapabilityRequestReply),
    hci::kSuccess,          // status
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kIoCapabilityRequestNegativeReply =
    CreateStaticByteBuffer(LowerBits(hci::kIOCapabilityRequestNegativeReply),
                           UpperBits(hci::kIOCapabilityRequestNegativeReply),
                           0x07,                    // parameter_total_size (7 bytes)
                           TEST_DEV_ADDR_BYTES_LE,  // peer address
                           hci::StatusCode::kPairingNotAllowed);

const auto kIoCapabilityRequestNegativeReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kIOCapabilityRequestNegativeReply),
    UpperBits(hci::kIOCapabilityRequestNegativeReply),
    hci::kSuccess,            // status
    TEST_DEV_ADDR_BYTES_LE);  // peer address

auto MakeUserConfirmationRequest(uint32_t passkey) {
  const auto passkey_bytes = ToBytes(kPasskey);
  return CreateStaticByteBuffer(hci::kUserConfirmationRequestEventCode,
                                0x0A,                    // parameter_total_size (10 byte payload)
                                TEST_DEV_ADDR_BYTES_LE,  // peer address
                                passkey_bytes[0], passkey_bytes[1], passkey_bytes[2],
                                0x00  // numeric value
  );
}

const auto kUserConfirmationRequestReply = CreateStaticByteBuffer(
    LowerBits(hci::kUserConfirmationRequestReply), UpperBits(hci::kUserConfirmationRequestReply),
    0x06,                   // parameter_total_size (6 bytes)
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kUserConfirmationRequestReplyRsp =
    COMMAND_COMPLETE_RSP(hci::kUserConfirmationRequestReply);

const auto kUserConfirmationRequestNegativeReply =
    CreateStaticByteBuffer(LowerBits(hci::kUserConfirmationRequestNegativeReply),
                           UpperBits(hci::kUserConfirmationRequestNegativeReply),
                           0x06,                   // parameter_total_size (6 bytes)
                           TEST_DEV_ADDR_BYTES_LE  // peer address
    );

const auto kUserConfirmationRequestNegativeReplyRsp =
    COMMAND_COMPLETE_RSP(hci::kUserConfirmationRequestNegativeReply);

const auto kSimplePairingCompleteSuccess =
    CreateStaticByteBuffer(hci::kSimplePairingCompleteEventCode,
                           0x07,                   // parameter_total_size (7 byte payload)
                           0x00,                   // status (success)
                           TEST_DEV_ADDR_BYTES_LE  // peer address
    );

const auto kSimplePairingCompleteError =
    CreateStaticByteBuffer(hci::kSimplePairingCompleteEventCode,
                           0x07,                   // parameter_total_size (7 byte payload)
                           0x05,                   // status (authentication failure)
                           TEST_DEV_ADDR_BYTES_LE  // peer address
    );

DynamicByteBuffer MakeLinkKeyNotification(hci::LinkKeyType key_type) {
  return DynamicByteBuffer(StaticByteBuffer(hci::kLinkKeyNotificationEventCode,
                                            0x17,  // parameter_total_size (17 bytes)
                                            TEST_DEV_ADDR_BYTES_LE,  // peer address
                                            0xc0, 0xde, 0xfa, 0x57, 0x4b, 0xad, 0xf0, 0x0d, 0xa7,
                                            0x60, 0x06, 0x1e, 0xca, 0x1e, 0xca, 0xfe,  // link key
                                            static_cast<uint8_t>(key_type)             // key type
                                            ));
}

const auto kLinkKeyNotification =
    MakeLinkKeyNotification(hci::LinkKeyType::kAuthenticatedCombination192);

const auto kLinkKeyRequestReply = CreateStaticByteBuffer(
    LowerBits(hci::kLinkKeyRequestReply), UpperBits(hci::kLinkKeyRequestReply),
    0x16,                    // parameter_total_size (22 bytes)
    TEST_DEV_ADDR_BYTES_LE,  // peer address
    0xc0, 0xde, 0xfa, 0x57, 0x4b, 0xad, 0xf0, 0x0d, 0xa7, 0x60, 0x06, 0x1e, 0xca, 0x1e, 0xca,
    0xfe  // link key
);

const auto kLinkKeyRequestReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kLinkKeyRequestReply),
    UpperBits(hci::kLinkKeyRequestReply),
    hci::kSuccess,          // status
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

const auto kLinkKeyNotificationChanged =
    CreateStaticByteBuffer(hci::kLinkKeyNotificationEventCode,
                           0x17,                    // parameter_total_size (17 bytes)
                           TEST_DEV_ADDR_BYTES_LE,  // peer address
                           0xfa, 0xce, 0xb0, 0x0c, 0xa5, 0x1c, 0xcd, 0x15, 0xea, 0x5e, 0xfe, 0xdb,
                           0x1d, 0x0d, 0x0a, 0xd5,  // link key
                           0x06                     // key type (Changed Combination Key)
    );

const auto kLinkKeyRequestReplyChanged = CreateStaticByteBuffer(
    LowerBits(hci::kLinkKeyRequestReply), UpperBits(hci::kLinkKeyRequestReply),
    0x16,                    // parameter_total_size (22 bytes)
    TEST_DEV_ADDR_BYTES_LE,  // peer address
    0xfa, 0xce, 0xb0, 0x0c, 0xa5, 0x1c, 0xcd, 0x15, 0xea, 0x5e, 0xfe, 0xdb, 0x1d, 0x0d, 0x0a,
    0xd5  // link key
);

const auto kSetConnectionEncryption = CreateStaticByteBuffer(
    LowerBits(hci::kSetConnectionEncryption), UpperBits(hci::kSetConnectionEncryption),
    0x03,        // parameter total size
    0xAA, 0x0B,  // connection handle
    0x01         // encryption enable
);

const auto kSetConnectionEncryptionRsp =
    COMMAND_STATUS_RSP(hci::kSetConnectionEncryption, hci::StatusCode::kSuccess);

const auto kEncryptionChangeEvent = CreateStaticByteBuffer(hci::kEncryptionChangeEventCode,
                                                           4,           // parameter total size
                                                           0x00,        // status
                                                           0xAA, 0x0B,  // connection handle
                                                           0x01         // encryption enabled
);

const auto kReadEncryptionKeySize = CreateStaticByteBuffer(LowerBits(hci::kReadEncryptionKeySize),
                                                           UpperBits(hci::kReadEncryptionKeySize),
                                                           0x02,       // parameter size
                                                           0xAA, 0x0B  // connection handle
);

const auto kReadEncryptionKeySizeRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode,
    0x07,  // parameters total size
    0xFF,  // num command packets allowed (255)
    LowerBits(hci::kReadEncryptionKeySize), UpperBits(hci::kReadEncryptionKeySize),
    hci::kSuccess,  // status
    0xAA, 0x0B,     // connection handle
    0x10            // encryption key size: 16
);

auto MakeUserPasskeyRequestReply(uint32_t passkey) {
  const auto passkey_bytes = ToBytes(kPasskey);
  return CreateStaticByteBuffer(LowerBits(hci::kUserPasskeyRequestReply),
                                UpperBits(hci::kUserPasskeyRequestReply),
                                0x0A,                    // parameter_total_size (10 bytes)
                                TEST_DEV_ADDR_BYTES_LE,  // peer address
                                passkey_bytes[0], passkey_bytes[1], passkey_bytes[2],
                                0x00  // numeric value
  );
}

const auto kUserPasskeyRequestReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kUserPasskeyRequestReply),
    UpperBits(hci::kUserPasskeyRequestReply),
    hci::kSuccess,          // status
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

auto MakeUserPasskeyNotification(uint32_t passkey) {
  const auto passkey_bytes = ToBytes(kPasskey);
  return CreateStaticByteBuffer(hci::kUserPasskeyNotificationEventCode,
                                0x0A,                    // parameter_total_size (10 byte payload)
                                TEST_DEV_ADDR_BYTES_LE,  // peer address
                                passkey_bytes[0], passkey_bytes[1], passkey_bytes[2],
                                0x00  // numeric value
  );
}

const hci::DataBufferInfo kBrEdrBufferInfo(1024, 1);
const hci::DataBufferInfo kLeBufferInfo(1024, 1);

constexpr l2cap::ChannelParameters kChannelParams;

class BrEdrConnectionManagerTest : public TestingBase {
 public:
  BrEdrConnectionManagerTest() = default;
  ~BrEdrConnectionManagerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel(kBrEdrBufferInfo, kLeBufferInfo);

    peer_cache_ = std::make_unique<PeerCache>();
    data_domain_ = data::testing::FakeDomain::Create();

    connection_manager_ = std::make_unique<BrEdrConnectionManager>(
        transport()->WeakPtr(), peer_cache_.get(), kLocalDevAddr, data_domain_, true);

    StartTestDevice();

    test_device()->SetTransactionCallback([this] { transaction_count_++; },
                                          async_get_default_dispatcher());
  }

  void TearDown() override {
    // Don't trigger the transaction callback when cleaning up the manager.
    test_device()->ClearTransactionCallback();
    if (connection_manager_ != nullptr) {
      // deallocating the connection manager disables connectivity.
      EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
      EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);
      connection_manager_ = nullptr;
    }
    RunLoopUntilIdle();
    test_device()->Stop();
    data_domain_ = nullptr;
    peer_cache_ = nullptr;
    TestingBase::TearDown();
  }

 protected:
  static constexpr const int kIncomingConnTransactions = 6;
  static constexpr const int kDisconnectionTransactions = 1;

  BrEdrConnectionManager* connmgr() const { return connection_manager_.get(); }
  void SetConnectionManager(std::unique_ptr<BrEdrConnectionManager> mgr) {
    connection_manager_ = std::move(mgr);
  }

  PeerCache* peer_cache() const { return peer_cache_.get(); }

  data::testing::FakeDomain* data_domain() const { return data_domain_.get(); }

  int transaction_count() const { return transaction_count_; }

  // Add expectations and simulated responses for the outbound commands sent
  // after an inbound Connection Request Event is received. Results in
  // |kIncomingConnTransactions| transactions.

  void QueueSuccessfulIncomingConn(DeviceAddress addr = kTestDevAddr,
                                   hci::ConnectionHandle handle = kConnectionHandle) const {
    const auto connection_complete = testing::ConnectionCompletePacket(addr, handle);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::AcceptConnectionRequestPacket(addr),
                          &kAcceptConnectionRequestRsp, &connection_complete);
    QueueSuccessfulInterrogation(addr, handle);
  }

  void QueueSuccessfulCreateConnection(Peer* peer, hci::ConnectionHandle conn) const {
    const DynamicByteBuffer complete_packet =
        testing::ConnectionCompletePacket(peer->address(), conn);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::CreateConnectionPacket(peer->address()),
                          &kCreateConnectionRsp, &complete_packet);
  }

  void QueueSuccessfulInterrogation(DeviceAddress addr, hci::ConnectionHandle conn) const {
    const DynamicByteBuffer remote_name_complete_packet =
        testing::RemoteNameRequestCompletePacket(addr);
    const DynamicByteBuffer remote_version_complete_packet =
        testing::ReadRemoteVersionInfoCompletePacket(conn);
    const DynamicByteBuffer remote_supported_complete_packet =
        testing::ReadRemoteSupportedFeaturesCompletePacket(conn, true);
    const DynamicByteBuffer remote_extended1_complete_packet =
        testing::ReadRemoteExtended1CompletePacket(conn);
    const DynamicByteBuffer remote_extended2_complete_packet =
        testing::ReadRemoteExtended2CompletePacket(conn);

    EXPECT_CMD_PACKET_OUT(test_device(), testing::RemoteNameRequestPacket(addr),
                          &kRemoteNameRequestRsp, &remote_name_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(conn),
                          &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteSupportedFeaturesPacket(conn),
                          &kReadRemoteSupportedFeaturesRsp, &remote_supported_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteExtended1Packet(conn),
                          &kReadRemoteExtendedFeaturesRsp, &remote_extended1_complete_packet);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteExtended2Packet(conn),
                          &kReadRemoteExtendedFeaturesRsp, &remote_extended2_complete_packet);
  }

  void QueueSuccessfulPairing(
      hci::LinkKeyType key_type = hci::LinkKeyType::kAuthenticatedCombination192) {
    EXPECT_CMD_PACKET_OUT(test_device(), kAuthenticationRequested, &kAuthenticationRequestedStatus,
                          &kLinkKeyRequest);
    EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply,
                          &kLinkKeyRequestNegativeReplyRsp, &kIoCapabilityRequest);
    const auto kIoCapabilityResponse = MakeIoCapabilityResponse(
        IOCapability::kDisplayYesNo, AuthRequirements::kMITMGeneralBonding);
    const auto kUserConfirmationRequest = MakeUserConfirmationRequest(kPasskey);
    EXPECT_CMD_PACKET_OUT(test_device(),
                          MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                       AuthRequirements::kMITMGeneralBonding),
                          &kIoCapabilityRequestReplyRsp, &kIoCapabilityResponse,
                          &kUserConfirmationRequest);
    const auto kLinkKeyNotificationWithKeyType = MakeLinkKeyNotification(key_type);
    EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                          &kUserConfirmationRequestReplyRsp, &kSimplePairingCompleteSuccess,
                          &kLinkKeyNotificationWithKeyType, &kAuthenticationComplete);
    EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                          &kEncryptionChangeEvent);
    EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);
  }

  // Use when pairing with no IO, where authenticated pairing is not possible.
  void QueueSuccessfulUnauthenticatedPairing(
      hci::LinkKeyType key_type = hci::LinkKeyType::kUnauthenticatedCombination192) {
    EXPECT_CMD_PACKET_OUT(test_device(), kAuthenticationRequested, &kAuthenticationRequestedStatus,
                          &kLinkKeyRequest);
    EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply,
                          &kLinkKeyRequestNegativeReplyRsp, &kIoCapabilityRequest);
    const auto kIoCapabilityReply = MakeIoCapabilityRequestReply(IOCapability::kNoInputNoOutput,
                                                                 AuthRequirements::kGeneralBonding);
    const auto kIoCapabilityResponse =
        MakeIoCapabilityResponse(IOCapability::kNoInputNoOutput, AuthRequirements::kGeneralBonding);
    const auto kUserConfirmationRequest = MakeUserConfirmationRequest(kPasskey);
    EXPECT_CMD_PACKET_OUT(test_device(), kIoCapabilityReply, &kIoCapabilityRequestReplyRsp,
                          &kIoCapabilityResponse, &kUserConfirmationRequest);
    const auto kLinkKeyNotificationWithKeyType = MakeLinkKeyNotification(key_type);
    // User Confirmation Request Reply will be automatic due to no IO.
    EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                          &kUserConfirmationRequestReplyRsp, &kSimplePairingCompleteSuccess,
                          &kLinkKeyNotificationWithKeyType, &kAuthenticationComplete);
    EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                          &kEncryptionChangeEvent);
    EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);
  }

  void QueueDisconnection(
      hci::ConnectionHandle conn,
      hci::StatusCode reason = hci::StatusCode::kRemoteUserTerminatedConnection) const {
    const DynamicByteBuffer disconnect_complete =
        testing::DisconnectionCompletePacket(conn, reason);
    EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(conn, reason), &kDisconnectRsp,
                          &disconnect_complete);
  }

 private:
  std::unique_ptr<BrEdrConnectionManager> connection_manager_;
  std::unique_ptr<PeerCache> peer_cache_;
  fbl::RefPtr<data::testing::FakeDomain> data_domain_;
  int transaction_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrConnectionManagerTest);
};

using GAP_BrEdrConnectionManagerTest = BrEdrConnectionManagerTest;

TEST_F(GAP_BrEdrConnectionManagerTest, DisableConnectivity) {
  size_t cb_count = 0;
  auto cb = [&cb_count](const auto& status) {
    cb_count++;
    EXPECT_TRUE(status);
  };

  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspPage);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableNone, &kWriteScanEnableRsp);

  connmgr()->SetConnectable(false, cb);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, cb_count);

  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);

  connmgr()->SetConnectable(false, cb);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, cb_count);
}

TEST_F(GAP_BrEdrConnectionManagerTest, EnableConnectivity) {
  size_t cb_count = 0;
  auto cb = [&cb_count](const auto& status) {
    cb_count++;
    EXPECT_TRUE(status);
  };

  EXPECT_CMD_PACKET_OUT(test_device(), kWritePageScanActivity, &kWritePageScanActivityRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kWritePageScanType, &kWritePageScanTypeRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspNone);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnablePage, &kWriteScanEnableRsp);

  connmgr()->SetConnectable(true, cb);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, cb_count);

  EXPECT_CMD_PACKET_OUT(test_device(), kWritePageScanActivity, &kWritePageScanActivityRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kWritePageScanType, &kWritePageScanTypeRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspInquiry);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableBoth, &kWriteScanEnableRsp);

  connmgr()->SetConnectable(true, cb);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, cb_count);
}

// Test: An incoming connection request should trigger an acceptance and
// interrogation should allow a peer that only report the first Extended
// Features page.
TEST_F(GAP_BrEdrConnectionManagerTest, IncomingConnection_BrokenExtendedPageResponse) {
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kRemoteNameRequest, &kRemoteNameRequestRsp,
                        &kRemoteNameRequestComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteVersionInfo, &kReadRemoteVersionInfoRsp,
                        &kRemoteVersionInfoComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeatures,
                        &kReadRemoteSupportedFeaturesRsp, &kReadRemoteSupportedFeaturesComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended1, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended1Complete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended2, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended1Complete);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(6, transaction_count());

  // When we deallocate the connection manager next, we should disconnect.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnect, &kDisconnectRsp, &kDisconnectionComplete);

  // deallocating the connection manager disables connectivity.
  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);

  SetConnectionManager(nullptr);

  RunLoopUntilIdle();

  EXPECT_EQ(9, transaction_count());
}

// Test: An incoming connection request should trigger an acceptance and an
// interrogation to discover capabilities.
TEST_F(GAP_BrEdrConnectionManagerTest, IncomingConnectionSuccess) {
  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle));

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));
  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  // When we deallocate the connection manager next, we should disconnect.
  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnect, &kDisconnectRsp, &kDisconnectionComplete);

  // deallocating the connection manager disables connectivity.
  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);

  SetConnectionManager(nullptr);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 3, transaction_count());
}

// Test: An incoming connection request should upgrade a known LE peer with a
// matching address to a dual mode peer.
TEST_F(GAP_BrEdrConnectionManagerTest, IncomingConnectionUpgradesKnownLowEnergyPeerToDualMode) {
  const DeviceAddress le_alias_addr(DeviceAddress::Type::kLEPublic, kTestDevAddr.value());
  Peer* const peer = peer_cache()->NewPeer(le_alias_addr, true);
  ASSERT_TRUE(peer);
  ASSERT_EQ(TechnologyType::kLowEnergy, peer->technology());

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  ASSERT_EQ(peer, peer_cache()->FindByAddress(kTestDevAddr));
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());

  // Prepare for disconnection upon teardown.
  QueueDisconnection(kConnectionHandle);
}

// Test: A remote disconnect should correctly remove the connection.
TEST_F(GAP_BrEdrConnectionManagerTest, RemoteDisconnect) {
  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle));
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);
  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  // Remote end disconnects.
  test_device()->SendCommandChannelPacket(kDisconnectionComplete);

  RunLoopUntilIdle();

  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle));

  // deallocating the connection manager disables connectivity.
  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);

  SetConnectionManager(nullptr);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 2, transaction_count());
}

const auto kRemoteNameRequestCompleteFailed =
    CreateStaticByteBuffer(hci::kRemoteNameRequestCompleteEventCode,
                           0x01,  // parameter_total_size (1 bytes)
                           hci::StatusCode::kHardwareFailure);

const auto kReadRemoteSupportedFeaturesCompleteFailed =
    CreateStaticByteBuffer(hci::kReadRemoteSupportedFeaturesCompleteEventCode,
                           0x01,  // parameter_total_size (1 bytes)
                           hci::StatusCode::kHardwareFailure);

// Test: if the interrogation fails, we disconnect.
//  - Receiving extra responses after a command fails will not fail
//  - We don't query extended features if we don't receive an answer.
TEST_F(GAP_BrEdrConnectionManagerTest, IncomingConnectionFailedInterrogation) {
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kRemoteNameRequest, &kRemoteNameRequestRsp,
                        &kRemoteNameRequestCompleteFailed);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteVersionInfo, &kReadRemoteVersionInfoRsp,
                        &kRemoteVersionInfoComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeatures,
                        &kReadRemoteSupportedFeaturesRsp,
                        &kReadRemoteSupportedFeaturesCompleteFailed);

  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnect, &kDisconnectRsp, &kDisconnectionComplete);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(5, transaction_count());
}

// Test: replies negative to IO Capability Requests before PairingDelegate is set
TEST_F(GAP_BrEdrConnectionManagerTest, IoCapabilityRequestNegativeReplyWithNoPairingDelegate) {
  EXPECT_CMD_PACKET_OUT(test_device(), kIoCapabilityRequestNegativeReply,
                        &kIoCapabilityRequestNegativeReplyRsp);

  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count());
}

// Test: replies negative to IO Capability Requests for unconnected peers
TEST_F(GAP_BrEdrConnectionManagerTest, IoCapabilityRequestNegativeReplyWhenNotConnected) {
  FakePairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(), kIoCapabilityRequestNegativeReply,
                        &kIoCapabilityRequestNegativeReplyRsp);

  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count());
}

// Test: replies to IO Capability Requests for connected peers
TEST_F(GAP_BrEdrConnectionManagerTest, IoCapabilityRequestReplyWhenConnected) {
  FakePairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  ASSERT_EQ(kIncomingConnTransactions, transaction_count());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kNoInputNoOutput,
                                                     AuthRequirements::kGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayOnly, AuthRequirements::kMITMGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  QueueDisconnection(kConnectionHandle);
}

// Test: Responds to Secure Simple Pairing with user rejection of Numeric Comparison association
TEST_F(GAP_BrEdrConnectionManagerTest, RespondToNumericComparisonPairingAfterUserRejects) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayOnly, AuthRequirements::kGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        EXPECT_EQ(kPasskey, passkey);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kComparison, method);
        ASSERT_TRUE(confirm_cb);
        confirm_cb(false);
      });

  EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestNegativeReply,
                        &kUserConfirmationRequestNegativeReplyRsp);
  test_device()->SendCommandChannelPacket(MakeUserConfirmationRequest(kPasskey));

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_EQ(sm::Status(HostError::kFailed), status); });

  test_device()->SendCommandChannelPacket(kSimplePairingCompleteError);

  // We disconnect the peer when authentication fails.
  QueueDisconnection(kConnectionHandle);

  RunLoopUntilIdle();
}

const auto kUserPasskeyRequest =
    CreateStaticByteBuffer(hci::kUserPasskeyRequestEventCode,
                           0x06,                   // parameter_total_size (6 byte payload)
                           TEST_DEV_ADDR_BYTES_LE  // peer address
    );

const auto kUserPasskeyRequestNegativeReply =
    CreateStaticByteBuffer(LowerBits(hci::kUserPasskeyRequestNegativeReply),
                           UpperBits(hci::kUserPasskeyRequestNegativeReply),
                           0x06,                   // parameter_total_size (6 bytes)
                           TEST_DEV_ADDR_BYTES_LE  // peer address
    );

const auto kUserPasskeyRequestNegativeReplyRsp = CreateStaticByteBuffer(
    hci::kCommandCompleteEventCode, 0x0A, 0xF0, LowerBits(hci::kUserPasskeyRequestNegativeReply),
    UpperBits(hci::kUserPasskeyRequestNegativeReply),
    hci::kSuccess,          // status
    TEST_DEV_ADDR_BYTES_LE  // peer address
);

// Test: Responds to Secure Simple Pairing as the input side of Passkey Entry association after the
// user declines or provides invalid input
TEST_F(GAP_BrEdrConnectionManagerTest,
       RespondToPasskeyEntryPairingAfterUserProvidesInvalidPasskey) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kKeyboardOnly);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kKeyboardOnly,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayOnly, AuthRequirements::kGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  pairing_delegate.SetRequestPasskeyCallback([](PeerId, auto response_cb) {
    ASSERT_TRUE(response_cb);
    response_cb(-128);  // Negative values indicate rejection.
  });

  EXPECT_CMD_PACKET_OUT(test_device(), kUserPasskeyRequestNegativeReply,
                        &kUserPasskeyRequestNegativeReplyRsp);
  test_device()->SendCommandChannelPacket(kUserPasskeyRequest);

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_EQ(sm::Status(HostError::kFailed), status); });

  test_device()->SendCommandChannelPacket(kSimplePairingCompleteError);

  // We disconnect the peer when authentication fails.
  QueueDisconnection(kConnectionHandle);

  RunLoopUntilIdle();
}

// Test: replies negative to Link Key Requests for unknown and unbonded peers
TEST_F(GAP_BrEdrConnectionManagerTest, LinkKeyRequestAndNegativeReply) {
  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply,
                        &kLinkKeyRequestNegativeReplyRsp);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(1, transaction_count());

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply,
                        &kLinkKeyRequestNegativeReplyRsp);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 2, transaction_count());

  QueueDisconnection(kConnectionHandle);
}

// Test: replies to Link Key Requests for bonded peer
TEST_F(GAP_BrEdrConnectionManagerTest, RecallLinkKeyForBondedPeer) {
  ASSERT_TRUE(
      peer_cache()->AddBondedPeer(BondingData{PeerId(999), kTestDevAddr, {}, {}, kLinkKey}));
  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_FALSE(peer->connected());
  ASSERT_TRUE(peer->bonded());

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  ASSERT_TRUE(peer->connected());

  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestReply, &kLinkKeyRequestReplyRsp);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  QueueDisconnection(kConnectionHandle);
}

// Test: Responds to Secure Simple Pairing as the input side of Passkey Entry association after the
// user provides the correct passkey
TEST_F(GAP_BrEdrConnectionManagerTest,
       EncryptAfterPasskeyEntryPairingAndUserProvidesAcceptedPasskey) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kKeyboardOnly);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kKeyboardOnly,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayOnly, AuthRequirements::kGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  pairing_delegate.SetRequestPasskeyCallback([](PeerId, auto response_cb) {
    ASSERT_TRUE(response_cb);
    response_cb(kPasskey);
  });

  EXPECT_CMD_PACKET_OUT(test_device(), MakeUserPasskeyRequestReply(kPasskey),
                        &kUserPasskeyRequestReplyRsp);
  test_device()->SendCommandChannelPacket(kUserPasskeyRequest);

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  test_device()->SendCommandChannelPacket(kSimplePairingCompleteSuccess);
  test_device()->SendCommandChannelPacket(kLinkKeyNotification);

  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);

  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(peer->bonded());

  QueueDisconnection(kConnectionHandle);
}

// Test: Responds to Secure Simple Pairing as the display side of Passkey Entry association after
// the user provides the correct passkey on the peer
TEST_F(GAP_BrEdrConnectionManagerTest, EncryptAfterPasskeyDisplayPairing) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayOnly,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kKeyboardOnly, AuthRequirements::kGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        EXPECT_EQ(kPasskey, passkey);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kPeerEntry, method);
        EXPECT_TRUE(confirm_cb);
      });

  test_device()->SendCommandChannelPacket(MakeUserPasskeyNotification(kPasskey));

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  test_device()->SendCommandChannelPacket(kSimplePairingCompleteSuccess);
  test_device()->SendCommandChannelPacket(kLinkKeyNotification);

  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);

  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(peer->bonded());

  QueueDisconnection(kConnectionHandle);
}

// Test: Responds to Secure Simple Pairing and user confirmation of Numeric Comparison association,
// then bonds and encrypts using resulting link key
TEST_F(GAP_BrEdrConnectionManagerTest, EncryptAndBondAfterNumericComparisonPairingAndUserConfirms) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp);

  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayYesNo, AuthRequirements::kGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        EXPECT_EQ(kPasskey, passkey);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kComparison, method);
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                        &kUserConfirmationRequestReplyRsp);
  test_device()->SendCommandChannelPacket(MakeUserConfirmationRequest(kPasskey));

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  test_device()->SendCommandChannelPacket(kSimplePairingCompleteSuccess);
  test_device()->SendCommandChannelPacket(kLinkKeyNotification);

  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);

  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(peer->bonded());

  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestReply, &kLinkKeyRequestReplyRsp);
  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  QueueDisconnection(kConnectionHandle);
}

// Test: can't change the link key of an unbonded peer
TEST_F(GAP_BrEdrConnectionManagerTest, UnbondedPeerChangeLinkKey) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  // Change the link key.
  test_device()->SendCommandChannelPacket(kLinkKeyNotificationChanged);

  RunLoopUntilIdle();
  EXPECT_FALSE(peer->bonded());

  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply, &kLinkKeyRequestReplyRsp);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  EXPECT_FALSE(peer->bonded());
  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  QueueDisconnection(kConnectionHandle);
}

const auto kLinkKeyNotificationLegacy =
    CreateStaticByteBuffer(hci::kLinkKeyNotificationEventCode,
                           0x17,                    // parameter_total_size (17 bytes)
                           TEST_DEV_ADDR_BYTES_LE,  // peer address
                           0x41, 0x33, 0x7c, 0x0d, 0xef, 0xee, 0xda, 0xda, 0xba, 0xad, 0x0f, 0xf1,
                           0xce, 0xc0, 0xff, 0xee,  // link key
                           0x00                     // key type (Combination Key)
    );

// Test: don't bond if the link key resulted from legacy pairing
TEST_F(GAP_BrEdrConnectionManagerTest, LegacyLinkKeyNotBonded) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->connected());
  ASSERT_FALSE(peer->bonded());

  test_device()->SendCommandChannelPacket(kLinkKeyNotificationLegacy);

  RunLoopUntilIdle();
  EXPECT_FALSE(peer->bonded());

  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply, &kLinkKeyRequestReplyRsp);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  RunLoopUntilIdle();

  EXPECT_FALSE(peer->bonded());
  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  QueueDisconnection(kConnectionHandle);
}

// Test: if L2CAP gets a link error, we disconnect the connection
TEST_F(GAP_BrEdrConnectionManagerTest, DisconnectOnLinkError) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  // When we deallocate the connection manager next, we should disconnect.
  QueueDisconnection(kConnectionHandle);

  data_domain()->TriggerLinkError(kConnectionHandle);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());

  EXPECT_CMD_PACKET_OUT(test_device(), kReadScanEnable, &kReadScanEnableRspBoth);
  EXPECT_CMD_PACKET_OUT(test_device(), kWriteScanEnableInq, &kWriteScanEnableRsp);

  SetConnectionManager(nullptr);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 3, transaction_count());
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectedPeerTimeout) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_TRUE(peer->connected());

  // We want to make sure the connection doesn't expire.
  RunLoopFor(zx::sec(600));

  // Remote end disconnects.
  test_device()->SendCommandChannelPacket(kDisconnectionComplete);

  RunLoopUntilIdle();

  // Peer should still be there, but not connected anymore
  peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_FALSE(peer->connected());
  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle));
}

TEST_F(GAP_BrEdrConnectionManagerTest, ServiceSearch) {
  size_t search_cb_count = 0;
  auto search_cb = [&](auto id, const auto& attributes) {
    auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
    ASSERT_TRUE(peer);
    ASSERT_EQ(id, peer->identifier());
    ASSERT_EQ(1u, attributes.count(sdp::kServiceId));
    search_cb_count++;
  };

  auto search_id =
      connmgr()->AddServiceSearch(sdp::profile::kAudioSink, {sdp::kServiceId}, search_cb);

  fbl::RefPtr<l2cap::testing::FakeChannel> sdp_chan;
  std::optional<uint32_t> sdp_request_tid;

  data_domain()->set_channel_callback([&sdp_chan, &sdp_request_tid](auto new_chan) {
    new_chan->SetSendCallback(
        [&sdp_request_tid](auto packet) {
          const auto kSearchExpectedParams = CreateStaticByteBuffer(
              // ServiceSearchPattern
              0x35, 0x03,        // Sequence uint8 3 bytes
              0x19, 0x11, 0x0B,  // UUID (kAudioSink)
              0xFF, 0xFF,        // MaxAttributeByteCount (no max)
              // Attribute ID list
              0x35, 0x03,        // Sequence uint8 3 bytes
              0x09, 0x00, 0x03,  // uint16_t (kServiceId)
              0x00               // No continuation state
          );
          // First byte should be type.
          ASSERT_LE(3u, packet->size());
          ASSERT_EQ(sdp::kServiceSearchAttributeRequest, (*packet)[0]);
          ASSERT_EQ(kSearchExpectedParams, packet->view(5));
          sdp_request_tid = (*packet)[1] << 8 || (*packet)[2];
        },
        async_get_default_dispatcher());
    sdp_chan = std::move(new_chan);
  });

  QueueSuccessfulIncomingConn();
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kSDP, 0x40, 0x41,
                                            kChannelParams);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  ASSERT_TRUE(sdp_chan);
  ASSERT_TRUE(sdp_request_tid);
  ASSERT_EQ(0u, search_cb_count);

  sdp::ServiceSearchAttributeResponse rsp;
  rsp.SetAttribute(0, sdp::kServiceId, sdp::DataElement(UUID()));
  auto rsp_ptr =
      rsp.GetPDU(0xFFFF /* max attribute bytes */, *sdp_request_tid, PDU_MAX, BufferView());

  sdp_chan->Receive(*rsp_ptr);

  RunLoopUntilIdle();

  ASSERT_EQ(1u, search_cb_count);

  // Remote end disconnects.
  test_device()->SendCommandChannelPacket(kDisconnectionComplete);

  RunLoopUntilIdle();

  sdp_request_tid.reset();

  EXPECT_TRUE(connmgr()->RemoveServiceSearch(search_id));
  EXPECT_FALSE(connmgr()->RemoveServiceSearch(search_id));

  // Second connection is shortened because we have already interrogated,
  // and we don't search for SDP services because none are registered
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended1, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended1Complete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended2, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended2Complete);

  test_device()->SendCommandChannelPacket(kConnectionRequest);
  RunLoopUntilIdle();

  // We shouldn't have searched for anything.
  ASSERT_FALSE(sdp_request_tid);
  ASSERT_EQ(1u, search_cb_count);

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest, SearchOnReconnect) {
  size_t search_cb_count = 0;
  auto search_cb = [&](auto id, const auto& attributes) {
    auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
    ASSERT_TRUE(peer);
    ASSERT_EQ(id, peer->identifier());
    ASSERT_EQ(1u, attributes.count(sdp::kServiceId));
    search_cb_count++;
  };

  connmgr()->AddServiceSearch(sdp::profile::kAudioSink, {sdp::kServiceId}, search_cb);

  fbl::RefPtr<l2cap::testing::FakeChannel> sdp_chan;
  std::optional<uint32_t> sdp_request_tid;

  data_domain()->set_channel_callback([&sdp_chan, &sdp_request_tid](auto new_chan) {
    new_chan->SetSendCallback(
        [&sdp_request_tid](auto packet) {
          const auto kSearchExpectedParams = CreateStaticByteBuffer(
              // ServiceSearchPattern
              0x35, 0x03,        // Sequence uint8 3 bytes
              0x19, 0x11, 0x0B,  // UUID (kAudioSink)
              0xFF, 0xFF,        // MaxAttributeByteCount (no max)
              // Attribute ID list
              0x35, 0x03,        // Sequence uint8 3 bytes
              0x09, 0x00, 0x03,  // uint16_t (kServiceId)
              0x00               // No continuation state
          );
          // First byte should be type.
          ASSERT_LE(3u, packet->size());
          ASSERT_EQ(sdp::kServiceSearchAttributeRequest, (*packet)[0]);
          ASSERT_EQ(kSearchExpectedParams, packet->view(5));
          sdp_request_tid = (*packet)[1] << 8 || (*packet)[2];
        },
        async_get_default_dispatcher());
    sdp_chan = std::move(new_chan);
  });

  // This test uses a modified peer and interrogation which doesn't use
  // extended pages.
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  const DynamicByteBuffer remote_name_complete_packet =
      testing::RemoteNameRequestCompletePacket(kTestDevAddr);
  const DynamicByteBuffer remote_version_complete_packet =
      testing::ReadRemoteVersionInfoCompletePacket(kConnectionHandle);
  const DynamicByteBuffer remote_supported_complete_packet =
      testing::ReadRemoteSupportedFeaturesCompletePacket(kConnectionHandle, false);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::RemoteNameRequestPacket(kTestDevAddr),
                        &kRemoteNameRequestRsp, &remote_name_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::ReadRemoteVersionInfoPacket(kConnectionHandle),
                        &kReadRemoteVersionInfoRsp, &remote_version_complete_packet);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        testing::ReadRemoteSupportedFeaturesPacket(kConnectionHandle),
                        &kReadRemoteSupportedFeaturesRsp, &remote_supported_complete_packet);

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kSDP, 0x40, 0x41,
                                            kChannelParams);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  ASSERT_TRUE(sdp_chan);
  ASSERT_TRUE(sdp_request_tid);
  ASSERT_EQ(0u, search_cb_count);

  sdp::ServiceSearchAttributeResponse rsp;
  rsp.SetAttribute(0, sdp::kServiceId, sdp::DataElement(UUID()));
  auto rsp_ptr =
      rsp.GetPDU(0xFFFF /* max attribute bytes */, *sdp_request_tid, PDU_MAX, BufferView());

  sdp_chan->Receive(*rsp_ptr);

  RunLoopUntilIdle();

  ASSERT_EQ(1u, search_cb_count);

  // Remote end disconnects.
  test_device()->SendCommandChannelPacket(kDisconnectionComplete);

  RunLoopUntilIdle();

  sdp_request_tid.reset();
  sdp_chan = nullptr;

  // Second connection is shortened because we have already interrogated.
  // We still search for SDP services.
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  // We don't send any interrogation packets, because there is none to be done.

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kSDP, 0x40, 0x41,
                                            kChannelParams);

  test_device()->SendCommandChannelPacket(kConnectionRequest);
  RunLoopUntilIdle();

  // We should have searched again.
  ASSERT_TRUE(sdp_chan);
  ASSERT_TRUE(sdp_request_tid);
  ASSERT_EQ(1u, search_cb_count);

  rsp_ptr = rsp.GetPDU(0xFFFF /* max attribute bytes */, *sdp_request_tid, PDU_MAX, BufferView());

  sdp_chan->Receive(*rsp_ptr);

  RunLoopUntilIdle();

  ASSERT_EQ(2u, search_cb_count);

  QueueDisconnection(kConnectionHandle);
}

// Test: when opening an L2CAP channel on an unbonded peer, indicate that we have no link key then
// pair, authenticate, bond, and encrypt the link, then try to open the channel.
TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capPairsAndEncryptsThenRetries) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  std::optional<fbl::RefPtr<l2cap::Channel>> connected_chan;

  auto chan_cb = [&](auto chan) { connected_chan = std::move(chan); };

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Approve pairing requests.
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Initial connection request

  // Pairing initiation and flow that results in bonding then encryption, but verifying the strength
  // of the encryption key doesn't complete
  EXPECT_CMD_PACKET_OUT(test_device(), kAuthenticationRequested, &kAuthenticationRequestedStatus,
                        &kLinkKeyRequest);
  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestNegativeReply,
                        &kLinkKeyRequestNegativeReplyRsp, &kIoCapabilityRequest);
  const auto kIoCapabilityResponse =
      MakeIoCapabilityResponse(IOCapability::kDisplayYesNo, AuthRequirements::kMITMGeneralBonding);
  const auto kUserConfirmationRequest = MakeUserConfirmationRequest(kPasskey);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp, &kIoCapabilityResponse,
                        &kUserConfirmationRequest);
  EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                        &kUserConfirmationRequestReplyRsp, &kSimplePairingCompleteSuccess,
                        &kLinkKeyNotification, &kAuthenticationComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, );

  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, chan_cb);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // We should not have a channel because the L2CAP open callback shouldn't have been called, but
  // the LTK should be stored since the link key got received.
  ASSERT_FALSE(connected_chan);

  test_device()->SendCommandChannelPacket(kReadEncryptionKeySizeRsp);

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kAVDTP, 0x40, 0x41,
                                            kChannelParams);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // The socket should be returned.
  ASSERT_TRUE(connected_chan);

  connected_chan.reset();

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kAVDTP, 0x40, 0x41,
                                            kChannelParams);

  // A second connection request should not require another authentication.
  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, chan_cb);

  RunLoopUntilIdle();

  ASSERT_TRUE(connected_chan);

  QueueDisconnection(kConnectionHandle);
}

// Test: when the peer is already bonded, the link key gets stored when it is provided to the
// connection.
TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capEncryptsForBondedPeerThenRetries) {
  ASSERT_TRUE(
      peer_cache()->AddBondedPeer(BondingData{PeerId(999), kTestDevAddr, {}, {}, kLinkKey}));
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_FALSE(peer->connected());
  ASSERT_TRUE(peer->bonded());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  ASSERT_TRUE(peer->bredr()->connected());

  std::optional<fbl::RefPtr<l2cap::Channel>> connected_chan;

  auto socket_cb = [&](auto chan) { connected_chan = std::move(chan); };

  // Initial connection request

  // Note: this skips some parts of the pairing flow, because the link key being
  // received is the important part of this. The key is not received when the
  // authentication fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kAuthenticationRequested, &kAuthenticationRequestedStatus);

  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, socket_cb);

  RunLoopUntilIdle();

  // L2CAP connect shouldn't have been called, and callback shouldn't be called.
  // We should not have a socket.
  ASSERT_FALSE(connected_chan);

  // The authentication flow will request the existing link key, which should be
  // returned and stored, and then the authentication is complete.
  EXPECT_CMD_PACKET_OUT(test_device(), kLinkKeyRequestReply, &kLinkKeyRequestReplyRsp,
                        &kAuthenticationComplete);

  test_device()->SendCommandChannelPacket(kLinkKeyRequest);

  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, );

  RunLoopUntilIdle();

  // No socket until the encryption verification completes.
  ASSERT_FALSE(connected_chan);

  test_device()->SendCommandChannelPacket(kReadEncryptionKeySizeRsp);

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kAVDTP, 0x40, 0x41,
                                            kChannelParams);

  RunLoopUntilIdle();

  // The socket should be connected.
  ASSERT_TRUE(connected_chan);

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest,
       OpenL2capAuthenticationFailureReturnsInvalidSocketAndDisconnects) {
  QueueSuccessfulIncomingConn();

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  std::optional<fbl::RefPtr<l2cap::Channel>> connected_chan;

  auto socket_cb = [&](auto chan) { connected_chan = std::move(chan); };

  // Initial connection request

  // Note: this skips some parts of the pairing flow, because the link key being
  // received is the important part of this. The key is not received when the
  // authentication fails.
  EXPECT_CMD_PACKET_OUT(test_device(), kAuthenticationRequested, &kAuthenticationRequestedStatus);

  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, socket_cb);

  RunLoopUntilIdle();

  // The L2CAP shouldn't have been called
  // We should not have a channel, and the callback shouldn't have been called.
  ASSERT_FALSE(connected_chan);

  test_device()->SendCommandChannelPacket(kAuthenticationCompleteFailed);

  int count = transaction_count();

  // We disconnect the peer when authentication fails.
  QueueDisconnection(kConnectionHandle);

  RunLoopUntilIdle();

  // An invalid channel should have been sent because the connection failed.
  ASSERT_TRUE(connected_chan);
  ASSERT_EQ(connected_chan.value(), nullptr);

  ASSERT_EQ(count + kDisconnectionTransactions, transaction_count());
}

// Test: when pairing is in progress, opening an L2CAP channel waits for the pairing to complete
// before retrying.
TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capDuringPairingWaitsForPairingToComplete) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  std::optional<fbl::RefPtr<l2cap::Channel>> connected_chan;

  auto socket_cb = [&](auto chan) { connected_chan = std::move(chan); };

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Approve pairing requests
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Initiate pairing from the peer
  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayYesNo, AuthRequirements::kMITMGeneralBonding));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Initial connection request

  // Pair and bond as the responder. Note that Authentication Requested is not sent even though we
  // are opening the L2CAP channel because the peer started pairing first.
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);
  const auto kUserConfirmationRequest = MakeUserConfirmationRequest(kPasskey);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp, &kUserConfirmationRequest);
  EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                        &kUserConfirmationRequestReplyRsp, &kSimplePairingCompleteSuccess,
                        &kLinkKeyNotification);
  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, );

  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, socket_cb);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // We should not have a socket because the L2CAP open callback shouldn't have been called, but
  // the LTK should be stored since the link key got received.
  ASSERT_FALSE(connected_chan);

  test_device()->SendCommandChannelPacket(kReadEncryptionKeySizeRsp);

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kAVDTP, 0x40, 0x41,
                                            kChannelParams);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // The socket should be returned.
  ASSERT_TRUE(connected_chan);

  QueueDisconnection(kConnectionHandle);
}

// Test: when pairing is in progress, opening an L2CAP channel waits for the pairing to complete
// before retrying.
TEST_F(GAP_BrEdrConnectionManagerTest, InterrogationInProgressAllowsBondingButNotL2cap) {
  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Trigger inbound connection and respond to some (but not all) of interrogation.
  EXPECT_CMD_PACKET_OUT(test_device(), kAcceptConnectionRequest, &kAcceptConnectionRequestRsp,
                        &kConnectionComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kRemoteNameRequest, &kRemoteNameRequestRsp,
                        &kRemoteNameRequestComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteVersionInfo, &kReadRemoteVersionInfoRsp,
                        &kRemoteVersionInfoComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeatures,
                        &kReadRemoteSupportedFeaturesRsp);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  // Ensure that the interrogation has begun but the peer hasn't yet bonded
  EXPECT_EQ(4, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_FALSE(peer->bredr()->connected());
  ASSERT_FALSE(peer->bredr()->bonded());

  // Approve pairing requests
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Initiate pairing from the peer before interrogation completes
  test_device()->SendCommandChannelPacket(
      MakeIoCapabilityResponse(IOCapability::kDisplayYesNo, AuthRequirements::kMITMGeneralBonding));
  test_device()->SendCommandChannelPacket(kIoCapabilityRequest);
  const auto kUserConfirmationRequest = MakeUserConfirmationRequest(kPasskey);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        MakeIoCapabilityRequestReply(IOCapability::kDisplayYesNo,
                                                     AuthRequirements::kMITMGeneralBonding),
                        &kIoCapabilityRequestReplyRsp, &kUserConfirmationRequest);
  EXPECT_CMD_PACKET_OUT(test_device(), kUserConfirmationRequestReply,
                        &kUserConfirmationRequestReplyRsp, &kSimplePairingCompleteSuccess,
                        &kLinkKeyNotification);
  EXPECT_CMD_PACKET_OUT(test_device(), kSetConnectionEncryption, &kSetConnectionEncryptionRsp,
                        &kEncryptionChangeEvent);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadEncryptionKeySize, &kReadEncryptionKeySizeRsp);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // At this point the peer is bonded and the link is encrypted but interrogation has not completed
  // so host-side L2CAP should still be inactive on this link (though it may be buffering packets).
  EXPECT_FALSE(data_domain()->IsLinkConnected(kConnectionHandle));

  bool socket_cb_called = false;
  auto socket_fails_cb = [&socket_cb_called](auto chan_sock) {
    EXPECT_FALSE(chan_sock);
    socket_cb_called = true;
  };
  connmgr()->OpenL2capChannel(peer->identifier(), l2cap::kAVDTP, kNoSecurityRequirements,
                              kChannelParams, socket_fails_cb);

  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(socket_cb_called);

  // Complete interrogation successfully.
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended1, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended1Complete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteExtended2, &kReadRemoteExtendedFeaturesRsp,
                        &kReadRemoteExtended1Complete);
  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesComplete);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_TRUE(data_domain()->IsLinkConnected(kConnectionHandle));

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectUnknownPeer) {
  EXPECT_FALSE(connmgr()->Connect(PeerId(456), {}));
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectLowEnergyPeer) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddrLe, true);
  EXPECT_FALSE(connmgr()->Connect(peer->identifier(), {}));
}

TEST_F(GAP_BrEdrConnectionManagerTest, DisconnectUnknownPeerDoesNothing) {
  EXPECT_TRUE(connmgr()->Disconnect(PeerId(999)));

  RunLoopUntilIdle();

  EXPECT_EQ(0, transaction_count());
}

// Test: user-initiated disconnection
TEST_F(GAP_BrEdrConnectionManagerTest, DisconnectClosesHciConnection) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  QueueDisconnection(kConnectionHandle);

  EXPECT_TRUE(connmgr()->Disconnect(peer->identifier()));
  EXPECT_FALSE(peer->bredr()->connected());

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());
  EXPECT_FALSE(peer->bredr()->connected());
}

TEST_F(GAP_BrEdrConnectionManagerTest, DisconnectSamePeerIsIdempotent) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  QueueDisconnection(kConnectionHandle);

  EXPECT_TRUE(connmgr()->Disconnect(peer->identifier()));
  EXPECT_FALSE(peer->bredr()->connected());

  // Try to disconnect again while the first disconnect is in progress (HCI
  // Disconnection Complete not yet received).
  EXPECT_TRUE(connmgr()->Disconnect(peer->identifier()));

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());
  EXPECT_FALSE(peer->bredr()->connected());

  // Try to disconnect once more, now that the link is gone.
  EXPECT_TRUE(connmgr()->Disconnect(peer->identifier()));
}

TEST_F(GAP_BrEdrConnectionManagerTest, RemovePeerFromPeerCacheDuringDisconnection) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());

  QueueDisconnection(kConnectionHandle);

  const PeerId id = peer->identifier();
  EXPECT_TRUE(connmgr()->Disconnect(id));
  ASSERT_FALSE(peer->bredr()->connected());

  // Remove the peer from PeerCache before receiving HCI Disconnection Complete.
  EXPECT_TRUE(peer_cache()->RemoveDisconnectedPeer(id));

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions + 1, transaction_count());
  EXPECT_FALSE(peer_cache()->FindById(id));
  EXPECT_FALSE(peer_cache()->FindByAddress(kTestDevAddr));
}

TEST_F(GAP_BrEdrConnectionManagerTest, AddServiceSearchAll) {
  size_t search_cb_count = 0;
  auto search_cb = [&](auto id, const auto&) {
    auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
    ASSERT_TRUE(peer);
    ASSERT_EQ(id, peer->identifier());
    search_cb_count++;
  };

  connmgr()->AddServiceSearch(sdp::profile::kAudioSink, {}, search_cb);

  fbl::RefPtr<l2cap::testing::FakeChannel> sdp_chan;
  std::optional<uint32_t> sdp_request_tid;

  data_domain()->set_channel_callback([&sdp_chan, &sdp_request_tid](auto new_chan) {
    new_chan->SetSendCallback(
        [&sdp_request_tid](auto packet) {
          const auto kSearchExpectedParams = CreateStaticByteBuffer(
              // ServiceSearchPattern
              0x35, 0x03,        // Sequence uint8 3 bytes
              0x19, 0x11, 0x0B,  // UUID (kAudioSink)
              0xFF, 0xFF,        // MaxAttributeByteCount (none)
              // Attribute ID list
              0x35, 0x05,                    // Sequence uint8 5 bytes
              0x0A, 0x00, 0x00, 0xFF, 0xFF,  // uint32_t (all attributes)
              0x00                           // No continuation state
          );
          // First byte should be type.
          ASSERT_LE(3u, packet->size());
          ASSERT_EQ(sdp::kServiceSearchAttributeRequest, (*packet)[0]);
          ASSERT_EQ(kSearchExpectedParams, packet->view(5));
          sdp_request_tid = (*packet)[1] << 8 || (*packet)[2];
        },
        async_get_default_dispatcher());
    sdp_chan = std::move(new_chan);
  });

  QueueSuccessfulIncomingConn();
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kSDP, 0x40, 0x41,
                                            kChannelParams);

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  ASSERT_TRUE(sdp_chan);
  ASSERT_TRUE(sdp_request_tid);
  ASSERT_EQ(0u, search_cb_count);

  sdp::ServiceSearchAttributeResponse rsp;
  rsp.SetAttribute(0, sdp::kServiceId, sdp::DataElement(UUID()));
  auto rsp_ptr =
      rsp.GetPDU(0xFFFF /* max attribute bytes */, *sdp_request_tid, PDU_MAX, BufferView());

  sdp_chan->Receive(*rsp_ptr);

  RunLoopUntilIdle();

  ASSERT_EQ(1u, search_cb_count);

  QueueDisconnection(kConnectionHandle);
}

std::string FormatConnectionState(Peer::ConnectionState s) {
  switch (s) {
    case Peer::ConnectionState::kConnected:
      return "kConnected";
    case Peer::ConnectionState::kInitializing:
      return "kInitializing";
    case Peer::ConnectionState::kNotConnected:
      return "kNotConnected";
  }
  return "<Invalid state>";
}

::testing::AssertionResult IsInitializing(Peer* peer) {
  if (Peer::ConnectionState::kInitializing != peer->bredr()->connection_state()) {
    return ::testing::AssertionFailure()
           << "Expected peer connection_state: kInitializing, found "
           << FormatConnectionState(peer->bredr()->connection_state());
  }
  return ::testing::AssertionSuccess();
}
::testing::AssertionResult IsConnected(Peer* peer) {
  if (Peer::ConnectionState::kConnected != peer->bredr()->connection_state()) {
    return ::testing::AssertionFailure()
           << "Expected peer connection_state: kConnected, found "
           << FormatConnectionState(peer->bredr()->connection_state());
  }
  if (peer->temporary()) {
    return ::testing::AssertionFailure()
           << "Expected peer to be non-temporary, but found temporary";
  }
  return ::testing::AssertionSuccess();
}
::testing::AssertionResult NotConnected(Peer* peer) {
  if (Peer::ConnectionState::kNotConnected != peer->bredr()->connection_state()) {
    return ::testing::AssertionFailure()
           << "Expected peer connection_state: kNotConnected, found "
           << FormatConnectionState(peer->bredr()->connection_state());
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult HasConnectionTo(Peer* peer, BrEdrConnection* conn) {
  if (!conn) {
    return ::testing::AssertionFailure() << "Expected BrEdrConnection, but found nullptr";
  }
  if (peer->identifier() != conn->peer_id()) {
    return ::testing::AssertionFailure()
           << "Expected connection peer_id " << bt_str(peer->identifier()) << " but found "
           << bt_str(conn->peer_id());
  }
  return ::testing::AssertionSuccess();
}

#define CALLBACK_EXPECT_FAILURE(status_param)       \
  ([&status_param](auto cb_status, auto conn_ref) { \
    EXPECT_FALSE(conn_ref);                         \
    status_param = cb_status;                       \
  })

// An error is received via the HCI Command cb_status event
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerErrorStatus) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRspError);

  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(NotConnected(peer));

  hci::Status status;
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), CALLBACK_EXPECT_FAILURE(status)));
  EXPECT_TRUE(IsInitializing(peer));
  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(hci::StatusCode::kConnectionFailedToBeEstablished, status.protocol_error());
  EXPECT_TRUE(NotConnected(peer));
}

::testing::AssertionResult StatusEqual(hci::StatusCode expected, hci::StatusCode actual) {
  if (expected == actual)
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure()
           << expected << " is '" << StatusCodeToString(expected) << "', " << actual << " is '"
           << StatusCodeToString(actual) << "'";
}

// Connection Complete event reports error
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerFailure) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionCompleteError);

  hci::Status status(HostError::kFailed);
  bool callback_run = false;

  auto callback = [&status, &callback_run](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
    callback_run = true;
  };
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(IsInitializing(peer));

  RunLoopUntilIdle();

  EXPECT_TRUE(callback_run);

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_TRUE(
      StatusEqual(hci::StatusCode::kConnectionFailedToBeEstablished, status.protocol_error()));
  EXPECT_TRUE(NotConnected(peer));
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerTimeout) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnectionCancel, &kCreateConnectionCancelRsp,
                        &kConnectionCompleteCanceled);

  hci::Status status;
  auto callback = [&status](auto cb_status, auto conn_ref) {
    EXPECT_FALSE(conn_ref);
    status = cb_status;
  };

  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(IsInitializing(peer));
  RunLoopFor(kBrEdrCreateConnectionTimeout);
  RunLoopFor(kBrEdrCreateConnectionTimeout);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kTimedOut, status.error()) << status.ToString();
  EXPECT_TRUE(NotConnected(peer));
}

// Successful connection to single peer
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeer) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up the connection
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);
  QueueSuccessfulInterrogation(peer->address(), kConnectionHandle);
  QueueDisconnection(kConnectionHandle);

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(IsInitializing(peer));
  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), hci::Status().ToString());
  EXPECT_TRUE(HasConnectionTo(peer, conn_ref));
  EXPECT_TRUE(IsConnected(peer));
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerFailedInterrogation) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up outbound connection.
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);

  // Queue up most of interrogation.
  EXPECT_CMD_PACKET_OUT(test_device(), kRemoteNameRequest, &kRemoteNameRequestRsp,
                        &kRemoteNameRequestComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteVersionInfo, &kReadRemoteVersionInfoRsp,
                        &kRemoteVersionInfoComplete);
  EXPECT_CMD_PACKET_OUT(test_device(), kReadRemoteSupportedFeatures,
                        &kReadRemoteSupportedFeaturesRsp);

  hci::Status status;
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_FALSE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  RETURN_IF_FATAL(RunLoopUntilIdle());

  test_device()->SendCommandChannelPacket(kReadRemoteSupportedFeaturesCompleteFailed);
  QueueDisconnection(kConnectionHandle);
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kNotSupported, status.error()) << status.ToString();
  EXPECT_TRUE(NotConnected(peer));
}

// Connecting to an already connected peer should complete instantly
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerAlreadyConnected) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up the connection
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);
  QueueSuccessfulInterrogation(peer->address(), kConnectionHandle);
  QueueDisconnection(kConnectionHandle);

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  int num_callbacks = 0;
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref, &num_callbacks](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    ++num_callbacks;
  };

  // Connect to the peer for the first time
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(IsInitializing(peer));
  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), hci::Status().ToString());
  EXPECT_TRUE(HasConnectionTo(peer, conn_ref));
  EXPECT_TRUE(IsConnected(peer));
  EXPECT_EQ(num_callbacks, 1);

  // Attempt to connect again to the already connected peer
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  RunLoopUntilIdle();
  EXPECT_EQ(num_callbacks, 2);
  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), hci::Status().ToString());
  EXPECT_TRUE(HasConnectionTo(peer, conn_ref));
  EXPECT_TRUE(IsConnected(peer));
}

// Initiating Two Connections to the same (currently unconnected) peer should
// successfully establish both
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSinglePeerTwoInFlight) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up the connection
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);
  QueueSuccessfulInterrogation(peer->address(), kConnectionHandle);
  QueueDisconnection(kConnectionHandle);

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  int num_callbacks = 0;
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref, &num_callbacks](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
    ++num_callbacks;
  };

  // Launch one request, but don't run the loop
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  EXPECT_TRUE(IsInitializing(peer));

  // Launch second inflight request
  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));

  // Run the loop which should complete both requests
  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(status.ToString(), hci::Status().ToString());
  EXPECT_TRUE(HasConnectionTo(peer, conn_ref));
  EXPECT_TRUE(IsConnected(peer));
  EXPECT_EQ(num_callbacks, 2);
}

TEST_F(GAP_BrEdrConnectionManagerTest, ConnectSecondPeerFirstTimesOut) {
  auto* peer_a = peer_cache()->NewPeer(kTestDevAddr, true);
  auto* peer_b = peer_cache()->NewPeer(kTestDevAddr2, true);

  // Enqueue first connection request (which will timeout and be cancelled)
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnectionCancel, &kCreateConnectionCancelRsp,
                        &kConnectionCompleteCanceled);

  // Enqueue second connection (which will succeed once previous has ended)
  QueueSuccessfulCreateConnection(peer_b, kConnectionHandle2);
  QueueSuccessfulInterrogation(peer_b->address(), kConnectionHandle2);
  QueueDisconnection(kConnectionHandle2);

  // Initialize as success to verify that |callback_a| assigns failure.
  hci::Status status_a;
  auto callback_a = [&status_a](auto cb_status, auto cb_conn_ref) {
    status_a = cb_status;
    EXPECT_FALSE(cb_conn_ref);
  };

  // Initialize as error to verify that |callback_b| assigns success.
  hci::Status status_b(HostError::kFailed);
  BrEdrConnection* connection = nullptr;
  auto callback_b = [&status_b, &connection](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status_b = cb_status;
    connection = std::move(cb_conn_ref);
  };

  // Launch one request (this will timeout)
  EXPECT_TRUE(connmgr()->Connect(peer_a->identifier(), callback_a));
  ASSERT_TRUE(peer_a->bredr());
  EXPECT_TRUE(IsInitializing(peer_a));

  RunLoopUntilIdle();

  // Launch second inflight request (this will wait for the first)
  EXPECT_TRUE(connmgr()->Connect(peer_b->identifier(), callback_b));
  ASSERT_TRUE(peer_b->bredr());

  // Run the loop which should complete both requests
  RunLoopFor(kBrEdrCreateConnectionTimeout);
  RunLoopFor(kBrEdrCreateConnectionTimeout);

  EXPECT_FALSE(status_a);
  EXPECT_TRUE(status_b);
  EXPECT_EQ(status_b.ToString(), hci::Status().ToString());
  EXPECT_TRUE(HasConnectionTo(peer_b, connection));
  EXPECT_TRUE(NotConnected(peer_a));
  EXPECT_TRUE(IsConnected(peer_b));
}

TEST_F(GAP_BrEdrConnectionManagerTest, DisconnectPendingConnections) {
  auto* peer_a = peer_cache()->NewPeer(kTestDevAddr, true);
  auto* peer_b = peer_cache()->NewPeer(kTestDevAddr2, true);

  // Enqueue first connection request (which will await Connection Complete)
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp);
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnectionCancel, &kCreateConnectionCancelRsp,
                        &kConnectionCompleteCanceled);

  // No-op connection callbacks
  auto callback_a = [](auto, auto) {};
  auto callback_b = [](auto, auto) {};

  // Launch both requests (second one is queued. Neither completes.)
  EXPECT_TRUE(connmgr()->Connect(peer_a->identifier(), callback_a));
  EXPECT_TRUE(connmgr()->Connect(peer_b->identifier(), callback_b));

  // Put the first connection into flight.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  ASSERT_TRUE(IsInitializing(peer_a));
  ASSERT_TRUE(IsInitializing(peer_b));

  EXPECT_FALSE(connmgr()->Disconnect(peer_a->identifier()));
  EXPECT_FALSE(connmgr()->Disconnect(peer_b->identifier()));
}

// If SDP channel creation fails, null channel should be caught and
// not be dereferenced. Search should fail to return results.
TEST_F(GAP_BrEdrConnectionManagerTest, SDPChannelCreationFailsGracefully) {
  constexpr l2cap::ChannelId kLocalCId = 0x40;
  constexpr l2cap::ChannelId kRemoteCId = 0x41;

  // Channel creation should fail.
  data_domain()->set_channel_callback([](auto new_chan) { ASSERT_FALSE(new_chan); });

  // Since SDP channel creation fails, search_cb should not be called by SDP.
  auto search_cb = [&](auto id, const auto& attributes) { FAIL(); };
  connmgr()->AddServiceSearch(sdp::profile::kAudioSink, {sdp::kServiceId}, search_cb);

  QueueSuccessfulIncomingConn();
  data_domain()->set_simulate_open_channel_failure(true);
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, l2cap::kSDP, kLocalCId, kRemoteCId,
                                            kChannelParams);

  test_device()->SendCommandChannelPacket(kConnectionRequest);
  RunLoopUntilIdle();

  // Peer should still connect successfully.
  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));
  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  EXPECT_TRUE(IsConnected(peer));

  test_device()->SendCommandChannelPacket(kDisconnectionComplete);
  RunLoopUntilIdle();

  EXPECT_FALSE(IsConnected(peer));
}

TEST_F(GAP_BrEdrConnectionManagerTest,
       PendingPacketsNotClearedOnDisconnectAndClearedOnDisconnectionCompleteEvent) {
  constexpr size_t kMaxNumPackets = 1;

  ASSERT_EQ(kMaxNumPackets, kBrEdrBufferInfo.max_num_packets());

  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle));
  EXPECT_EQ(kInvalidPeerId, connmgr()->GetPeerId(kConnectionHandle2));

  QueueSuccessfulIncomingConn(kTestDevAddr, kConnectionHandle);
  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));

  QueueSuccessfulIncomingConn(kTestDevAddr2, kConnectionHandle2);
  test_device()->SendCommandChannelPacket(testing::ConnectionRequestPacket(kTestDevAddr2));

  RunLoopUntilIdle();

  auto* peer2 = peer_cache()->FindByAddress(kTestDevAddr2);
  ASSERT_TRUE(peer2);
  EXPECT_EQ(peer2->identifier(), connmgr()->GetPeerId(kConnectionHandle2));

  EXPECT_EQ(2 * kIncomingConnTransactions, transaction_count());

  size_t packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      hci::ACLDataPacket::New(kConnectionHandle, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      hci::ACLDataPacket::New(kConnectionHandle2, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  EXPECT_EQ(1u, packet_count);

  EXPECT_CMD_PACKET_OUT(test_device(), kDisconnect, &kDisconnectRsp);

  EXPECT_TRUE(connmgr()->Disconnect(peer->identifier()));
  RunLoopUntilIdle();

  // Packet for |kConnectionHandle2| should not have been sent before Disconnection Complete event.
  EXPECT_EQ(1u, packet_count);

  test_device()->SendCommandChannelPacket(kDisconnectionComplete);

  RunLoopUntilIdle();

  EXPECT_FALSE(IsConnected(peer));

  // Packet for |kConnectionHandle2| should have been sent.
  EXPECT_EQ(2u, packet_count);

  // Link |kConnectionHandle| should have been unregistered.
  ASSERT_FALSE(acl_data_channel()->SendPacket(
      hci::ACLDataPacket::New(kConnectionHandle, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  QueueDisconnection(kConnectionHandle2);
}

TEST_F(GAP_BrEdrConnectionManagerTest, PairUnconnectedPeer) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());
  ASSERT_EQ(peer_cache()->count(), 1u);
  uint count_cb_called = 0;
  auto cb = [&count_cb_called](hci::Status status) {
    ASSERT_EQ(status.error(), bt::HostError::kNotFound);
    count_cb_called++;
  };
  connmgr()->Pair(peer->identifier(), kNoSecurityRequirements, cb);
  ASSERT_EQ(count_cb_called, 1u);
}

TEST_F(GAP_BrEdrConnectionManagerTest, Pair) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());
  ASSERT_FALSE(peer->bonded());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Approve pairing requests.
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  QueueSuccessfulPairing();

  // Make the pairing error a "bad" error to confirm the callback is called at the end of the
  // pairing process.
  auto pairing_error = HostError::kPacketMalformed;
  auto pairing_complete_cb = [&pairing_error](hci::Status status) {
    ASSERT_TRUE(status);
    pairing_error = status.error();
  };

  connmgr()->Pair(peer->identifier(), kNoSecurityRequirements, pairing_complete_cb);
  ASSERT_FALSE(peer->bonded());
  RunLoopUntilIdle();

  ASSERT_EQ(pairing_error, HostError::kNoError);
  ASSERT_TRUE(peer->bonded());

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest, PairTwice) {
  QueueSuccessfulIncomingConn();

  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  EXPECT_EQ(kIncomingConnTransactions, transaction_count());
  auto* const peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  ASSERT_TRUE(peer->bredr()->connected());
  ASSERT_FALSE(peer->bonded());

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Approve pairing requests.
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) {
        ASSERT_TRUE(confirm_cb);
        confirm_cb(true);
      });

  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  QueueSuccessfulPairing();

  // Make the pairing error a "bad" error to confirm the callback is called at the end of the
  // pairing process.
  auto pairing_error = HostError::kPacketMalformed;
  auto pairing_complete_cb = [&pairing_error](hci::Status status) {
    ASSERT_TRUE(status);
    pairing_error = status.error();
  };

  connmgr()->Pair(peer->identifier(), kNoSecurityRequirements, pairing_complete_cb);
  RunLoopUntilIdle();

  ASSERT_EQ(pairing_error, HostError::kNoError);
  ASSERT_TRUE(peer->bonded());

  pairing_error = HostError::kPacketMalformed;
  connmgr()->Pair(peer->identifier(), kNoSecurityRequirements, pairing_complete_cb);

  // Note that we do not call QueueSuccessfulPairing twice, even though we pair twice - this is to
  // test that pairing on an already-paired link succeeds without sending any messages to the peer.
  RunLoopUntilIdle();
  ASSERT_EQ(pairing_error, HostError::kNoError);
  ASSERT_TRUE(peer->bonded());

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capChannelCreatesChannelWithChannelParameters) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = l2cap::kFirstDynamicChannelId;
  l2cap::ChannelParameters params;
  params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  params.max_rx_sdu_size = l2cap::kMinACLMTU;

  QueueSuccessfulIncomingConn(kTestDevAddr, kConnectionHandle);
  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));

  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate.GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  QueueSuccessfulPairing();
  RunLoopUntilIdle();

  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, kPSM, kLocalId, 0x41, params);

  std::optional<l2cap::ChannelInfo> chan_info;
  size_t sock_cb_count = 0;
  auto sock_cb = [&](auto chan) {
    sock_cb_count++;
    ASSERT_TRUE(chan);
    chan_info = chan->info();
  };
  connmgr()->OpenL2capChannel(peer->identifier(), kPSM, kNoSecurityRequirements, params, sock_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(1u, sock_cb_count);
  ASSERT_TRUE(chan_info);
  EXPECT_EQ(*params.mode, chan_info->mode);
  EXPECT_EQ(*params.max_rx_sdu_size, chan_info->max_rx_sdu_size);

  QueueDisconnection(kConnectionHandle);
}

// Tests that the connection manager cleans up its connection map correctly following a
// disconnection due to encryption failure.
TEST_F(GAP_BrEdrConnectionManagerTest, ConnectionCleanUpFollowingEncryptionFailure) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up the connection
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);
  QueueSuccessfulInterrogation(peer->address(), kConnectionHandle);
  QueueDisconnection(kConnectionHandle, hci::StatusCode::kAuthenticationFailure);

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  RunLoopUntilIdle();
  ASSERT_TRUE(status);

  test_device()->SendCommandChannelPacket(
      testing::EncryptionChangeEventPacket(hci::StatusCode::kConnectionTerminatedMICFailure,
                                           kConnectionHandle, hci::EncryptionStatus::kOff));
  test_device()->SendCommandChannelPacket(testing::DisconnectionCompletePacket(
      kConnectionHandle, hci::StatusCode::kConnectionTerminatedMICFailure));
  RunLoopUntilIdle();

  EXPECT_TRUE(NotConnected(peer));
}

TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capChannelUpgradesLinkKey) {
  QueueSuccessfulIncomingConn(kTestDevAddr, kConnectionHandle);
  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));

  FakePairingDelegate pairing_delegate_no_io(sm::IOCapability::kNoInputNoOutput);
  connmgr()->SetPairingDelegate(pairing_delegate_no_io.GetWeakPtr());
  pairing_delegate_no_io.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  size_t sock_cb_count = 0;
  auto sock_cb = [&](auto chan_sock) {
    sock_cb_count++;
    EXPECT_TRUE(chan_sock);
  };

  // Pairing caused by missing link key.
  QueueSuccessfulUnauthenticatedPairing();

  constexpr auto kPSM0 = l2cap::kHIDControl;
  constexpr l2cap::ChannelId kLocalId0 = l2cap::kFirstDynamicChannelId;
  constexpr l2cap::ChannelId kRemoteId0 = 0x41;
  const BrEdrSecurityRequirements sec_reqs{.authentication = false, .secure_connections = false};
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, kPSM0, kLocalId0, kRemoteId0,
                                            l2cap::ChannelParameters());
  connmgr()->OpenL2capChannel(peer->identifier(), kPSM0, sec_reqs, l2cap::ChannelParameters(),
                              sock_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(1u, sock_cb_count);

  // New pairing delegate with display can support authenticated pairing.
  FakePairingDelegate pairing_delegate_with_display(sm::IOCapability::kDisplayYesNo);
  connmgr()->SetPairingDelegate(pairing_delegate_with_display.GetWeakPtr());
  pairing_delegate_with_display.SetDisplayPasskeyCallback(
      [](PeerId, uint32_t passkey, auto method, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate_with_display.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Pairing caused by insufficient link key.
  QueueSuccessfulPairing();

  constexpr auto kPSM1 = l2cap::kHIDInteerup;
  constexpr l2cap::ChannelId kLocalId1 = kLocalId0 + 1;
  constexpr l2cap::ChannelId kRemoteId1 = kRemoteId0 + 1;
  const BrEdrSecurityRequirements sec_reqs1{.authentication = true, .secure_connections = false};
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, kPSM1, kLocalId1, kRemoteId1,
                                            l2cap::ChannelParameters());
  connmgr()->OpenL2capChannel(peer->identifier(), kPSM1, sec_reqs1, l2cap::ChannelParameters(),
                              sock_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(2u, sock_cb_count);

  QueueDisconnection(kConnectionHandle);
}

TEST_F(GAP_BrEdrConnectionManagerTest, OpenL2capChannelUpgradeLinkKeyFails) {
  QueueSuccessfulIncomingConn(kTestDevAddr, kConnectionHandle);
  test_device()->SendCommandChannelPacket(kConnectionRequest);

  RunLoopUntilIdle();

  auto* peer = peer_cache()->FindByAddress(kTestDevAddr);
  ASSERT_TRUE(peer);
  EXPECT_EQ(peer->identifier(), connmgr()->GetPeerId(kConnectionHandle));

  FakePairingDelegate pairing_delegate_no_io(sm::IOCapability::kNoInputNoOutput);
  connmgr()->SetPairingDelegate(pairing_delegate_no_io.GetWeakPtr());
  pairing_delegate_no_io.SetCompletePairingCallback(
      [](PeerId, sm::Status status) { EXPECT_TRUE(status.is_success()); });

  size_t sock_cb_count = 0;
  auto sock_cb = [&](auto chan_sock) {
    if (sock_cb_count == 0) {
      EXPECT_TRUE(chan_sock);
    } else {
      // Second OpenL2capChannel fails due to insufficient security.
      EXPECT_FALSE(chan_sock);
    }
    sock_cb_count++;
  };

  // Initial pairing.
  QueueSuccessfulUnauthenticatedPairing();

  constexpr auto kPSM0 = l2cap::kHIDControl;
  constexpr l2cap::ChannelId kLocalId = l2cap::kFirstDynamicChannelId;
  constexpr l2cap::ChannelId kRemoteId = 0x41;
  const BrEdrSecurityRequirements sec_reqs_none{.authentication = false,
                                                .secure_connections = false};
  data_domain()->ExpectOutboundL2capChannel(kConnectionHandle, kPSM0, kLocalId, kRemoteId,
                                            l2cap::ChannelParameters());
  connmgr()->OpenL2capChannel(peer->identifier(), kPSM0, sec_reqs_none, l2cap::ChannelParameters(),
                              sock_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(1u, sock_cb_count);

  // Pairing caused by insufficient link key.
  QueueSuccessfulUnauthenticatedPairing();

  constexpr auto kPSM1 = l2cap::kHIDInteerup;
  const BrEdrSecurityRequirements sec_reqs_auth{.authentication = true,
                                                .secure_connections = false};
  connmgr()->OpenL2capChannel(peer->identifier(), kPSM1, sec_reqs_auth, l2cap::ChannelParameters(),
                              sock_cb);

  RunLoopUntilIdle();
  EXPECT_EQ(2u, sock_cb_count);

  // Pairing should not be attempted a third time.

  QueueDisconnection(kConnectionHandle);
}

// Tests for assertions that enforce invariants.
class GAP_BrEdrConnectionManagerDeathTest : public BrEdrConnectionManagerTest {};

// Tests that a disconnection event that occurs after a peer gets removed is handled gracefully.
TEST_F(GAP_BrEdrConnectionManagerDeathTest, DisconnectAfterPeerRemovalAsserts) {
  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);
  EXPECT_TRUE(peer->temporary());

  // Queue up the connection
  EXPECT_CMD_PACKET_OUT(test_device(), kCreateConnection, &kCreateConnectionRsp,
                        &kConnectionComplete);
  QueueSuccessfulInterrogation(peer->address(), kConnectionHandle);
  QueueDisconnection(kConnectionHandle);

  // Initialize as error to verify that |callback| assigns success.
  hci::Status status(HostError::kFailed);
  BrEdrConnection* conn_ref = nullptr;
  auto callback = [&status, &conn_ref](auto cb_status, auto cb_conn_ref) {
    EXPECT_TRUE(cb_conn_ref);
    status = cb_status;
    conn_ref = std::move(cb_conn_ref);
  };

  EXPECT_TRUE(connmgr()->Connect(peer->identifier(), callback));
  ASSERT_TRUE(peer->bredr());
  RunLoopUntilIdle();
  ASSERT_TRUE(status);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        // Remove the peer without removing it from the cache. Normally this is not recommended as
        // implied by the name of the function but it is possible for this invariant to be broken
        // due to programmer error. The connection manager should assert this invariant.
        peer->MutBrEdr().SetConnectionState(Peer::ConnectionState::kNotConnected);
        __UNUSED auto _ = peer_cache()->RemoveDisconnectedPeer(peer->identifier());

        test_device()->SendCommandChannelPacket(kDisconnectionComplete);
        RunLoopUntilIdle();
      },
      ".*");
}

// TODO(BT-819) Connecting a peer that's being interrogated

#undef COMMAND_COMPLETE_RSP
#undef COMMAND_STATUS_RSP

}  // namespace
}  // namespace gap
}  // namespace bt
