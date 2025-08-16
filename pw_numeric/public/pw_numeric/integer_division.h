// Copyright 2024 The Pigweed Authors
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

#include <type_traits>

namespace pw {

/// @module{pw_numeric}

/// Performs integer division and rounds to the nearest integer. Gives the same
/// result as `std::round(static_cast<double>(dividend) /
/// static_cast<double>(divisor))`, but requires no floating point operations
/// and is `constexpr`.
///
/// @rst
/// .. warning::
///
///    ``signed`` or ``unsigned`` ``int``, ``long``, or ``long long`` operands
///    overflow if:
///
///    - the quotient is positive and ``dividend + divisor/2`` overflows, or
///    - the quotient is negative and ``dividend - divisor/2`` overflows.
///
///    To avoid overflow, do not use this function with very large
///    operands, or cast ``int`` or ``long`` to a larger type first. Overflow
///    cannot occur with types smaller than ``int`` because C++ implicitly
///    converts them to ``int``.
/// @endrst
template <typename T>
constexpr T IntegerDivisionRoundNearest(T dividend, T divisor) {
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>);
  // Integer division typically truncates towards zero, so change the direction
  // of the bias when the quotient is negative.
  if constexpr (std::is_signed_v<T>) {
    if ((dividend < 0) != (divisor < 0)) {
      return (dividend - divisor / T{2}) / divisor;
    }
  }

  return (dividend + divisor / T{2}) / divisor;
}

}  // namespace pw
