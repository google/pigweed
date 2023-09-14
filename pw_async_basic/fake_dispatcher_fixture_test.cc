// Copyright 2023 The Pigweed Authors
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
#include "pw_async/fake_dispatcher_fixture.h"

#include <chrono>

#include "gtest/gtest.h"

namespace pw::async {
namespace {

using FakeDispatcherFixture = test::FakeDispatcherFixture;

TEST_F(FakeDispatcherFixture, PostTasksAndStop) {
  int count = 0;
  auto inc_count = [&count](Context& /*c*/, Status /*status*/) { ++count; };

  Task task(inc_count);
  dispatcher().Post(task);

  ASSERT_EQ(count, 0);
  EXPECT_TRUE(RunUntilIdle());
  ASSERT_EQ(count, 1);
  EXPECT_FALSE(RunUntilIdle());

  dispatcher().Post(task);
  EXPECT_TRUE(RunUntil(dispatcher().now()));
  ASSERT_EQ(count, 2);
  EXPECT_FALSE(RunUntilIdle());

  dispatcher().Post(task);
  EXPECT_TRUE(RunFor(std::chrono::seconds(1)));
  ASSERT_EQ(count, 3);
  EXPECT_FALSE(RunUntilIdle());

  dispatcher().PostAfter(task, std::chrono::minutes(1));
  dispatcher().RequestStop();
  ASSERT_EQ(count, 3);
  EXPECT_TRUE(RunUntilIdle());
  ASSERT_EQ(count, 4);
}

}  // namespace
}  // namespace pw::async
