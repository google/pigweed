// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_

#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/hci-protocol.emb.h"

namespace bt {
namespace hci {

using Error = Error<pw::bluetooth::emboss::StatusCode>;

template <typename... V>
using Result = fit::result<bt::hci::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::hci::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::hci::Result<V...> result)>;

}  // namespace hci

// Specializations for pw::bluetooth::emboss::StatusCode.
template <>
struct ProtocolErrorTraits<pw::bluetooth::emboss::StatusCode> {
  static std::string ToString(pw::bluetooth::emboss::StatusCode ecode);

  static constexpr bool is_success(pw::bluetooth::emboss::StatusCode ecode) {
    return ecode == pw::bluetooth::emboss::StatusCode::SUCCESS;
  }
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_
