// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {

using common::DeviceAddress;

namespace hci {

std::string HCIVersionToString(HCIVersion version) {
  switch (version) {
    case HCIVersion::k1_0b:
      return "1.0b";
    case HCIVersion::k1_1:
      return "1.1";
    case HCIVersion::k1_2:
      return "1.2";
    case HCIVersion::k2_0_EDR:
      return "2.0 + EDR";
    case HCIVersion::k2_1_EDR:
      return "2.1 + EDR";
    case HCIVersion::k3_0_HS:
      return "3.0 + HS";
    case HCIVersion::k4_0:
      return "4.0";
    case HCIVersion::k4_1:
      return "4.1";
    case HCIVersion::k4_2:
      return "4.2";
    case HCIVersion::k5_0:
      return "5.0";
    default:
      break;
  }
  return "(unknown)";
}

// clang-format off
std::string StatusCodeToString(StatusCode code) {
  switch (code) {
    case kSuccess: return "success";
    case kUnknownCommand: return "unknown command";
    case kUnknownConnectionId: return "unknown connection ID";
    case kHardwareFailure: return "hardware failure";
    case kPageTimeout: return "page timeout";
    case kAuthenticationFailure: return "authentication failure";
    case kPinOrKeyMissing: return "pin or key missing";
    case kMemoryCapacityExceeded: return "memory capacity exceeded";
    case kConnectionTimeout: return "connection timeout";
    case kConnectionLimitExceeded: return "connection limit exceeded";
    case kSynchronousConnectionLimitExceeded: return "synchronous connection limit exceeded";
    case kConnectionAlreadyExists: return "connection already exists";
    case kCommandDisallowed: return "command disallowed";
    case kConnectionRejectedLimitedResources: return "connection rejected: limited resources";
    case kConnectionRejectedSecurity: return "connection rejected: security";
    case kConnectionRejectedBadBdAddr: return "connection rejected: bad BD_ADDR";
    case kConnectionAcceptTimeoutExceeded: return "connection accept timeout exceeded";
    case kUnsupportedFeatureOrParameter: return "unsupported feature or parameter";
    case kInvalidHCICommandParameters: return "invalid HCI command parameters";
    case kRemoteUserTerminatedConnection: return "remote user terminated connection";
    case kRemoteDeviceTerminatedConnectionLowResources: return "remote device terminated connection: low resources";
    case kRemoteDeviceTerminatedConnectionPowerOff: return "remote device terminated connection: power off";
    case kConnectionTerminatedByLocalHost: return "connection terminated by local host";
    case kRepeatedAttempts: return "repeated attempts";
    case kPairingNotAllowed: return "pairing not allowed";
    case kUnknownLMPPDU: return "unknown LMP PDU";
    case kUnsupportedRemoteFeature: return "unsupported remote feature";
    case kSCOOffsetRejected: return "SCO offset rejected";
    case kSCOIntervalRejected: return "SCO interval rejected";
    case kSCOAirModeRejected: return "SCO air mode rejected";
    case kInvalidLMPOrLLParameters: return "invalid LMP or LL parameters";
    case kUnspecifiedError: return "unspecified error";
    case kUnsupportedLMPOrLLParameterValue: return "unsupported LMP or LL parameter value";
    case kRoleChangeNotAllowed: return "role change not allowed";
    case kLMPOrLLResponseTimeout: return "LMP or LL response timeout";
    case kLMPErrorTransactionCollision: return "LMP error transaction collision";
    case kLMPPDUNotAllowed: return "LMP PDU not allowed";
    case kEncryptionModeNotAcceptable: return "encryption mode not acceptable";
    case kLinkKeyCannotBeChanged: return "link key cannot be changed";
    case kRequestedQoSNotSupported: return "requested QoS not supported";
    case kInstantPassed: return "instant passed";
    case kPairingWithUnitKeyNotSupported: return "pairing with unit key not supported";
    case kDifferentTransactionCollision: return "different transaction collision";
    case kQoSUnacceptableParameter: return "QoS unacceptable parameter";
    case kQoSRejected: return "QoS rejected";
    case kChannelClassificationNotSupported: return "channel classification not supported";
    case kInsufficientSecurity: return "insufficient security";
    case kParameterOutOfMandatoryRange: return "parameter out of mandatory range";
    case kRoleSwitchPending: return "role switch pending";
    case kReservedSlotViolation: return "reserved slot violation";
    case kRoleSwitchFailed: return "role switch failed";
    case kExtendedInquiryResponseTooLarge: return "extended inquiry response too large";
    case kSecureSimplePairingNotSupportedByHost: return "secure simple pairing not supported by host";
    case kHostBusyPairing: return "host busy pairing";
    case kConnectionRejectedNoSuitableChannelFound: return "connection rejected: no suitable channel found";
    case kControllerBusy: return "controller busy";
    case kUnacceptableConnectionParameters: return "unacceptable connection parameters";
    case kDirectedAdvertisingTimeout: return "directed advertising timeout";
    case kConnectionTerminatedMICFailure: return "connection terminated: MIC failure";
    case kConnectionFailedToBeEstablished: return "connection failed to be established";
    case kMACConnectionFailed: return "MAC connection failed";
    case kCoarseClockAdjustmentRejected: return "coarse clock adjustment rejected";
    case kType0SubmapNotDefined: return "type 0 submap not defined";
    case kUnknownAdvertisingIdentifier: return "unknown advertising identifier";
    case kLimitReached: return "limit reached";
    case kOperationCancelledByHost: return "operation cancelled by host";
    default: break;
  };
  return "unknown status";
}
// clang-format on

bool DeviceAddressFromAdvReport(const LEAdvertisingReportData& report,
                                DeviceAddress* out_address,
                                bool* out_resolved) {
  ZX_DEBUG_ASSERT(out_address);
  ZX_DEBUG_ASSERT(out_resolved);

  DeviceAddress::Type type;
  switch (report.address_type) {
    case LEAddressType::kPublicIdentity:
      type = DeviceAddress::Type::kLEPublic;
      *out_resolved = true;
      break;
    case LEAddressType::kPublic:
      type = DeviceAddress::Type::kLEPublic;
      *out_resolved = false;
      break;
    case LEAddressType::kRandomIdentity:
      type = DeviceAddress::Type::kLERandom;
      *out_resolved = true;
      break;
    case LEAddressType::kRandom:
      type = DeviceAddress::Type::kLERandom;
      *out_resolved = false;
      break;
    default:
      bt_log(WARN, "hci", "invalid address type in advertising report: %#.2x",
             static_cast<uint8_t>(report.address_type));
      return false;
  }

  *out_address = DeviceAddress(type, report.address);
  return true;
}

DeviceAddress::Type AddressTypeFromHCI(LEAddressType type) {
  DeviceAddress::Type result;
  switch (type) {
    case LEAddressType::kPublic:
    case LEAddressType::kPublicIdentity:
      result = DeviceAddress::Type::kLEPublic;
      break;
    case LEAddressType::kRandom:
    case LEAddressType::kRandomIdentity:
    case LEAddressType::kRandomUnresolved:
      result = DeviceAddress::Type::kLERandom;
      break;
    case LEAddressType::kAnonymous:
      result = DeviceAddress::Type::kLEAnonymous;
      break;
  }
  return result;
}

DeviceAddress::Type AddressTypeFromHCI(LEPeerAddressType type) {
  DeviceAddress::Type result;
  switch (type) {
    case LEPeerAddressType::kPublic:
      result = DeviceAddress::Type::kLEPublic;
      break;
    case LEPeerAddressType::kRandom:
      result = DeviceAddress::Type::kLERandom;
      break;
    case LEPeerAddressType::kAnonymous:
      result = DeviceAddress::Type::kLEAnonymous;
      break;
  }
  return result;
}

LEAddressType AddressTypeToHCI(DeviceAddress::Type type) {
  LEAddressType result = LEAddressType::kPublic;
  switch (type) {
    case DeviceAddress::Type::kLEPublic:
      result = LEAddressType::kPublic;
      break;
    case DeviceAddress::Type::kLERandom:
      result = LEAddressType::kRandom;
      break;
    case DeviceAddress::Type::kLEAnonymous:
      result = LEAddressType::kAnonymous;
      break;
    default:
      ZX_PANIC("invalid address type: %u", static_cast<unsigned int>(type));
      break;
  }
  return result;
}

}  // namespace hci
}  // namespace bt
