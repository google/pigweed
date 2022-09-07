// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error.h"

#include "pw_string/format.h"

namespace bt {
namespace {

constexpr const char* ErrorToString(sm::ErrorCode ecode) {
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

constexpr size_t kMaxErrorStringSize =
    std::string_view(ErrorToString(sm::ErrorCode::kCrossTransportKeyDerivationNotAllowed)).size();

}  // namespace

// static
std::string ProtocolErrorTraits<sm::ErrorCode>::ToString(sm::ErrorCode ecode) {
  const size_t out_size = kMaxErrorStringSize + sizeof(" (SMP 0x0e)");
  char out[out_size] = "";
  pw::StatusWithSize status = pw::string::Format(
      {out, sizeof(out)}, "%s (SMP %#.2x)", ErrorToString(ecode), static_cast<unsigned int>(ecode));
  BT_DEBUG_ASSERT(status.ok());
  return out;
}

}  // namespace bt
