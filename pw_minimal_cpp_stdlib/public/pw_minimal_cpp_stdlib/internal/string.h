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

#include "pw_polyfill/standard_library/namespace.h"

_PW_POLYFILL_BEGIN_NAMESPACE_STD

// NOT IMPLEMENTED: Some functions and overloads are missing.
template <typename T>
class char_traits {
 public:
  static constexpr void assign(T& dest, const T& source) noexcept {
    dest = source;
  }

  static constexpr T* assign(T* dest, decltype(sizeof(0)) count, T value) {
    for (decltype(sizeof(0)) i = 0; i < count; ++i) {
      dest[i] = value;
    }
    return dest;
  }

  static constexpr bool eq(T lhs, T rhs) noexcept { return lhs == rhs; }

  static constexpr bool lt(T lhs, T rhs) noexcept { return lhs < rhs; }

  static constexpr T* move(T* dest,
                           const T* source,
                           decltype(sizeof(0)) count) {
    if (dest < source) {
      copy(dest, source, count);
    } else if (source < dest) {
      for (decltype(sizeof(0)) i = count; i != 0; --i) {
        assign(dest[i - 1], source[i - 1]);
      }
    }
    return dest;
  }

  static constexpr T* copy(T* dest,
                           const T* source,
                           decltype(sizeof(0)) count) {
    for (decltype(sizeof(0)) i = 0; i < count; ++i) {
      char_traits<T>::assign(dest[i], source[i]);
    }
    return dest;
  }

  static constexpr int compare(const T* lhs,
                               const T* rhs,
                               decltype(sizeof(0)) count) {
    for (decltype(sizeof(0)) i = 0; i < count; ++i) {
      if (!eq(lhs[i], rhs[i])) {
        return lt(lhs[i], rhs[i]) ? -1 : 1;
      }
    }
    return 0;
  }
};

_PW_POLYFILL_END_NAMESPACE_STD
