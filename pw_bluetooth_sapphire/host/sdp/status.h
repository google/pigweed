// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_STATUS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_STATUS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"

// This file provides a common::Status template specialization for sdp::Status
//
// EXAMPLES:
//
//   // 1. Status containing success:
//   sdp::Status status;
//
//   // 2. Status containing a host-internal error:
//   sdp::Status status(common::HostError::kTimedOut);
//
//   // 3. Status containing SDP status code:
//   sdp::Status status(sdp::ErrorCode::kInvalidSize);

namespace bt {
namespace common {

template <>
struct ProtocolErrorTraits<bt::sdp::ErrorCode> {
  static std::string ToString(bt::sdp::ErrorCode ecode);
};

}  // namespace common

namespace sdp {

std::string ErrorCodeToString(ErrorCode code);

class Status : public common::Status<ErrorCode> {
 public:
  explicit Status(common::HostError ecode = common::HostError::kNoError);
  explicit Status(sdp::ErrorCode proto_code);
};

}  // namespace sdp
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SDP_STATUS_H_
