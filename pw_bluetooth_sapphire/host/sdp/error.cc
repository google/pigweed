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

#include "pw_bluetooth_sapphire/internal/host/sdp/error.h"

namespace bt {
namespace sdp {
namespace {

constexpr const char* ErrorCodeToString(ErrorCode code) {
  switch (code) {
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

std::string ProtocolErrorTraits<sdp::ErrorCode>::ToString(
    sdp::ErrorCode ecode) {
  return bt_lib_cpp_string::StringPrintf("%s (SDP %#.2x)",
                                         bt::sdp::ErrorCodeToString(ecode),
                                         static_cast<unsigned int>(ecode));
}

}  // namespace bt
