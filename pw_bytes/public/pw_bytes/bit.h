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

/// Queries size of the object or type in bits.
#define SIZE_OF_IN_BIT(...) (sizeof(__VA_ARGS__) * CHAR_BIT)

/// Extends the nth bit to the left. Useful for expanding singed values into
/// larger integer types.
template <std::size_t kBitWidth, typename T>
constexpr T SignExtend(T nbit_value) {
  static_assert(std::is_integral_v<T>);
  static_assert(kBitWidth < SIZE_OF_IN_BIT(T));

  using SignedT = std::make_signed_t<T>;

  constexpr std::size_t extension_bits = SIZE_OF_IN_BIT(SignedT) - kBitWidth;

  SignedT nbit_temp = static_cast<SignedT>(nbit_value);
  return ((nbit_temp << extension_bits) >> extension_bits);
}

/// Extracts bits between msb and lsb from a value.
///
/// @tparam     OutType   The type of output number to be extracted from input
/// number.
/// @tparam     kMsb      The left bit (included) that extraction starts at.
/// @tparam     kLsb      The right bit (included) that extraction ends at.
/// @tparam     InType    The type of input number.
/// @param[in]  value     The input number.
///
/// Example (extrat bits between 10 and 5 from a uint32_t and return as a
/// uint8_t):
///
/// @code
///   constexpr uint32_t number = 0xA0A0A0A0;
///   constexpr uint8_t extracted_number = ExtractBits<uint8_t, 10, 5>(number);
/// @endcode
template <typename OutType, std::size_t kMsb, std::size_t kLsb, typename InType>
constexpr OutType ExtractBits(InType value) {
  static_assert(kMsb >= kLsb);
  static_assert(kMsb < SIZE_OF_IN_BIT(InType));

  constexpr std::size_t kBitWidth = kMsb - kLsb + 1;
  static_assert(kBitWidth <= SIZE_OF_IN_BIT(OutType));

  if constexpr (kBitWidth == SIZE_OF_IN_BIT(InType)) {
    return OutType(value);
  } else {
    constexpr OutType mask = OutType((OutType(1) << kBitWidth) - 1);
    return OutType((value >> kLsb) & mask);
  }
}

}  // namespace bytes
}  // namespace pw
