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

// This is mostly a compile test to verify that the log backend is able to
// compile the constructs promised by the logging facade; and that when run,
// there is no crash.
//
// TODO(pwbug/88): Add verification of the actually logged statements.

// clang-format off
#define PW_ASSERT_USE_SHORT_NAMES 1
#include "pw_assert/assert.h"
// clang-format on

#include "gtest/gtest.h"

// This is a global constant to feed into the formatter for tests.
// Intended to pair with FAIL_IF_DISPLAYED_ARGS or FAIL_IF_HIDDEN_ARGS.
static const int z = 10;

// At some point in the future when there is a proper test system in place for
// crashing, the below strings can help indicate pass/fail for a check.

#define FAIL_IF_DISPLAYED "FAIL IF DISPLAYED"
#define FAIL_IF_DISPLAYED_ARGS "FAIL IF DISPLAYED: %d"

#define FAIL_IF_HIDDEN "FAIL IF HIDDEN"
#define FAIL_IF_HIDDEN_ARGS "FAIL IF HIDDEN: %d"

// This switch exists to support compiling and/or running the tests.
#define DISABLE_ASSERT_TEST_EXECUTION 1
#if DISABLE_ASSERT_TEST_EXECUTION
#define MAYBE_SKIP_TEST return
#else
#define MAYBE_SKIP_TEST ;
#endif

namespace {

TEST(Crash, WithAndWithoutMessageArguments) {
  MAYBE_SKIP_TEST;
  PW_CRASH(FAIL_IF_HIDDEN);
  PW_CRASH(FAIL_IF_HIDDEN_ARGS, z);
}

TEST(Check, NoMessage) {
  MAYBE_SKIP_TEST;
  PW_CHECK(true);
  PW_CHECK(false);
}

TEST(Check, WithMessageAndArgs) {
  MAYBE_SKIP_TEST;
  PW_CHECK(true, FAIL_IF_DISPLAYED);
  PW_CHECK(true, FAIL_IF_DISPLAYED_ARGS, z);

  PW_CHECK(false, FAIL_IF_HIDDEN);
  PW_CHECK(false, FAIL_IF_HIDDEN_ARGS, z);
}

TEST(Check, IntComparison) {
  MAYBE_SKIP_TEST;
  int x_int = 50;
  int y_int = 66;

  PW_CHECK_INT_LE(x_int, y_int);
  PW_CHECK_INT_LE(x_int, y_int, "INT: " FAIL_IF_DISPLAYED);
  PW_CHECK_INT_LE(x_int, y_int, "INT: " FAIL_IF_DISPLAYED_ARGS, z);

  PW_CHECK_INT_GE(x_int, y_int);
  PW_CHECK_INT_GE(x_int, y_int, "INT: " FAIL_IF_HIDDEN);
  PW_CHECK_INT_GE(x_int, y_int, "INT: " FAIL_IF_HIDDEN_ARGS, z);
}

TEST(Check, UintComparison) {
  MAYBE_SKIP_TEST;
  unsigned int x_uint = 50;
  unsigned int y_uint = 66;

  PW_CHECK_UINT_LE(x_uint, y_uint);
  PW_CHECK_UINT_LE(x_uint, y_uint, "UINT: " FAIL_IF_DISPLAYED);
  PW_CHECK_UINT_LE(x_uint, y_uint, "UINT: " FAIL_IF_DISPLAYED_ARGS, z);

  PW_CHECK_UINT_GE(x_uint, y_uint);
  PW_CHECK_UINT_GE(x_uint, y_uint, "UINT: " FAIL_IF_HIDDEN);
  PW_CHECK_UINT_GE(x_uint, y_uint, "UINT: " FAIL_IF_HIDDEN_ARGS, z);
}

TEST(Check, FloatComparison) {
  MAYBE_SKIP_TEST;
  float x_float = 50.5;
  float y_float = 66.5;

  PW_CHECK_FLOAT_LE(x_float, y_float);
  PW_CHECK_FLOAT_LE(x_float, y_float, "FLOAT: " FAIL_IF_DISPLAYED);
  PW_CHECK_FLOAT_LE(x_float, y_float, "FLOAT: " FAIL_IF_DISPLAYED_ARGS, z);

  PW_CHECK_FLOAT_GE(x_float, y_float);
  PW_CHECK_FLOAT_GE(x_float, y_float, "FLOAT: " FAIL_IF_HIDDEN);
  PW_CHECK_FLOAT_GE(x_float, y_float, "FLOAT: " FAIL_IF_HIDDEN_ARGS, z);
}

static int Add3(int a, int b, int c) { return a + b + c; }

TEST(Check, ComparisonArgumentsWithCommas) {
  MAYBE_SKIP_TEST;
  int x_int = 50;
  int y_int = 66;

  PW_CHECK_INT_LE(Add3(1, 2, 3), y_int);
  PW_CHECK_INT_LE(x_int, Add3(1, 2, 3));

  PW_CHECK_INT_LE(Add3(1, 2, 3), y_int, FAIL_IF_DISPLAYED);
  PW_CHECK_INT_LE(x_int, Add3(1, 2, 3), FAIL_IF_DISPLAYED_ARGS, z);

  PW_CHECK_INT_LE(Add3(1, 2, 3), Add3(1, 2, 3), "INT: " FAIL_IF_DISPLAYED);
  PW_CHECK_INT_LE(x_int, y_int, "INT: " FAIL_IF_DISPLAYED_ARGS, z);
}

// These are defined in assert_test.c, to test C compatibility.
extern "C" {
void AssertTestsInC();
}  // extern "C"

TEST(Check, AssertTestsInC) {
  MAYBE_SKIP_TEST;
  AssertTestsInC();
}

static int global_state_for_multi_evaluate_test;
static int IncrementsGlobal() {
  global_state_for_multi_evaluate_test++;
  return 0;
}

// This test verifies that the binary CHECK_*(x,y) macros only
// evaluate their arguments once.
TEST(Check, BinaryOpOnlyEvaluatesOnce) {
  MAYBE_SKIP_TEST;

  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(0, IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);

  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(IncrementsGlobal(), IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 2);

  // Fails; should only evaluate IncrementGlobal() once.
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(1, IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);

  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(IncrementsGlobal(), 1 + IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 2);
}

// Note: This requires enabling PW_ASSERT_USE_SHORT_NAMES 1 above.
TEST(Check, ShortNamesWork) {
  MAYBE_SKIP_TEST;

  // Crash
  CRASH(FAIL_IF_HIDDEN);
  CRASH(FAIL_IF_HIDDEN_ARGS, z);

  // Check
  CHECK(true, FAIL_IF_DISPLAYED);
  CHECK(true, FAIL_IF_DISPLAYED_ARGS, z);
  CHECK(false, FAIL_IF_HIDDEN);
  CHECK(false, FAIL_IF_HIDDEN_ARGS, z);

  // Check with binary comparison
  int x_int = 50;
  int y_int = 66;

  CHECK_INT_LE(Add3(1, 2, 3), y_int);
  CHECK_INT_LE(x_int, Add3(1, 2, 3));

  CHECK_INT_LE(Add3(1, 2, 3), y_int, FAIL_IF_DISPLAYED);
  CHECK_INT_LE(x_int, Add3(1, 2, 3), FAIL_IF_DISPLAYED_ARGS, z);

  CHECK_INT_LE(Add3(1, 2, 3), Add3(1, 2, 3), "INT: " FAIL_IF_DISPLAYED);
  CHECK_INT_LE(x_int, y_int, "INT: " FAIL_IF_DISPLAYED_ARGS, z);
}
}  // namespace
