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

// Utilities for building std::byte arrays from strings or integer values.
#pragma once

#include <array>
#include <cstddef>

namespace pw {

template <typename T, typename... Args>
constexpr void CopyBytes(std::byte* array, T value, Args... args) {
  if constexpr (std::is_integral_v<T>) {
    if constexpr (sizeof(T) == 1u) {
      *array++ = static_cast<std::byte>(value);
    } else {
      for (size_t i = 0; i < sizeof(T); ++i) {
        *array++ = static_cast<std::byte>(value & 0xFF);
        value >>= 8;
      }
    }
  } else {
    static_assert(sizeof(value[0]) == sizeof(std::byte));
    for (auto b : value) {
      *array++ = static_cast<std::byte>(b);
    }
  }

  if constexpr (sizeof...(args) > 0u) {
    CopyBytes(array, args...);
  }
}

template <typename T>
constexpr size_t SizeOfBytes(const T& arg) {
  if constexpr (std::is_integral_v<T>) {
    return sizeof(arg);
  } else {
    return arg.size();
  }
}

// Converts a series of integers or byte arrays to a std::byte array at compile
// time.
template <typename... Args>
constexpr auto AsBytes(Args... args) {
  std::array<std::byte, (SizeOfBytes(args) + ...)> bytes{};

  auto iterator = bytes.begin();
  CopyBytes(iterator, args...);

  return bytes;
}

template <size_t kSize>
constexpr auto InitializedBytes(uint8_t value) {
  std::array<std::byte, kSize> bytes{};
  for (std::byte& b : bytes) {
    b = std::byte(value);
  }
  return bytes;
}

namespace internal {

template <typename T, size_t... kIndex>
constexpr auto ByteStr(const T& array, std::index_sequence<kIndex...>) {
  return std::array{static_cast<std::byte>(array[kIndex])...};
}

}  // namespace internal

// Converts a string literal to a byte array, without the trailing '\0'.
template <size_t kSize, typename Indices = std::make_index_sequence<kSize - 1>>
constexpr auto ByteStr(const char (&str)[kSize]) {
  return internal::ByteStr(str, Indices{});
}

}  // namespace pw
