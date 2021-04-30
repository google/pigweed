// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_STATUS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_STATUS_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

// This file provides a Status template specialization for hci::Status
//
// EXAMPLES:
//
//   // 1. Status containing success:
//   hci::Status status;
//
//   // 2. Status containing a host-internal error:
//   hci::Status status(HostError::kTimedOut);
//
//   // 3. Status containing HCI status code:
//   hci::Status status(hci::Status::kHardwareFailure);
//
//   // 4. Status containing HCI success status code is converted to #1:
//   hci::Status status(hci::StatusCode::kSuccess);
//   status.is_success() -> true
//   status.is_protocol_error() -> false

namespace bt {

template <>
struct ProtocolErrorTraits<hci::StatusCode> {
  static std::string ToString(hci::StatusCode ecode);
};

namespace hci {

class Status : public bt::Status<StatusCode> {
 public:
  explicit Status(HostError ecode = HostError::kNoError);
  explicit Status(hci::StatusCode proto_code);
};

using StatusCallback = fit::function<void(const Status& status)>;

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_STATUS_H_
