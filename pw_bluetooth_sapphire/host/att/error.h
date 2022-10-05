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
using Result = fit::result<bt::att::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::att::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::att::Result<V...> result)>;

}  // namespace att

template <>
struct ProtocolErrorTraits<att::ErrorCode> {
  static std::string ToString(att::ErrorCode ecode);

  // is_success() not declared because ATT_ERROR_RSP does not encode a "success" value (Core Spec
  // v5.3, Vol 3, Part F, Section 3.4.1.1, Table 3.4).
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_
