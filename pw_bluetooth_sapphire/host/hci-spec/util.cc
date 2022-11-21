// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util.h"

#include <endian.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"

namespace bt::hci_spec {

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
std::string StatusCodeToString(hci_spec::StatusCode code) {
  switch (code) {
    case hci_spec::StatusCode::SUCCESS: return "success";
    case hci_spec::StatusCode::UNKNOWN_COMMAND: return "unknown command";
    case hci_spec::StatusCode::UNKNOWN_CONNECTION_ID: return "unknown connection ID";
    case hci_spec::StatusCode::HARDWARE_FAILURE: return "hardware failure";
    case hci_spec::StatusCode::PAGE_TIMEOUT: return "page timeout";
    case hci_spec::StatusCode::AUTHENTICATION_FAILURE: return "authentication failure";
    case hci_spec::StatusCode::PIN_OR_KEY_MISSING: return "pin or key missing";
    case hci_spec::StatusCode::MEMORY_CAPACITY_EXCEEDED: return "memory capacity exceeded";
    case hci_spec::StatusCode::CONNECTION_TIMEOUT: return "connection timeout";
    case hci_spec::StatusCode::CONNECTION_LIMIT_EXCEEDED: return "connection limit exceeded";
    case hci_spec::StatusCode::SYNCHRONOUS_CONNECTION_LIMIT_EXCEEDED: return "synchronous connection limit exceeded";
    case hci_spec::StatusCode::CONNECTION_ALREADY_EXISTS: return "connection already exists";
    case hci_spec::StatusCode::COMMAND_DISALLOWED: return "command disallowed";
    case hci_spec::StatusCode::CONNECTION_REJECTED_LIMITED_RESOURCES: return "connection rejected: limited resources";
    case hci_spec::StatusCode::CONNECTION_REJECTED_SECURITY: return "connection rejected: security";
    case hci_spec::StatusCode::CONNECTION_REJECTED_BAD_BD_ADDR: return "connection rejected: bad BD_ADDR";
    case hci_spec::StatusCode::CONNECTION_ACCEPT_TIMEOUT_EXCEEDED: return "connection accept timeout exceeded";
    case hci_spec::StatusCode::UNSUPPORTED_FEATURE_OR_PARAMETER: return "unsupported feature or parameter";
    case hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS: return "invalid HCI command parameters";
    case hci_spec::StatusCode::REMOTE_USER_TERMINATED_CONNECTION: return "remote user terminated connection";
    case hci_spec::StatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_LOW_RESOURCES: return "remote device terminated connection: low resources";
    case hci_spec::StatusCode::REMOTE_DEVICE_TERMINATED_CONNECTION_POWER_OFF: return "remote device terminated connection: power off";
    case hci_spec::StatusCode::CONNECTION_TERMINATED_BY_LOCAL_HOST: return "connection terminated by local host";
    case hci_spec::StatusCode::REPEATED_ATTEMPTS: return "repeated attempts";
    case hci_spec::StatusCode::PAIRING_NOT_ALLOWED: return "pairing not allowed";
    case hci_spec::StatusCode::UNKNOWN_LMP_PDU: return "unhci_spec::StatusCode::nown LMP PDU";
    case hci_spec::StatusCode::UNSUPPORTED_REMOTE_FEATURE: return "unsupported remote feature";
    case hci_spec::StatusCode::SCO_OFFSET_REJECTED: return "SCO offset rejected";
    case hci_spec::StatusCode::SCO_INTERVAL_REJECTED: return "SCO interval rejected";
    case hci_spec::StatusCode::SCO_AIRMODE_REJECTED: return "SCO air mode rejected";
    case hci_spec::StatusCode::INVALID_LMP_OR_LL_PARAMETERS: return "invalid LMP or LL parameters";
    case hci_spec::StatusCode::UNSPECIFIED_ERROR: return "unspecified error";
    case hci_spec::StatusCode::UNSUPPORTED_LMP_OR_LL_PARAMETER_VALUE: return "unsupported LMP or LL parameter value";
    case hci_spec::StatusCode::ROLE_CHANGE_NOT_ALLOWED: return "role change not allowed";
    case hci_spec::StatusCode::LMP_OR_LL_RESPONSE_TIMEOUT: return "LMP or LL response timeout";
    case hci_spec::StatusCode::LMP_ERROR_TRANSACTION_COLLISION: return "LMP error transaction collision";
    case hci_spec::StatusCode::LMP_PDU_NOT_ALLOWED: return "LMP PDU not allowed";
    case hci_spec::StatusCode::ENCRYPTION_MODE_NOT_ACCEPTABLE: return "encryption mode not acceptable";
    case hci_spec::StatusCode::LINK_KEY_CANNOT_BE_CHANGED: return "link key cannot be changed";
    case hci_spec::StatusCode::REQUESTED_QOS_NOT_SUPPORTED: return "requested QoS not supported";
    case hci_spec::StatusCode::INSTANT_PASSED: return "instant passed";
    case hci_spec::StatusCode::PAIRING_WITH_UNIT_KEY_NOT_SUPPORTED: return "pairing with unit key not supported";
    case hci_spec::StatusCode::DIFFERENT_TRANSACTION_COLLISION: return "different transaction collision";
    case hci_spec::StatusCode::QOS_UNACCEPTABLE_PARAMETER: return "QoS unacceptable parameter";
    case hci_spec::StatusCode::QOS_REJECTED: return "QoS rejected";
    case hci_spec::StatusCode::CHANNEL_CLASSIFICATION_NOT_SUPPORTED: return "channel classification not supported";
    case hci_spec::StatusCode::INSUFFICIENT_SECURITY: return "insufficient security";
    case hci_spec::StatusCode::PARAMETER_OUT_OF_MANDATORY_RANGE: return "parameter out of mandatory range";
    case hci_spec::StatusCode::ROLE_SWITCH_PENDING: return "role switch pending";
    case hci_spec::StatusCode::RESERVED_SLOT_VIOLATION: return "reserved slot violation";
    case hci_spec::StatusCode::ROLE_SWITCH_FAILED: return "role switch failed";
    case hci_spec::StatusCode::EXTENDED_INQUIRY_RESPONSE_TOO_LARGE: return "extended inquiry response too large";
    case hci_spec::StatusCode::SECURE_SIMPLE_PAIRING_NOT_SUPPORTED_BY_HOST: return "secure simple pairing not supported by host";
    case hci_spec::StatusCode::HOST_BUSY_PAIRING: return "host busy pairing";
    case hci_spec::StatusCode::CONNECTION_REJECTED_NO_SUITABLE_CHANNEL_FOUND: return "connection rejected: no suitable channel found";
    case hci_spec::StatusCode::CONTROLLER_BUSY: return "controller busy";
    case hci_spec::StatusCode::UNACCEPTABLE_CONNECTION_PARAMETERS: return "unacceptable connection parameters";
    case hci_spec::StatusCode::DIRECTED_ADVERTISING_TIMEOUT: return "directed advertising timeout";
    case hci_spec::StatusCode::CONNECTION_TERMINATED_MIC_FAILURE: return "connection terminated: MIC failure";
    case hci_spec::StatusCode::CONNECTION_FAILED_TO_BE_ESTABLISHED: return "connection failed to be established";
    case hci_spec::StatusCode::MAC_CONNECTION_FAILED: return "MAC connection failed";
    case hci_spec::StatusCode::COARSE_CLOCK_ADJUSTMENT_REJECTED: return "coarse clochci_spec::StatusCode:: adjustment rejected";
    case hci_spec::StatusCode::TYPE_0_SUBMAP_NOT_DEFINED: return "type 0 submap not defined";
    case hci_spec::StatusCode::UNKNOWN_ADVERTISING_IDENTIFIER: return "unknown advertising identifier";
    case hci_spec::StatusCode::LIMIT_REACHED: return "limit reached";
    case hci_spec::StatusCode::OPERATION_CANCELLED_BY_HOST: return "operation cancelled by host";
    default: break;
  };
  return "unknown status";
}
// clang-format on

std::string LinkTypeToString(hci_spec::LinkType link_type) {
  switch (link_type) {
    case LinkType::kSCO:
      return "SCO";
    case LinkType::kACL:
      return "ACL";
    case LinkType::kExtendedSCO:
      return "eSCO";
    default:
      return "<Unknown LinkType>";
  };
}

std::string ConnectionRoleToString(hci_spec::ConnectionRole role) {
  switch (role) {
    case hci_spec::ConnectionRole::CENTRAL:
      return "central";
    case hci_spec::ConnectionRole::PERIPHERAL:
      return "peripheral";
    default:
      return "<unknown role>";
  }
}

// TODO(fxbug.dev/80048): various parts of the spec call for a 3 byte integer. If we need to in the
// future, we should generalize this logic and make a uint24_t type that makes it easier to work
// with these types of conversions.
void EncodeLegacyAdvertisingInterval(uint16_t input, uint8_t (&result)[3]) {
  MutableBufferView result_view(result, sizeof(result));
  result_view.SetToZeros();

  // Core spec Volume 6, Part B, Section 1.2: Link layer order is little endian, convert to little
  // endian if host order is big endian
  input = htole16(input);
  BufferView input_view(&input, sizeof(input));

  input_view.Copy(&result_view, 0, sizeof(input));
}

// TODO(fxbug.dev/80048): various parts of the spec call for a 3 byte integer. If we need to in the
// future, we should generalize this logic and make a uint24_t type that makes it easier to work
// with these types of conversions.
uint32_t DecodeExtendedAdvertisingInterval(const uint8_t (&input)[3]) {
  uint32_t result = 0;
  MutableBufferView result_view(&result, sizeof(result));

  BufferView input_view(input, sizeof(input));
  input_view.Copy(&result_view);

  // Core spec Volume 6, Part B, Section 1.2: Link layer order is little endian, convert to little
  // endian if host order is big endian
  return letoh32(result);
}

std::optional<AdvertisingEventBits> AdvertisingTypeToEventBits(LEAdvertisingType type) {
  // TODO(fxbug.dev/81470): for backwards compatibility and because supporting extended advertising
  // PDUs is a much larger project, we currently only support legacy PDUs. Without using legacy
  // PDUs, non-Bluetooth 5 devices will not be able to discover extended advertisements.
  uint16_t adv_event_properties = kLEAdvEventPropBitUseLegacyPDUs;

  // Bluetooth Spec Volume 4, Part E, Section 7.8.53, Table 7.2 defines the mapping of legacy PDU
  // types to the corresponding bits within adv_event_properties.
  switch (type) {
    case LEAdvertisingType::kAdvInd:
      adv_event_properties |= kLEAdvEventPropBitConnectable;
      adv_event_properties |= kLEAdvEventPropBitScannable;
      break;
    case LEAdvertisingType::kAdvDirectIndLowDutyCycle:
      adv_event_properties |= kLEAdvEventPropBitConnectable;
      adv_event_properties |= kLEAdvEventPropBitDirected;
      break;
    case LEAdvertisingType::kAdvDirectIndHighDutyCycle:
      adv_event_properties |= kLEAdvEventPropBitConnectable;
      adv_event_properties |= kLEAdvEventPropBitDirected;
      adv_event_properties |= kLEAdvEventPropBitHighDutyCycleDirectedConnectable;
      break;
    case LEAdvertisingType::kAdvScanInd:
      adv_event_properties |= kLEAdvEventPropBitScannable;
      break;
    case LEAdvertisingType::kAdvNonConnInd:
      // no extra bits to set
      break;
    default:
      return std::nullopt;
  }

  return adv_event_properties;
}

}  // namespace bt::hci_spec
