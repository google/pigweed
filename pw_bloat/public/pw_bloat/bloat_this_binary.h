// Copyright 2019 The Pigweed Authors
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

#include <cstdint>

/// Binary size reports library
namespace pw::bloat {

// Function providing fundamental C/C++ functions to prevent them from appearing
// in size reports. Must be called in binaries which are compared to the bloat
// base in order to get accurate reports.
void BloatThisBinary();

/// @module{pw_bloat}

/// A value that can cause all conditions passed to `PW_BLOAT_COND` and
/// expressions passed to `PW_BLOAT_EXPR` to be executed. Assign a volatile
/// variable to this value and pass it those macros to prevent unwanted compiler
/// optimizations from removing code to be measured.
[[maybe_unused]] constexpr uint32_t kDefaultMask = ~0u;

/// Possibly evaluates a conditional statement as part of a size report.
///
/// The `mask` parameter is treated as a bitmap. If the least significant bit is
/// set, the condition is evaluated and, if true, the bit is recycled to the
/// most significant position. Otherwise, the bit is discarded.
///
/// A clever compiler should be kept from optimizing away the conditional
/// statements by initializing the `mask` parameter with a volatile variable:
///
/// @code{.cpp}
///   volatile uint32_t mask = pw::bloat::kDefaultMask;
///   MyObject my_obj;
///   PW_BLOAT_COND(my_obj.is_in_some_state(), mask);
/// @endcode
///
/// If a method returns void and is called for its side effects, use
/// `PW_BLOAT_EXPR` instead.
#define PW_BLOAT_COND(cond, mask)     \
  do {                                \
    if ((mask & 1) != 0 && (cond)) {  \
      mask = (mask >> 1) | 0x8000000; \
    } else {                          \
      mask >>= 1;                     \
    }                                 \
  } while (0)

/// Possibly evaluates an expression as part of a size report.
///
/// The `mask` parameter is treated as a bitmap. If the least significant bit is
/// set, the expression is evaluated and the bit is recycled to the most
/// significant position. Otherwise, the bit is discarded.
///
/// A clever compiler should be kept from optimizing away the expression by
/// initializing the `mask` parameter with a volatile variable, provided the
/// method has some side effect:
///
/// @code{.cpp}
///   volatile uint32_t mask = pw::bloat::kDefaultMask;
///   MyObject my_obj;
///   PW_BLOAT_EXPR(my_obj.some_method(), mask);
/// @endcode
///
/// If a method is const or has no effect beyond its return value, use
/// `PW_BLOAT_COND` instead.
#define PW_BLOAT_EXPR(expr, mask)     \
  do {                                \
    if ((mask & 1) != 0) {            \
      (expr);                         \
      mask = (mask >> 1) | 0x8000000; \
    } else {                          \
      mask >>= 1;                     \
    }                                 \
  } while (0)

}  // namespace pw::bloat
