// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error.h"

namespace bt {
namespace sdp {
namespace {

constexpr const char* ErrorCodeToString(ErrorCode code) {
  switch (code) {
    case ErrorCode::kReserved:
      return "reserved";
    case ErrorCode::kUnsupportedVersion:
      return "unsupported version";
    case ErrorCode::kInvalidRecordHandle:
      return "invalid record handle";
    case ErrorCode::kInvalidRequestSyntax:
      return "invalid request syntax";
    case ErrorCode::kInvalidSize:
      return "invalid size";
    case ErrorCode::kInvalidContinuationState:
      return "invalid continuation state";
    case ErrorCode::kInsufficientResources:
      return "insufficient resources";
    default:
      break;
  }
  return "unknown status";
}

}  // namespace
}  // namespace sdp

std::string ProtocolErrorTraits<sdp::ErrorCode>::ToString(sdp::ErrorCode ecode) {
  return bt_lib_cpp_string::StringPrintf("%s (SDP %#.2x)", bt::sdp::ErrorCodeToString(ecode),
                                         static_cast<unsigned int>(ecode));
}

}  // namespace bt
