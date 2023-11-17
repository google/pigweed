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

#include "pw_bluetooth_sapphire/internal/host/att/error.h"

#include "pw_string/format.h"

#pragma clang diagnostic ignored "-Wswitch-enum"

namespace bt {
namespace {

constexpr const char* ErrorToString(att::ErrorCode ecode) {
  switch (ecode) {
    case att::ErrorCode::kInvalidHandle:
      return "invalid handle";
    case att::ErrorCode::kReadNotPermitted:
      return "read not permitted";
    case att::ErrorCode::kWriteNotPermitted:
      return "write not permitted";
    case att::ErrorCode::kInvalidPDU:
      return "invalid PDU";
    case att::ErrorCode::kInsufficientAuthentication:
      return "insuff. authentication";
    case att::ErrorCode::kRequestNotSupported:
      return "request not supported";
    case att::ErrorCode::kInvalidOffset:
      return "invalid offset";
    case att::ErrorCode::kInsufficientAuthorization:
      return "insuff. authorization";
    case att::ErrorCode::kPrepareQueueFull:
      return "prepare queue full";
    case att::ErrorCode::kAttributeNotFound:
      return "attribute not found";
    case att::ErrorCode::kAttributeNotLong:
      return "attribute not long";
    case att::ErrorCode::kInsufficientEncryptionKeySize:
      return "insuff. encryption key size";
    case att::ErrorCode::kInvalidAttributeValueLength:
      return "invalid attribute value length";
    case att::ErrorCode::kUnlikelyError:
      return "unlikely error";
    case att::ErrorCode::kInsufficientEncryption:
      return "insuff. encryption";
    case att::ErrorCode::kUnsupportedGroupType:
      return "unsupported group type";
    case att::ErrorCode::kInsufficientResources:
      return "insuff. resources";
    default:
      break;
  }

  return "(unknown)";
}

}  // namespace

std::string ProtocolErrorTraits<att::ErrorCode>::ToString(
    att::ErrorCode ecode) {
  constexpr size_t out_size =
      sizeof("invalid attribute value length (ATT 0x0d)");
  char out[out_size] = "";
  pw::StatusWithSize status =
      pw::string::Format({out, sizeof(out)},
                         "%s (ATT %#.2hhx)",
                         ErrorToString(ecode),
                         static_cast<unsigned char>(ecode));
  BT_DEBUG_ASSERT(status.ok());
  return out;
}

}  // namespace bt
