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

// This test directly verifies the facade logic, by leveraging a fake backend
// that captures arguments and returns rather than aborting execution.

#include "pw_assert_test/fake_backend.h"

// This directly includes the assert facade implementation header rather than
// going through the backend header indirection mechanism, to prevent the real
// assert backend from triggering.
//
// clang-format off
#define PW_ASSERT_USE_SHORT_NAMES 1
#include "pw_assert/internal/assert_impl.h"
// clang-format on

#include "gtest/gtest.h"

namespace {

#define EXPECT_MESSAGE(expected_assert_message)                        \
  do {                                                                 \
    EXPECT_STREQ(pw_captured_assert.message, expected_assert_message); \
  } while (0)

class AssertFail : public ::testing::Test {
 protected:
  void SetUp() override { pw_captured_assert.triggered = 0; }
  void TearDown() override { EXPECT_EQ(pw_captured_assert.triggered, 1); }
};

class AssertPass : public ::testing::Test {
 protected:
  void SetUp() override { pw_captured_assert.triggered = 0; }
  void TearDown() override { EXPECT_EQ(pw_captured_assert.triggered, 0); }
};

// PW_CRASH(...)
TEST_F(AssertFail, CrashMessageNoArguments) {
  PW_CRASH("Goodbye");
  EXPECT_MESSAGE("Goodbye");
}
TEST_F(AssertFail, CrashMessageWithArguments) {
  PW_CRASH("Goodbye cruel %s", "world");
  EXPECT_MESSAGE("Goodbye cruel world");
}

// PW_CHECK(...) - No message
TEST_F(AssertPass, CheckNoMessage) { PW_CHECK(true); }
TEST_F(AssertFail, CheckNoMessage) {
  PW_CHECK(false);
  EXPECT_MESSAGE("Check failed: false. ");
}
TEST_F(AssertPass, CheckNoMessageComplexExpression) { PW_CHECK(2 == 2); }
TEST_F(AssertFail, CheckNoMessageComplexExpression) {
  PW_CHECK(1 == 2);
  EXPECT_MESSAGE("Check failed: 1 == 2. ");
}

// PW_CHECK(..., msg) - With message; with and without arguments.
TEST_F(AssertPass, CheckMessageNoArguments) { PW_CHECK(true, "Hello"); }
TEST_F(AssertFail, CheckMessageNoArguments) {
  PW_CHECK(false, "Hello");
  EXPECT_MESSAGE("Check failed: false. Hello");
}
TEST_F(AssertPass, CheckMessageWithArguments) { PW_CHECK(true, "Hello %d", 5); }
TEST_F(AssertFail, CheckMessageWithArguments) {
  PW_CHECK(false, "Hello %d", 5);
  EXPECT_MESSAGE("Check failed: false. Hello 5");
}

// PW_CHECK_INT_*(...)
// Binary checks with ints, comparisons: <, <=, =, >, >=.

// Test message formatting separate from the triggering.
// Only test formatting for the type once.
TEST_F(AssertFail, IntLessThanNoMessageNoArguments) {
  PW_CHECK_INT_LT(5, -2);
  EXPECT_MESSAGE("Check failed: 5 (=5) < -2 (=-2). ");
}
TEST_F(AssertFail, IntLessThanMessageNoArguments) {
  PW_CHECK_INT_LT(5, -2, "msg");
  EXPECT_MESSAGE("Check failed: 5 (=5) < -2 (=-2). msg");
}
TEST_F(AssertFail, IntLessThanMessageArguments) {
  PW_CHECK_INT_LT(5, -2, "msg: %d", 6);
  EXPECT_MESSAGE("Check failed: 5 (=5) < -2 (=-2). msg: 6");
}

// Test comparison boundaries.

// INT <
TEST_F(AssertPass, IntLt1) { PW_CHECK_INT_LT(-1, 2); }
TEST_F(AssertPass, IntLt2) { PW_CHECK_INT_LT(1, 2); }
TEST_F(AssertFail, IntLt3) { PW_CHECK_INT_LT(-1, -2); }
TEST_F(AssertFail, IntLt4) { PW_CHECK_INT_LT(1, 1); }

// INT <=
TEST_F(AssertPass, IntLe1) { PW_CHECK_INT_LE(-1, 2); }
TEST_F(AssertPass, IntLe2) { PW_CHECK_INT_LE(1, 2); }
TEST_F(AssertFail, IntLe3) { PW_CHECK_INT_LE(-1, -2); }
TEST_F(AssertPass, IntLe4) { PW_CHECK_INT_LE(1, 1); }

// INT ==
TEST_F(AssertFail, IntEq1) { PW_CHECK_INT_EQ(-1, 2); }
TEST_F(AssertFail, IntEq2) { PW_CHECK_INT_EQ(1, 2); }
TEST_F(AssertFail, IntEq3) { PW_CHECK_INT_EQ(-1, -2); }
TEST_F(AssertPass, IntEq4) { PW_CHECK_INT_EQ(1, 1); }

// INT >
TEST_F(AssertFail, IntGt1) { PW_CHECK_INT_GT(-1, 2); }
TEST_F(AssertFail, IntGt2) { PW_CHECK_INT_GT(1, 2); }
TEST_F(AssertPass, IntGt3) { PW_CHECK_INT_GT(-1, -2); }
TEST_F(AssertFail, IntGt4) { PW_CHECK_INT_GT(1, 1); }

// INT >=
TEST_F(AssertFail, IntGe1) { PW_CHECK_INT_GE(-1, 2); }
TEST_F(AssertFail, IntGe2) { PW_CHECK_INT_GE(1, 2); }
TEST_F(AssertPass, IntGe3) { PW_CHECK_INT_GE(-1, -2); }
TEST_F(AssertPass, IntGe4) { PW_CHECK_INT_GE(1, 1); }

// PW_CHECK_UINT_*(...)
// Binary checks with uints, comparisons: <, <=, =, >, >=.

// Test message formatting separate from the triggering.
// Only test formatting for the type once.
TEST_F(AssertFail, UintLessThanNoMessageNoArguments) {
  PW_CHECK_UINT_LT(5, 2);
  EXPECT_MESSAGE("Check failed: 5 (=5) < 2 (=2). ");
}
TEST_F(AssertFail, UintLessThanMessageNoArguments) {
  PW_CHECK_UINT_LT(5, 2, "msg");
  EXPECT_MESSAGE("Check failed: 5 (=5) < 2 (=2). msg");
}
TEST_F(AssertFail, UintLessThanMessageArguments) {
  PW_CHECK_UINT_LT(5, 2, "msg: %d", 6);
  EXPECT_MESSAGE("Check failed: 5 (=5) < 2 (=2). msg: 6");
}

// Test comparison boundaries.

// UINT <
TEST_F(AssertPass, UintLt1) { PW_CHECK_UINT_LT(1, 2); }
TEST_F(AssertFail, UintLt2) { PW_CHECK_UINT_LT(2, 2); }
TEST_F(AssertFail, UintLt3) { PW_CHECK_UINT_LT(2, 1); }

// UINT <=
TEST_F(AssertPass, UintLe1) { PW_CHECK_UINT_LE(1, 2); }
TEST_F(AssertPass, UintLe2) { PW_CHECK_UINT_LE(2, 2); }
TEST_F(AssertFail, UintLe3) { PW_CHECK_UINT_LE(2, 1); }

// UINT ==
TEST_F(AssertFail, UintEq1) { PW_CHECK_UINT_EQ(1, 2); }
TEST_F(AssertPass, UintEq2) { PW_CHECK_UINT_EQ(2, 2); }
TEST_F(AssertFail, UintEq3) { PW_CHECK_UINT_EQ(2, 1); }

// UINT >
TEST_F(AssertFail, UintGt1) { PW_CHECK_UINT_GT(1, 2); }
TEST_F(AssertFail, UintGt2) { PW_CHECK_UINT_GT(2, 2); }
TEST_F(AssertPass, UintGt3) { PW_CHECK_UINT_GT(2, 1); }

// UINT >=
TEST_F(AssertFail, UintGe1) { PW_CHECK_UINT_GE(1, 2); }
TEST_F(AssertPass, UintGe2) { PW_CHECK_UINT_GE(2, 2); }
TEST_F(AssertPass, UintGe3) { PW_CHECK_UINT_GE(2, 1); }

// PW_CHECK_FLOAT_*(...)
// Binary checks with floats, comparisons: <, <=, =, >, >=.

// Test message formatting separate from the triggering.
// Only test formatting for the type once.
TEST_F(AssertFail, FloatLessThanNoMessageNoArguments) {
  PW_CHECK_FLOAT_LT(5.2, 2.3);
  EXPECT_MESSAGE("Check failed: 5.2 (=5.200000) < 2.3 (=2.300000). ");
}
TEST_F(AssertFail, FloatLessThanMessageNoArguments) {
  PW_CHECK_FLOAT_LT(5.2, 2.3, "msg");
  EXPECT_MESSAGE("Check failed: 5.2 (=5.200000) < 2.3 (=2.300000). msg");
}
TEST_F(AssertFail, FloatLessThanMessageArguments) {
  PW_CHECK_FLOAT_LT(5.2, 2.3, "msg: %d", 6);
  EXPECT_MESSAGE("Check failed: 5.2 (=5.200000) < 2.3 (=2.300000). msg: 6");
}

// Test comparison boundaries.
// Note: The below example numbers all round to integer 1, to detect accidental
// integer conversions in the asserts.

// FLOAT <
TEST_F(AssertPass, FloatLt1) { PW_CHECK_FLOAT_LT(1.1, 1.2); }
TEST_F(AssertFail, FloatLt2) { PW_CHECK_FLOAT_LT(1.2, 1.2); }
TEST_F(AssertFail, FloatLt3) { PW_CHECK_FLOAT_LT(1.2, 1.1); }

// FLOAT <=
TEST_F(AssertPass, FloatLe1) { PW_CHECK_FLOAT_LE(1.1, 1.2); }
TEST_F(AssertPass, FloatLe2) { PW_CHECK_FLOAT_LE(1.2, 1.2); }
TEST_F(AssertFail, FloatLe3) { PW_CHECK_FLOAT_LE(1.2, 1.1); }

// FLOAT ==
TEST_F(AssertFail, FloatEq1) { PW_CHECK_FLOAT_EQ(1.1, 1.2); }
TEST_F(AssertPass, FloatEq2) { PW_CHECK_FLOAT_EQ(1.2, 1.2); }
TEST_F(AssertFail, FloatEq3) { PW_CHECK_FLOAT_EQ(1.2, 1.1); }

// FLOAT >
TEST_F(AssertFail, FloatGt1) { PW_CHECK_FLOAT_GT(1.1, 1.2); }
TEST_F(AssertFail, FloatGt2) { PW_CHECK_FLOAT_GT(1.2, 1.2); }
TEST_F(AssertPass, FloatGt3) { PW_CHECK_FLOAT_GT(1.2, 1.1); }

// FLOAT >=
TEST_F(AssertFail, FloatGe1) { PW_CHECK_FLOAT_GE(1.1, 1.2); }
TEST_F(AssertPass, FloatGe2) { PW_CHECK_FLOAT_GE(1.2, 1.2); }
TEST_F(AssertPass, FloatGe3) { PW_CHECK_FLOAT_GE(1.2, 1.1); }

// Nested comma handling.
static int Add3(int a, int b, int c) { return a + b + c; }

TEST_F(AssertFail, CommaHandlingLeftSide) {
  PW_CHECK_INT_EQ(Add3(1, 2, 3), 4);
  EXPECT_MESSAGE("Check failed: Add3(1, 2, 3) (=6) == 4 (=4). ");
}
TEST_F(AssertFail, CommaHandlingRightSide) {
  PW_CHECK_INT_EQ(4, Add3(1, 2, 3));
  EXPECT_MESSAGE("Check failed: 4 (=4) == Add3(1, 2, 3) (=6). ");
}

// Verify that the CHECK_*(x,y) macros only evaluate their arguments once.
static int global_state_for_multi_evaluate_test;
static int IncrementsGlobal() {
  global_state_for_multi_evaluate_test++;
  return 0;
}

TEST(AssertPass, CheckSingleSideEffectingCall) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK(IncrementsGlobal() == 0);
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);
}
TEST(AssertFail, CheckSingleSideEffectingCall) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK(IncrementsGlobal() == 1);
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);
}
TEST(AssertPass, BinaryOpSingleSideEffectingCall) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(0, IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);
}
TEST(AssertPass, BinaryOpTwoSideEffectingCalls) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(IncrementsGlobal(), IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 2);
}
TEST(AssertFail, BinaryOpSingleSideEffectingCall) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(12314, IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 1);
}
TEST(AssertFail, BinaryOpTwoSideEffectingCalls) {
  global_state_for_multi_evaluate_test = 0;
  PW_CHECK_INT_EQ(IncrementsGlobal() + 10, IncrementsGlobal());
  EXPECT_EQ(global_state_for_multi_evaluate_test, 2);
}

// Note: This requires enabling PW_ASSERT_USE_SHORT_NAMES 1 above.
TEST(Check, ShortNamesWork) {
  // Crash
  CRASH("msg");
  CRASH("msg: %d", 5);

  // Check
  CHECK(true);
  CHECK(true, "msg");
  CHECK(true, "msg: %d", 5);
  CHECK(false);
  CHECK(false, "msg");
  CHECK(false, "msg: %d", 5);

  // Check with binary comparison
  CHECK_INT_LE(1, 2);
  CHECK_INT_LE(1, 2, "msg");
  CHECK_INT_LE(1, 2, "msg: %d", 5);
}

// TODO: Figure out how to run some of these tests is C.

}  // namespace
