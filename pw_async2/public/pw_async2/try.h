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

/// @module{pw_async2}

/// Returns `Poll::Pending()` if \a expr is `Poll::Pending()`.
#define PW_TRY_READY(expr)            \
  do {                                \
    if ((expr).IsPending()) {         \
      return ::pw::async2::Pending(); \
    }                                 \
  } while (0)

#define PW_TRY_READY_ASSIGN(lhs, expression) \
  _PW_TRY_READY_ASSIGN(_PW_TRY_READY_UNIQUE(__LINE__), lhs, expression)

/// Returns `Poll::Pending()` if \a expr is `Poll::Pending()`. If expression
/// is `Poll::Ready()`, assigns the inner value to \a lhs.
#define _PW_TRY_READY_ASSIGN(result, lhs, expr) \
  auto&& result = (expr);                       \
  if (result.IsPending()) {                     \
    return ::pw::async2::Pending();             \
  }                                             \
  lhs = std::move(*result)

#define _PW_TRY_READY_UNIQUE(line) _PW_TRY_READY_UNIQUE_EXPANDED(line)
#define _PW_TRY_READY_UNIQUE_EXPANDED(line) _pw_try_unique_name_##line
