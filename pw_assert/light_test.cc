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

#include "pw_assert/light.h"

#include "gtest/gtest.h"
#include "pw_assert/assert.h"

// PW_ASSERT() should always be enabled, and always evaluate the expression.
TEST(Light, AssertTrue) {
  int evaluated = 1;
  PW_ASSERT(++evaluated);
  EXPECT_EQ(evaluated, 2);
}

// PW_DASSERT() might be disabled sometimes.
TEST(Light, DebugAssertTrue) {
  int evaluated = 1;
  PW_DASSERT(++evaluated);
  if (PW_ASSERT_ENABLE_DEBUG == 1) {
    EXPECT_EQ(evaluated, 2);
  } else {
    EXPECT_EQ(evaluated, 1);
  }
}

// Unfortunately, we don't have the infrastructure to test failure handling
// automatically, since the harness crashes in the process of running this
// test. The unsatisfying alternative is to test the functionality manually,
// then disable the test.

TEST(Light, AssertFalse) {
  if (0) {
    PW_ASSERT(false);
  }
}

TEST(Light, DebugAssertFalse) {
  if (0) {
    PW_DASSERT(false);
  }
}
