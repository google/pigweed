// Copyright 2022 The Pigweed Authors
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

// pw::span is an implementation of std::span for C++14 or newer. The
// implementation is shared with the std::span polyfill class.
#pragma once

#if __has_include(<version>)
#include <version>
#endif  // __has_include(<version>)

// If the C++ library fully supports <span>, pw::span is an alias of std::span.
#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L || \
    defined(_PW_SPAN_POLYFILL_ENABLED)

#include <span>

namespace pw {

using std::as_bytes;
using std::as_writable_bytes;
using std::dynamic_extent;
using std::span;

}  // namespace pw

#else

#define _PW_SPAN_COMMON_NAMEPACE_BEGIN namespace pw {
#define _PW_SPAN_COMMON_NAMEPACE_END }  // namespace pw

#include "pw_span/internal/span_common.inc"

#undef _PW_SPAN_COMMON_NAMEPACE_BEGIN
#undef _PW_SPAN_COMMON_NAMEPACE_END

#endif  // defined(__cpp_lib_span) && __cpp_lib_span >= 202002L
