// Copyright 2025 The Pigweed Authors
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

// TODO: https://pwbug.dev/434778918 - This file ensures that builtins that are
// widely assumed to be pre-declared are available without having to explicitly
// include <arm_acle.h> at the usage site.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#if __has_builtin(__wfi)
// Forward declare so we don't have to #include <arm_acle.h>.
void __wfi(void);
#endif  // __has_builtin(__wfi)

#if __has_builtin(__wfe)
// Forward declare so we don't have to #include <arm_acle.h>.
void __wfe(void);
#endif  // __has_builtin(__wfe)

#if __has_builtin(__sev)
// Forward declare so we don't have to #include <arm_acle.h>.
void __sev(void);
#endif  // __has_builtin(__sev)

#ifdef __cplusplus
}
#endif  // __cplusplus
