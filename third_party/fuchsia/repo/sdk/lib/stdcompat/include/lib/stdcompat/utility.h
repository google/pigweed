// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_UTILITY_H_
#define LIB_STDCOMPAT_UTILITY_H_

#include <cstddef>
#include <type_traits>
#include <utility>

#include "internal/utility.h"
#include "version.h"

namespace cpp17 {
// Use alias for cpp17 and above.

using std::in_place;
using std::in_place_t;

using std::in_place_index;
using std::in_place_index_t;

using std::in_place_type;
using std::in_place_type_t;

using std::as_const;

}  // namespace cpp17

namespace cpp20 {

#if defined(__cpp_lib_constexpr_algorithms) && __cpp_lib_constexpr_algorithms >= 201806L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::exchange;

#else  // Add swap constexpr polyfill.

template <
    typename T, typename U = T,
    typename std::enable_if<std::is_move_assignable<T>::value && cpp17::is_assignable_v<T&, U>,
                            bool>::type = true>
constexpr T exchange(T& obj, U&& new_value) {
  T old = std::move(obj);
  obj = std::forward<U>(new_value);
  return old;
}

#endif  // __cpp_lib_constexpr_algorithms >= 201806L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

}  // namespace cpp20

#if defined(__cpp_lib_to_underlying) && __cpp_lib_to_underlying >= 202102L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

namespace cpp23 {

using std::to_underlying;

}  // namespace cpp23

#else  // Provide to_underlying polyfill.

namespace cpp23 {

// An implementation of C23 `std::to_underlying` for C++17 or newer.
// https://en.cppreference.com/w/cpp/utility/to_underlying
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p1682r2.html
template <typename Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

}  // namespace cpp23

#endif  // defined(__cpp_lib_to_underlying) && __cpp_lib_to_underlying >= 202102L &&
        // !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#endif  // LIB_STDCOMPAT_UTILITY_H_
