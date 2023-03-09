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

// todo-check: ignore
// TODO(fxbug.dev/120998): Once this bug is addressed, this module can likely
// be removed and we could just inline the using statements.

#include <atomic>
#include <limits>
#include <type_traits>

namespace pw {

#if __cplusplus >= 202002L
using bit_ceil = std::bit_ceil;
#else
constexpr size_t countl_zero(size_t x) noexcept {
  size_t size_digits = std::numeric_limits<size_t>::digits;

  if (sizeof(x) <= sizeof(unsigned int))
    return __builtin_clz(static_cast<unsigned int>(x)) -
           (std::numeric_limits<unsigned int>::digits - size_digits);

  if (sizeof(x) <= sizeof(unsigned long))
    return __builtin_clzl(static_cast<unsigned long>(x)) -
           (std::numeric_limits<unsigned long>::digits - size_digits);

  static_assert(sizeof(x) <= sizeof(unsigned long long));
  return __builtin_clzll(static_cast<unsigned long long>(x)) -
         (std::numeric_limits<unsigned long long>::digits - size_digits);
}

constexpr size_t bit_width(size_t x) noexcept {
  return std::numeric_limits<size_t>::digits - countl_zero(x);
}

constexpr size_t bit_ceil(size_t x) noexcept {
  if (x == 0)
    return 1;
  return size_t{1} << bit_width(size_t{x - 1});
}
#endif

// The NaturallyAligned class is a wrapper class for ensuring the object is
// aligned to a power of 2 bytes greater than or equal to its size.
template <typename T>
struct [[gnu::aligned(bit_ceil(sizeof(T)))]] NaturallyAligned
    : public T{NaturallyAligned() : T(){} NaturallyAligned(const T& t) :
                   T(t){} template <class U>
                   NaturallyAligned(const U& u) : T(u){} NaturallyAligned
                   operator=(T other){return T::operator=(other);
}  // namespace pw
}
;

// This is a convenience wrapper for ensuring the object held by std::atomic is
// naturally aligned. Ensuring the underlying objects's alignment is natural
// allows clang to replace libcalls to atomic functions
// (__atomic_load/store/exchange/etc) with native instructions when appropriate.
//
// Example usage:
//
//   // Here std::optional<bool> has a size of 2 but alignment of 1, which would
//   // normally lower to an __atomic_* libcall, but pw::NaturallyAligned in
//   // std::atomic tells the compiler to align the object to 2 bytes, which
//   // satisfies the requirements for replacing __atomic_* with instructions.
//   pw::AlignedAtomic<std::optional<bool>> mute_enable{};
//
template <typename T>
using AlignedAtomic = std::atomic<NaturallyAligned<T>>;

}  // namespace pw
