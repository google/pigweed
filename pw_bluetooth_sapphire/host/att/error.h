// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"

namespace bt {
namespace att {

using Error = Error<bt::att::ErrorCode>;

template <typename... V>
using Result = fitx::result<bt::att::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::att::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::att::Result<V...> result)>;

}  // namespace att

template <>
struct ProtocolErrorTraits<att::ErrorCode> {
  static std::string ToString(att::ErrorCode ecode);

  static constexpr bool is_success(att::ErrorCode ecode) {
    return ecode == att::ErrorCode::kNoError;
  }
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_
