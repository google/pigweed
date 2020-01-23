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

// This is "test" verifies that the assert backend:
//
// - Compiles as plain C
//
// Unfortunately, this doesn't really test the crashing functionality since
// that is so backend dependent.
//
// NOTE: To run these tests for pw_assert_basic, you must modify two things:
//
//   (1) Set DISABLE_ASSERT_TEST_EXECUTION 1 in assert_test.cc (this file)
//   (1) Set DISABLE_ASSERT_TEST_EXECUTION 1 in assert_test.c
//   (2) Set PW_ASSERT_BASIC_DISABLE_NORETURN 1 in assert_basic.h
//
// This is obviously not a long term solution.

#include "pw_assert/assert.h"

#ifdef __cplusplus
#error "This file must be compiled as plain C to verify C compilation works."
#endif  // __cplusplus

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

static int Add3(int a, int b, int c) { return a + b + c; }

void AssertTestsInC() {
  {  // TEST(Crash, WithAndWithoutMessageArguments)
    MAYBE_SKIP_TEST;
    PW_CRASH(FAIL_IF_HIDDEN);
    PW_CRASH(FAIL_IF_HIDDEN_ARGS, z);
  }

  {  // TEST(Check, NoMessage)
    MAYBE_SKIP_TEST;
    PW_CHECK(1);
    PW_CHECK(0);
  }

  {  // TEST(Check, WithMessageAndArgs)
    MAYBE_SKIP_TEST;
    PW_CHECK(1, FAIL_IF_DISPLAYED);
    PW_CHECK(1, FAIL_IF_DISPLAYED_ARGS, z);

    PW_CHECK(0, FAIL_IF_HIDDEN);
    PW_CHECK(0, FAIL_IF_HIDDEN_ARGS, z);
  }

  {  // TEST(Check, IntComparison)
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

  {  // TEST(Check, UintComparison)
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

  {  // TEST(Check, FloatComparison)
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

  {  // TEST(Check, ComparisonArgumentsWithCommas)
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
}
