// Copyright 2024 The Pigweed Authors
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

// Provide static_assert as a macro before C23.
#if !defined(__cplusplus) && __STDC_VERSION__ < 202311L

#undef static_assert  // Undefine the macro in case <assert.h> is included.

// Define static_assert to support an optional message argument, as in C23.
#define static_assert(expression, ...) \
  _Static_assert(expression, "(" #expression ") " __VA_ARGS__)

// _Static_assert was not added until C11, but modern compilers support it for
// all C standards. GCC has supported it since 4.6, for example.

#endif  // !defined(__cplusplus) && __STDC_VERSION__ < 202311L
