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
#include "pw_status/status_with_size.h"

/// PW_TEST_EXPECT_OK
/// Verifies that `expr` is OkStatus()
///
/// Converts `expr` to a Status value and checks that it is OkStatus().
///
/// @param[in] expr The expression to check.
#define PW_TEST_EXPECT_OK(expr) \
  EXPECT_EQ(::pw::internal::ConvertToStatus(expr), pw::OkStatus())

/// PW_TEST_ASSERT_OK
/// See `PW_TEST_EXPECT_OK`.
#define PW_TEST_ASSERT_OK(expr) \
  ASSERT_EQ(::pw::internal::ConvertToStatus(expr), pw::OkStatus())

/// PW_TEST_ASSERT_OK_AND_ASSIGN
///
/// Executes an expression that returns a `pw::Result` or `pw::StatusWithSize`
/// and assigns or moves that value to lhs if the error code is OK. If the
/// status is non-OK, generates a test failure and returns from the current
/// function, which must have a void return type.
///
/// Example: Declaring and initializing a new value. E.g.:
///   PW_TEST_ASSERT_OK_AND_ASSIGN(auto value, MaybeGetValue(arg));
///   PW_TEST_ASSERT_OK_AND_ASSIGN(const ValueType value, MaybeGetValue(arg));
///
/// Example: Assigning to an existing value
///   ValueType value;
///   PW_TEST_ASSERT_OK_AND_ASSIGN(value, MaybeGetValue(arg));
///
/// The value assignment example would expand into something like:
///   auto status_or_value = MaybeGetValue(arg);
///   PW_TEST_ASSERT_OK(status_or_value.status());
///   value = status_or_value.ValueOrDie();
///
/// WARNING: PW_TEST_ASSERT_OK_AND_ASSIGN expand into multiple statements; it
/// cannot be used in a single statement (e.g. as the body of an if statement
/// without {})!
///
/// @param [in] lhs Variable to assign unwrapped value to
/// @param [in] rexpr Expression to unwrap. Must be a `Result` or
/// `StatusWithSize`.
#define PW_TEST_ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  _PW_TEST_ASSERT_OK_AND_ASSIGN_DETAIL(          \
      _PW_UNIQUE_IDENTIFIER_DETAIL(__LINE__), lhs, rexpr)

#define _PW_TEST_ASSERT_OK_AND_ASSIGN_DETAIL(result, lhs, rexpr) \
  auto result = (rexpr);                                         \
  PW_TEST_ASSERT_OK(result);                                     \
  lhs = ::pw::internal::ConvertToValue(result)

#define _PW_UNIQUE_IDENTIFIER_DETAIL(line) \
  _PW_UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line)
#define _PW_UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line) \
  _assert_pw_test_assert_ok_and_assign_unique_name_##line
