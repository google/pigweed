// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

namespace bt {
namespace {

std::string ErrorToString(sm::ErrorCode ecode) {
  switch (ecode) {
    case sm::ErrorCode::kPasskeyEntryFailed:
      return "passkey entry failed";
    case sm::ErrorCode::kOOBNotAvailable:
      return "OOB not available";
    case sm::ErrorCode::kAuthenticationRequirements:
      return "authentication requirements";
    case sm::ErrorCode::kConfirmValueFailed:
      return "confirm value failed";
    case sm::ErrorCode::kPairingNotSupported:
      return "pairing not supported";
    case sm::ErrorCode::kEncryptionKeySize:
      return "encryption key size";
    case sm::ErrorCode::kCommandNotSupported:
      return "command not supported";
    case sm::ErrorCode::kUnspecifiedReason:
      return "unspecified reason";
    case sm::ErrorCode::kRepeatedAttempts:
      return "repeated attempts";
    case sm::ErrorCode::kInvalidParameters:
      return "invalid parameters";
    case sm::ErrorCode::kDHKeyCheckFailed:
      return "DHKey check failed";
    case sm::ErrorCode::kNumericComparisonFailed:
      return "numeric comparison failed";
    case sm::ErrorCode::kBREDRPairingInProgress:
      return "BR/EDR pairing in progress";
    case sm::ErrorCode::kCrossTransportKeyDerivationNotAllowed:
      return "cross-transport key dist. not allowed";
    default:
      break;
  }
  return "(unknown)";
}

}  // namespace

// static
std::string ProtocolErrorTraits<sm::ErrorCode>::ToString(sm::ErrorCode ecode) {
  return bt_lib_cpp_string::StringPrintf("%s (SMP %#.2x)", ErrorToString(ecode).c_str(),
                                         static_cast<unsigned int>(ecode));
}

}  // namespace bt
