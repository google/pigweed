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

// Macros for cleanly working with Status or StatusWithSize objects.
#define PW_TRY(expr) _PW_TRY(_PW_TRY_UNIQUE(__LINE__), expr)

#define _PW_TRY(result, expr)                 \
  do {                                        \
    if (auto result = (expr); !result.ok()) { \
      return result;                          \
    }                                         \
  } while (0)

#define PW_TRY_ASSIGN(assignment_lhs, expression) \
  _PW_TRY_ASSIGN(_PW_TRY_UNIQUE(__LINE__), assignment_lhs, expression)

#define _PW_TRY_ASSIGN(result, lhs, expr) \
  auto result = (expr);                   \
  if (!result.ok()) {                     \
    return result;                        \
  }                                       \
  lhs = std::move(result.size())

#define _PW_TRY_UNIQUE(line) _PW_TRY_UNIQUE_EXPANDED(line)
#define _PW_TRY_UNIQUE_EXPANDED(line) _pw_try_unique_name_##line

#define TRY PW_TRY
#define TRY_ASSIGN PW_TRY_ASSIGN
