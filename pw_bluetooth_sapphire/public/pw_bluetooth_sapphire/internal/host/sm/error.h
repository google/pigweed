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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ERROR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ERROR_H_

#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/common/error.h"
#include "pw_bluetooth_sapphire/internal/host/sm/smp.h"

namespace bt {
namespace sm {

using Error = Error<bt::sm::ErrorCode>;

template <typename... V>
using Result = fit::result<bt::sm::Error, V...>;

template <typename... V>
using ResultFunction = fit::function<void(bt::sm::Result<V...> result)>;

template <typename... V>
using ResultCallback = fit::callback<void(bt::sm::Result<V...> result)>;

}  // namespace sm

template <>
struct ProtocolErrorTraits<sm::ErrorCode> {
  static std::string ToString(sm::ErrorCode ecode);

  // is_success() not declared because the reason does not include a "success"
  // value (Core Spec v5.3, Vol 3, Part H, 3.5.5, Table 3.7).
};

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ERROR_H_
