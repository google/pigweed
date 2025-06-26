// Copyright 2025 The Pigweed Authors
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
#endif  // __has_include(<version>)

#ifdef __cpp_lib_saturation_arithmetic

#include <numeric>

namespace pw {

using std::add_sat;
using std::mul_sat;

}  // namespace pw

#else

#include <limits>
#include <type_traits>

#include "pw_preprocessor/compiler.h"

// Polyfills of C++26's saturating arithmetic operations in <numeric>.

namespace pw {

/// Polyfill of C++26's `std::add_sat`. Returns the sum of two integers, giving
/// the integer's maximum or minimum value if the sum would otherwise have
/// overflowed.
///
/// For example, `add_sat<uint8_t>(250, 6)` returns `255` instead of the
/// overflowed value (`0`).
template <typename T>
constexpr T add_sat(T lhs, T rhs) noexcept {
  static_assert(std::is_integral_v<T>);
  if (T sum = 0; !PW_ADD_OVERFLOW(lhs, rhs, &sum)) {
    return sum;
  }
  if constexpr (std::is_unsigned_v<T>) {
    return std::numeric_limits<T>::max();
  } else {  // since it overflowed, the operands had the same sign
    if (lhs < 0) {
      return std::numeric_limits<T>::min();
    }
    return std::numeric_limits<T>::max();
  }
}

/// Polyfill of C++26's `std::mul_sat`. Returns the product of two integers,
/// giving the integer's maximum or minimum value if the product would otherwise
/// have overflowed.
///
/// For example, for `100 * 10 = 1000`, `mul_sat<uint8_t>(100, 10)` returns
/// `255` instead of the overflowed value (`232`).
template <typename T>
constexpr T mul_sat(T lhs, T rhs) noexcept {
  static_assert(std::is_integral_v<T>);
  if (T product = 0; !PW_MUL_OVERFLOW(lhs, rhs, &product)) {
    return product;
  }
  if constexpr (std::is_unsigned_v<T>) {
    return std::numeric_limits<T>::max();
  } else {  // determine sign of result by comparing signs of the factors
    if ((lhs < 0) == (rhs < 0)) {
      return std::numeric_limits<T>::max();
    }
    return std::numeric_limits<T>::min();
  }
}

}  // namespace pw

#endif  // __cpp_lib_saturation_arithmetic
