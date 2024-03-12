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

// Macros for using C++ features in older standards.
#pragma once

// Mark functions as constexpr if C++20 or newer
#if __cplusplus >= 202002L
#define PW_CONSTEXPR_CPP20 constexpr
#else
#define PW_CONSTEXPR_CPP20
#endif  // __cpp_constexpr >= 201304L

// Mark functions as consteval if supported.
#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
#define PW_CONSTEVAL consteval
#else
#define PW_CONSTEVAL constexpr
#endif  // __cpp_consteval >= 201811L

// Mark functions as constinit if supported by the compiler.
#if defined(__cpp_constinit) && __cpp_constinit >= 201907L
#define PW_CONSTINIT constinit
#elif defined(__clang__)
#define PW_CONSTINIT [[clang::require_constant_initialization]]
#elif defined(__GNUC__) && __GNUC__ >= 13
#define PW_CONSTINIT __constinit
#else
#define PW_CONSTINIT
#endif  // __cpp_constinit

// nodiscard with a string literal is only available in later versions (~C++20).
#if __cplusplus >= 202002L
#define PW_NODISCARD_STR(str) [[nodiscard(str)]]
#else
#define PW_NODISCARD_STR(str) [[nodiscard]]
#endif
