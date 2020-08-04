// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SMP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SMP_H_

#include <zircon/compiler.h>

#include <cstdint>

#include "lib/zx/time.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"

// This file defines constants that are used by the Security Manager Protocol
// (SMP) that operates over the L2CAP SMP channel.

namespace bt {
namespace sm {

// v5.0, Vol 3, Part H, 3.2
constexpr uint16_t kNoSecureConnectionsMtu = 23;
constexpr uint16_t kLeSecureConnectionsMtu = 65;

// SMP Timeout in seconds (Vol 3, Part H, 3.4)
constexpr zx::duration kPairingTimeout = zx::sec(30);

// The supported encryption key sizes (Vol 3, Part H, 2.3.4).
constexpr uint8_t kMinEncryptionKeySize = 7;
constexpr uint8_t kMaxEncryptionKeySize = 16;

// These are the sample ltk and random from (Vol 6, Part C, 1), they are
// declared so that SecurityManager can reject any peers using them and prevent
// a mitm.
constexpr UInt128 kSpecSampleLtk = {0xBF, 0x01, 0xFB, 0x9D, 0x4E, 0xF3, 0xBC, 0x36,
                                    0xD8, 0x74, 0xF5, 0x39, 0x41, 0x38, 0x68, 0x4C};
constexpr uint64_t kSpecSampleRandom = 0xABCDEF1234567890;

// The field that identifies the type of a command.
using Code = uint8_t;

struct Header {
  Code code;
} __PACKED;

// Supported pairing methods.
enum class PairingMethod {
  // Unauthenticated
  kJustWorks,

  // Local host inputs passkey. Authenticated.
  kPasskeyEntryInput,

  // Local host displays passkey. Authenticated.
  kPasskeyEntryDisplay,

  // Authenticated, LE Secure Connections only.
  kNumericComparison,

  // Authenticated depending on OOB mechanism
  kOutOfBand,
};

enum class IOCapability : uint8_t {
  kDisplayOnly = 0x00,
  kDisplayYesNo = 0x01,
  kKeyboardOnly = 0x02,
  kNoInputNoOutput = 0x03,
  kKeyboardDisplay = 0x04,
};

enum class OOBDataFlag : uint8_t {
  kNotPresent = 0x00,
  kPresent = 0x01,
};

// Possible values that can be assigned to the "AuthReq" bit field (Vol 3, Part
// H, Figure 3.3).
enum AuthReq : uint8_t {
  // Indicates that bonding is requested.
  kBondingFlag = (1 << 0),

  // Indicates whether Man-in-the-middle protection is required.
  kMITM = (1 << 2),

  // Indicates whether Secure Connections is supported.
  kSC = (1 << 3),

  // Indicates whether Keypress notifications should be generated for the
  // Passkey Entry protocol.
  kKeypress = (1 << 4),

  // Indicates whether cross-transport key generation is supported for Secure
  // Connections.
  kCT2 = (1 << 5),
};
using AuthReqField = uint8_t;

// Possible values for the Key Distribution/Generation fields (Vol 3, Part H,
// Figure 3.11)
enum KeyDistGen : uint8_t {
  // LE: Indicates that the LTK will be distributed using the "Encryption
  // Information" command in LE legacy pairing. Ignored in LE Secure
  // Connections.
  //
  // BR/EDR: Indicates that the LTK will be derived from the BR/EDR Link Key.
  kEncKey = (1 << 0),

  // Indicates that the IRK will be distributed using the "Identity Information"
  // command and the Identity Address using the "Identity Address Information"
  // command.
  kIdKey = (1 << 1),

  // Indicates that the CSRK will be distributed using the "Signing Information"
  // command.
  kSignKey = (1 << 2),

  // LE: Indicates that the BR/EDR Link Key will be derived from the LTK.
  // Ignored if LE Secure Connections isn't supported.
  //
  // BR/EDR: Reserved for future use.
  kLinkKey = (1 << 3),
};
using KeyDistGenField = uint8_t;

// Possible failure reason codes used in the "Pairing Failed" command.
// (Vol 3, Part H, 3.5.5).
enum class ErrorCode : uint8_t {
  kNoError = 0x00,

  // User input of passkey failed, e.g. due to cancelation.
  kPasskeyEntryFailed = 0x01,

  // OOB data is not available.
  kOOBNotAvailable = 0x02,

  // Authentication requirements cannot be met due to IO capabilities.
  kAuthenticationRequirements = 0x03,

  // The confirm value does not match what was calculated.
  kConfirmValueFailed = 0x04,

  // Pairing is not supported.
  kPairingNotSupported = 0x05,

  // The resultant encryption key size is insufficient given local security
  // requirements.
  kEncryptionKeySize = 0x06,

  // An SMP command is not supported.
  kCommandNotSupported = 0x07,

  // Pairing failed due to an unspecified reason.
  kUnspecifiedReason = 0x08,

