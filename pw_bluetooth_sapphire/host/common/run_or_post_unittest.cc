// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "run_or_post.h"

#include "lib/gtest/test_loop_fixture.h"

namespace bt {
namespace common {
namespace {

using RunOrPostTest = ::gtest::TestLoopFixture;

TEST_F(RunOrPostTest, WithoutDispatcher) {
  bool run = false;
  RunOrPost([&run] { run = true; }, nullptr);
  EXPECT_TRUE(run);
}

TEST_F(RunOrPostTest, WithDispatcher) {
  bool run = false;
  RunOrPost([&run] { run = true; }, dispatcher());
  EXPECT_FALSE(run);

  RunLoopUntilIdle();
  EXPECT_TRUE(run);
}

}  // namespace
}  // namespace common
}  // namespace bt
