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

#pragma once
#include <lib/fit/function.h>

#include "pw_bluetooth_sapphire/internal/host/att/att.h"
#include "pw_bluetooth_sapphire/internal/host/common/error.h"

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

  // is_success() not declared because ATT_ERROR_RSP does not encode a "success"
  // value (Core Spec v5.3, Vol 3, Part F, Section 3.4.1.1, Table 3.4).
};

}  // namespace bt
