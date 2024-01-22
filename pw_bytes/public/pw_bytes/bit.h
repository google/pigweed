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

// Low-level bit operations including std::endian from C++20.
#pragma once

#include <climits>

#include "lib/stdcompat/bit.h"

namespace pw {

using ::cpp20::endian;

namespace bytes {

/// Extends the nth bit to the left. Useful for expanding singed values into
/// larger integer types.
template <std::size_t kBitWidth, typename T>
constexpr T SignExtend(T nbit_value) {
  static_assert(std::is_integral_v<T>);
  static_assert(kBitWidth < (sizeof(T) * CHAR_BIT));

  using SignedT = std::make_signed_t<T>;

  constexpr std::size_t extension_bits =
      (sizeof(SignedT) * CHAR_BIT) - kBitWidth;

  SignedT nbit_temp = static_cast<SignedT>(nbit_value);
  return ((nbit_temp << extension_bits) >> extension_bits);
}

}  // namespace bytes
}  // namespace pw
