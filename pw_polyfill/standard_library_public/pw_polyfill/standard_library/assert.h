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

#include <assert.h>

// In C11, assert.h should define static_assert as _Static_assert.
#if !defined(__cplusplus) && !defined(static_assert)

#if __STDC_VERSION__ >= 201112L
#define static_assert _Static_assert
#else  // _Static_assert requires C11.
#define static_assert(...)
#endif  // __STDC_VERSION__ >= 201112L

#endif  // !defined(__cplusplus) && !defined(static_assert)
