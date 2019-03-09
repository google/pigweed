// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_STATUS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_STATUS_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"

// This file provides a common::Status template specialization for hci::Status
//
// EXAMPLES:
//
//   // 1. Status containing success:
//   hci::Status status;
//
//   // 2. Status containing a host-internal error:
//   hci::Status status(common::HostError::kTimedOut);
//
//   // 3. Status containing HCI status code:
//   hci::Status status(hci::Status::kHardwareFailure);
//
//   // 4. Status containing HCI success status code is converted to #1:
//   hci::Status status(hci::StatusCode::kSuccess);
//   status.is_success() -> true
//   status.is_protocol_error() -> false

namespace btlib {
namespace common {

template <>
struct ProtocolErrorTraits<hci::StatusCode> {
  static std::string ToString(hci::StatusCode ecode);
};

}  // namespace common

namespace hci {

class Status : public common::Status<StatusCode> {
 public:
  explicit Status(common::HostError ecode = common::HostError::kNoError);
  explicit Status(hci::StatusCode proto_code);
};

using StatusCallback = fit::function<void(const Status& status)>;

}  // namespace hci
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_STATUS_H_
