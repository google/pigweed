// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_FUNCTIONAL_H_
#define LIB_STDCOMPAT_FUNCTIONAL_H_

#include "internal/functional.h"

namespace cpp20 {

// This version is always constexpr-qualified, with no other changes from C++17.

#if defined(__cpp_lib_invoke) && defined(__cpp_lib_constexpr_functional) &&     \
    __cpp_lib_invoke >= 201411L && __cpp_lib_constexpr_functional >= 201907L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::invoke;

#else  // Provide invoke() polyfill

template <typename F, typename... Args>
constexpr cpp17::invoke_result_t<F, Args...> invoke(F&& f, Args&&... args) noexcept(
    cpp17::is_nothrow_invocable_v<F, Args...>) {
  return ::cpp17::internal::invoke(std::forward<F>(f), std::forward<Args>(args)...);
}

#endif  // __cpp_lib_invoke >= 201411L && __cpp_lib_constexpr_functional >= 201907L &&
        // !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#if defined(__cpp_lib_bind_front) && __cpp_lib_bind_front >= 201907L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::bind_front;

#else  // Provide bind_front() polyfill

template <typename F, typename... Args>
constexpr ::cpp20::internal::front_binder_t<F, Args...> bind_front(F&& f, Args&&... args) noexcept(
    cpp17::is_nothrow_constructible_v<::cpp20::internal::front_binder_t<F, Args...>,
                                      cpp17::in_place_t, F, Args...>) {
  return ::cpp20::internal::front_binder_t<F, Args...>(cpp17::in_place, std::forward<F>(f),
                                                       std::forward<Args>(args)...);
}

#endif  // __cpp_lib_bind_front >= 201907L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

}  // namespace cpp20

#endif  // LIB_STDCOMPAT_FUNCTIONAL_H_
