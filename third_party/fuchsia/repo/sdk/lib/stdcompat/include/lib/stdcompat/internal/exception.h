// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_INCLUDE_LIB_STDCOMPAT_INTERNAL_EXCEPTION_H_
#define LIB_STDCOMPAT_INCLUDE_LIB_STDCOMPAT_INTERNAL_EXCEPTION_H_

#include <exception>
#include <type_traits>

namespace cpp17 {
namespace internal {

// When exceptions are enabled, will generate an exception of the right type, when disabled will
// simply abort execution.
//
// Note: both clang and gcc support gnu::unused, which makes it a portable alternative for
// [[maybe_unused]].
template <typename T,
          typename std::enable_if<std::is_base_of<std::exception, T>::value, bool>::type = true>
[[noreturn]] inline constexpr void throw_or_abort([[gnu::unused]] const char* reason) {
#if defined(__cpp_exceptions) && __cpp_exceptions >= 199711L
  throw T(reason);
#else
  __builtin_abort();
#endif
}

}  // namespace internal
}  // namespace cpp17

#endif  // LIB_STDCOMPAT_INCLUDE_LIB_STDCOMPAT_INTERNAL_EXCEPTION_H_