  // Pairing/authentication procedure is disallowed because too little time has
  // elapsed since the last pairing/security request.
  kRepeatedAttempts = 0x09,

  // SMP command parameters were invalid.
  kInvalidParameters = 0x0A,

  // Indicates to the remote device that the DHKey Check value received doesn't
  // match the one calculated locally.
  kDHKeyCheckFailed = 0x0B,

  // Indicates that the confirm values in the numeric comparison protocol do not
  // match.
  kNumericComparisonFailed = 0x0C,

  // Indicates that pairing over the LE transport failed due to a concurrent
  // pairing request over the BR/EDR transport.
  kBREDRPairingInProgress = 0x0D,

  // Indicates that the BR/EDR Link Key generated on the BR/EDR transport cannot
  // be used to derive keys for the LE transport.
  kCrossTransportKeyDerivationNotAllowed = 0x0E,
};

// Possible keypress notification types used in the "Keypress Notification"
// command (Vol 3, Part H, 3.5.8).
enum class KeypressNotificationType : uint8_t {
  kStarted = 0,
  kDigitEntered = 1,
  kDigitErased = 2,
  kCleared = 3,
  kCompleted = 4,
};

// Possible address types used in the "Identity Address Information" command
// (Vol 3, Part H, 3.6.5).
enum class AddressType : uint8_t {
  kPublic = 0x00,
  kStaticRandom = 0x01,
};

// ========== SMP PDUs ========
constexpr Code kInvalidCode = 0x00;

// ======================================
// Pairing Request (Vol 3, Part H, 3.5.1)
constexpr Code kPairingRequest = 0x01;
struct PairingRequestParams {
  // The local I/O capability.
  IOCapability io_capability;

  // Whether or not OOB authentication data is available.
  OOBDataFlag oob_data_flag;

  // The requested security properties (Vol 3, Part H, 2.3.1).
  AuthReqField auth_req;

  // Maximum encryption key size supported. Valid values are 7-16.
  uint8_t max_encryption_key_size;

  // The keys that the initiator requests to distribute/generate.
  KeyDistGenField initiator_key_dist_gen;

  // The keys that the responder requests to distribute/generate.
  KeyDistGenField responder_key_dist_gen;
} __PACKED;

// =======================================
// Pairing Response (Vol 3, Part H, 3.5.2)
constexpr Code kPairingResponse = 0x02;
using PairingResponseParams = PairingRequestParams;

// ======================================
// Pairing Confirm (Vol 3, Part H, 3.5.3)
constexpr Code kPairingConfirm = 0x03;
using PairingConfirmValue = UInt128;

// =====================================
// Pairing Random (Vol 3, Part H, 3.5.4)
constexpr Code kPairingRandom = 0x04;
using PairingRandomValue = UInt128;

// =====================================
// Pairing Failed (Vol 3, Part H, 3.5.5)
constexpr Code kPairingFailed = 0x05;
using PairingFailedParams = ErrorCode;

// =============================================
// Encryption Information (LE Legacy Pairing only; Vol 3, Part H, 3.6.2)
constexpr Code kEncryptionInformation = 0x06;
using EncryptionInformationParams = UInt128;

// ====================================================================
// Master Identification (LE Legacy Pairing only; Vol 3, Part H, 3.6.3)
constexpr Code kMasterIdentification = 0x07;
struct MasterIdentificationParams {
  uint16_t ediv;
  uint64_t rand;
} __PACKED;

// ===========================================
// Identity Information (Vol 3, Part H, 3.6.4)
constexpr Code kIdentityInformation = 0x08;
using IRK = UInt128;

// ===================================================
// Identity Address Information (Vol 3, Part H, 3.6.5)
constexpr Code kIdentityAddressInformation = 0x09;
struct IdentityAddressInformationParams {
  AddressType type;
  DeviceAddressBytes bd_addr;
} __PACKED;

// ==========================================
// Signing Information (Vol 3, Part H, 3.6.6)
constexpr Code kSigningInformation = 0x0A;
using CSRK = UInt128;

// =======================================
// Security Request (Vol 3, Part H, 3.6.7)
constexpr Code kSecurityRequest = 0x0B;

// See enum AuthReq for parameters.

// ==================================================================
// Pairing Public Key (Secure Connections only; Vol 3, Part H, 3.5.6)
constexpr Code kPairingPublicKey = 0x0C;
struct PairingPublicKeyParams {
  uint8_t x[32];
  uint8_t y[32];
} __PACKED;

// ======================================================================
// Pairing DHKey Check (LE Secure Connections only; Vol 3, Part H, 3.5.7)
constexpr Code kPairingDHKeyCheck = 0x0D;
using PairingDHKeyCheckValueE = UInt128;

// ============================================
// Keypress Notification (Vol 3, Part H, 3.5.8)
constexpr Code kKeypressNotification = 0x0E;

// See enum KeypressNotificationType above for parameters.

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SMP_H_
