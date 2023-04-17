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

#if __has_include(<version>)
#include <version>
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
// No polyfill for <expected>.

#include <expected>

namespace pw {

using std::expected;
using std::unexpect;
using std::unexpect_t;
using std::unexpected;

}  // namespace pw

#else
// Provide polyfill for <expected>.

#include "pw_result/internal/expected_impl.h"

namespace pw {

using internal::expected;
using internal::unexpect;
using internal::unexpect_t;
using internal::unexpected;

}  // namespace pw

#endif  // defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
