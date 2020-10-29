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

// This file provides adapters for newer C++ language features so that they can
// be used in older versions of C++ (though the code will not function exactly
// the same). This file is not on an include path and is intended to be used
// with -include when compiling C++.
//
// pw_polyfill/language_feature_macros.h provides macro wrappers for a few
// specific uses of modern C++ keywords.
#pragma once

// C++11 is required for the features in this header.
#if defined(__cplusplus) && __cplusplus >= 201103L

// If consteval is not supported, use constexpr. This does not guarantee
// compile-time execution, but works equivalently in constant expressions.
#ifndef __cpp_consteval
#define consteval constexpr
#endif  // __cpp_consteval

// If constinit is not supported, use a compiler attribute or omit it. If
// omitted, the compiler may still constant initialize the variable, but there
// is no guarantee.
#ifndef __cpp_constinit
#ifdef __clang__
#define constinit [[clang::require_constant_initialization]]
#else
#define constinit
#endif  // __clang__
#endif  // __cpp_constinit

// This is an adapter for supporting static_assert with a single argument in
// C++11 or C++14. Macros don't correctly parse commas in template expressions,
// so the static_assert arguments are passed to an overloaded C++ function. The
// full stringified static_assert arguments are used as the message.
#if __cpp_static_assert < 201411L
#undef __cpp_static_assert
#define __cpp_static_assert 201411L

#define static_assert(...)                                                     \
  static_assert(::pw::polyfill::internal::StaticAssertExpression(__VA_ARGS__), \
                #__VA_ARGS__)

namespace pw {
namespace polyfill {
namespace internal {

constexpr bool StaticAssertExpression(bool expression) { return expression; }

constexpr bool StaticAssertExpression(bool expression, const char*) {
  return expression;
}

}  // namespace internal
}  // namespace polyfill
}  // namespace pw

#endif  // __cpp_static_assert < 201411L
#endif  // defined(__cplusplus) && __cplusplus >= 201103L
