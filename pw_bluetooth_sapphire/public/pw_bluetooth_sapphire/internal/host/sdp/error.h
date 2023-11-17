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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_ERROR_H_

#include "pw_bluetooth_sapphire/internal/host/common/error.h"
#include "pw_bluetooth_sapphire/internal/host/sdp/sdp.h"

// This file would conventionally provide sdp::Result, sdp::ResultFunction, and
// sdp::ResultCallback but there is currently no code that would use these
// types. Instead, this file only contains the template specializations required
// to instantiate the Error<sdp::ErrorCode> underlying sdp::Result.

namespace bt {

template <>
struct ProtocolErrorTraits<sdp::ErrorCode> {
  static std::string ToString(sdp::ErrorCode ecode);

  // is_success() not declared because ErrorCode does not include a "success"
  // value (Core Spec v5.3, Vol 3, Part B, Sec 4.4.1).
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_ERROR_H_
