// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_

#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"

namespace bt {
namespace hci {

using Error = Error<hci_spec::StatusCode>;

template <typename... V>
using Result = fitx::result<bt::hci::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::hci::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::hci::Result<V...> result)>;

}  // namespace hci

// Specializations for hci_spec::StatusCode.
template <>
struct ProtocolErrorTraits<hci_spec::StatusCode> {
  static std::string ToString(hci_spec::StatusCode ecode);

  static constexpr bool is_success(hci_spec::StatusCode ecode) {
    return ecode == hci_spec::StatusCode::kSuccess;
  }
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_
