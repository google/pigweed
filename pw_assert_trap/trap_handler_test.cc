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

// #include "pw_assert_trap/assert_trap.h"
#include "pw_assert_trap/check_trap.h"
#include "pw_assert_trap/message.h"
#include "pw_unit_test/framework.h"

namespace {

void TestSetUp() { pw_assert_trap_clear_message(); }

// TODO: https://pwbug.dev/351889230 - add unittests and enable by default
// TEST(TrapHandler, AssertHandleFailure) {
//   TestSetUp();
//   PW_ASSERT_HANDLE_FAILURE(false);
//   auto actual_msg = pw_assert_trap_get_message();
//   ASSERT_STREQ("PW_ASSERT() or PW_DASSERT() failure", actual_msg.data());
// }

TEST(TrapHandler, Crash) {
  TestSetUp();
  PW_HANDLE_CRASH("crash message: %d", 7);
  auto actual_msg = pw_assert_trap_get_message();
  ASSERT_STREQ("crash message: 7", actual_msg.data());
}

TEST(TrapHandler, HandleAssertFailure) {
  TestSetUp();
  PW_HANDLE_ASSERT_FAILURE(false, "assert: %d", 1);
  auto actual_msg = pw_assert_trap_get_message();
  ASSERT_STREQ("assert: 1", actual_msg.data());
}

TEST(TrapHandler, HandleAssertBinaryCompareFailure) {
  TestSetUp();
  PW_HANDLE_ASSERT_BINARY_COMPARE_FAILURE(
      "expected", 1, "==", "actual", 2, "%d", "fail");
  auto actual_msg = pw_assert_trap_get_message();
  ASSERT_STREQ("Check failed: expected (=1) == actual (=2). fail",
               actual_msg.data());
}

}  // namespace
