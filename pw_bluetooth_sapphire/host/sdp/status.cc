// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

namespace bt {

// static
std::string ProtocolErrorTraits<sdp::ErrorCode>::ToString(sdp::ErrorCode ecode) {
  return bt_lib_cpp_string::StringPrintf("%s (SDP %#.2x)",
                                         bt::sdp::ErrorCodeToString(ecode).c_str(),
                                         static_cast<unsigned int>(ecode));
}

namespace sdp {

std::string ErrorCodeToString(ErrorCode code) {
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

using Base = bt::Status<ErrorCode>;

Status::Status(HostError ecode) : Base(ecode) {}

Status::Status(ErrorCode proto_code) : Base(proto_code) {}

}  // namespace sdp
}  // namespace bt
