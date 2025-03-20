// Copyright 2025 The Pigweed Authors
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

#include "pw_toolchain/infinite_loop.h"

#include "pw_unit_test/framework.h"

extern "C" int pw_TestInfiniteLoopC(int loop_infinitely_if_0);

namespace {

int TestInfiniteLoop(int loop_infinitely_if_0) {
  if (loop_infinitely_if_0 != 0) {
    return loop_infinitely_if_0;
  }
  pw::InfiniteLoop();  // No return statement needed because of infinite loop.
}

TEST(InfiniteLoop, Compiles) {
  EXPECT_EQ(TestInfiniteLoop(123), 123);

  if (false) {
    EXPECT_EQ(TestInfiniteLoop(0), 0);  // Loops forever!
  }
}

int TestInfiniteLoopCAlias(int loop_infinitely_if_0) {
  if (loop_infinitely_if_0 != 0) {
    return loop_infinitely_if_0;
  }
  pw_InfiniteLoop();  // No return statement needed because of infinite loop.
}

TEST(InfiniteLoop, CAliasCompiles) {
  EXPECT_EQ(TestInfiniteLoopCAlias(123), 123);

  if (false) {
    EXPECT_EQ(TestInfiniteLoopCAlias(0), 0);  // Loops forever!
  }
}

TEST(InfiniteLoop, CompilesInC) {
  EXPECT_EQ(pw_TestInfiniteLoopC(123), 123);

  if (false) {
    EXPECT_EQ(pw_TestInfiniteLoopC(0), 0);  // Loops forever!
  }
}

}  // namespace
