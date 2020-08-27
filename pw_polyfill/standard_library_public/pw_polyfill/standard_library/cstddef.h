// Copyright 2020 The Pigweed Authors
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

#include <cstddef>

#include "pw_polyfill/standard_library/namespace.h"

// Defines the std::byte type if it is not present.
#ifndef __cpp_lib_byte
#define __cpp_lib_byte 201603L

_PW_POLYFILL_BEGIN_NAMESPACE_STD

enum class byte : unsigned char {};

template <typename I>
constexpr I to_integer(byte b) noexcept {
  return I(b);
}

constexpr byte operator|(byte l, byte r) noexcept {
  return byte(static_cast<unsigned int>(l) | static_cast<unsigned int>(r));
}

constexpr byte operator&(byte l, byte r) noexcept {
  return byte(static_cast<unsigned int>(l) & static_cast<unsigned int>(r));
}

constexpr byte operator^(byte l, byte r) noexcept {
  return byte(static_cast<unsigned int>(l) ^ static_cast<unsigned int>(r));
}

constexpr byte operator~(byte b) noexcept {
  return byte(~static_cast<unsigned int>(b));
}

template <typename I>
constexpr byte operator<<(byte b, I shift) noexcept {
  return byte(static_cast<unsigned int>(b) << shift);
}

template <typename I>
constexpr byte operator>>(byte b, I shift) noexcept {
  return byte(static_cast<unsigned int>(b) >> shift);
}

#if __cpp_constexpr >= 201304L

constexpr byte& operator|=(byte& l, byte r) noexcept { return l = l | r; }
constexpr byte& operator&=(byte& l, byte r) noexcept { return l = l & r; }
constexpr byte& operator^=(byte& l, byte r) noexcept { return l = l ^ r; }

#else  // For C++11 compatiblity, these operators cannot be constexpr.

inline byte& operator|=(byte& l, byte r) noexcept { return l = l | r; }
inline byte& operator&=(byte& l, byte r) noexcept { return l = l & r; }
inline byte& operator^=(byte& l, byte r) noexcept { return l = l ^ r; }

#endif  // __cpp_constexpr >= 201304L

template <typename I>
inline byte& operator<<=(byte& b, I shift) noexcept {
  return b = b << shift;
}

template <typename I>
inline byte& operator>>=(byte& b, I shift) noexcept {
  return b = b >> shift;
}

_PW_POLYFILL_END_NAMESPACE_STD

#endif  // __cpp_lib_byte
