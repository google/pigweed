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

// Macros for adapting to older versions of C++. A few keywords (consteval,
// constinit) are handled by pw_polfyill/language_features.h, which is directly
// -included by users of pw_polyfill.
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
