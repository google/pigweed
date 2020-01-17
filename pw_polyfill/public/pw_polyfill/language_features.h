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

#ifdef __cpp_inline_variables
#define PW_INLINE_VARIABLE inline
#else
#define PW_INLINE_VARIABLE
#endif  // __cpp_inline_variables

// Mark functions as constexpr if the relaxed constexpr rules are supported.
#if __cpp_constexpr >= 201304L
#define PW_CONSTEXPR_FUNCTION constexpr
#else
#define PW_CONSTEXPR_FUNCTION
#endif  // __cpp_constexpr >= 201304L

// This is an adapter for supporting static_assert with a single argument in
// C++11 or C++14. Macros don't correctly parse commas in template expressions,
// so the static_assert arguments are passed to an overloaded C++ function. The
// full stringified static_assert arguments are used as the message.
#if __cpp_static_assert < 201411L

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
