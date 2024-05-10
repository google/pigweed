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

#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

// TODO(b/338094795): Remove when these are available in the light framework.

#ifndef ASSERT_OK
#define ASSERT_OK(expr) ASSERT_EQ(expr, pw::OkStatus())
#endif  // ASSERT_OK

#ifndef EXPECT_OK
#define EXPECT_OK(expr) EXPECT_EQ(expr, pw::OkStatus())
#endif  // EXPECT_OK

#ifndef ASSERT_OK_AND_ASSIGN

#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_DETAIL(UNIQUE_IDENTIFIER_DETAIL(__LINE__), lhs, rexpr)

// NOLINTBEGIN(bugprone-macro-parentheses)
// The suggestion would produce bad code.
#define ASSERT_OK_AND_ASSIGN_DETAIL(result, lhs, rexpr) \
  auto result = (rexpr);                                \
  ASSERT_EQ(result.status(), pw::OkStatus());           \
  lhs = ::pw::internal::ConvertToValue(result)

#define UNIQUE_IDENTIFIER_DETAIL(line) UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line)
#define UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line) \
  _assert_ok_and_assign_unique_name_##line

#endif  // ASSERT_OK_AND_ASSIGN
