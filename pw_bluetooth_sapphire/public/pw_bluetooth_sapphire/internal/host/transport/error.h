// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ERROR_H_

#include <pw_bluetooth/hci_common.emb.h>

#include "pw_bluetooth_sapphire/internal/host/common/error.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"

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
