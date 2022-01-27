// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"

namespace bt::att {

using Error = Error<bt::att::ErrorCode>;

template <typename... V>
using Result = fitx::result<bt::att::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::att::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::att::Result<V...> result)>;

}  // namespace bt::att

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_ATT_ERROR_H_
