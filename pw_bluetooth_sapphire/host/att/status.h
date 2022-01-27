// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_STATUS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_STATUS_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/att/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"

namespace bt {

template <>
struct ProtocolErrorTraits<att::ErrorCode> {
  static std::string ToString(att::ErrorCode ecode);

  static constexpr bool is_success(att::ErrorCode ecode) {
    return ecode == att::ErrorCode::kNoError;
  }
};

namespace att {

using Status = bt::Status<ErrorCode>;

// Copyable callback for reporting a Status.
using StatusCallback = fit::function<void(att::Status)>;

}  // namespace att
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_STATUS_H_
