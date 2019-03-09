// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_STATUS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_STATUS_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace btlib {
namespace common {

template <>
struct ProtocolErrorTraits<sm::ErrorCode> {
  static std::string ToString(sm::ErrorCode ecode);
};

}  // namespace common

namespace sm {

using Status = common::Status<ErrorCode>;

using StatusCallback = fit::function<void(Status)>;

}  // namespace sm
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_STATUS_H_
