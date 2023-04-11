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
#include "pw_async/heap_dispatcher.h"

#include "gtest/gtest.h"
#include "pw_async/fake_dispatcher_fixture.h"

using namespace std::chrono_literals;

namespace pw::async {
namespace {

using HeapDispatcherTest = test::FakeDispatcherFixture;

class DestructionChecker {
 public:
  DestructionChecker(bool* flag) : flag_(flag) {}
  DestructionChecker(DestructionChecker&& other) {
    flag_ = other.flag_;
    other.flag_ = nullptr;
  }
  ~DestructionChecker() {
    if (flag_) {
      *flag_ = true;
    }
  }

 private:
  bool* flag_;
};

TEST_F(HeapDispatcherTest, RunUntilIdleRunsPostedTask) {
  HeapDispatcher heap_dispatcher(dispatcher());

  int count = 0;
  Status status = heap_dispatcher.Post(
      [&count](Context& /*ctx*/, Status /*status*/) { ++count; });
  EXPECT_TRUE(status.ok());
  ASSERT_EQ(count, 0);
  RunUntilIdle();
  ASSERT_EQ(count, 1);
}

TEST_F(HeapDispatcherTest, TaskFunctionIsDestroyedAfterBeingCalled) {
  HeapDispatcher heap_dispatcher(dispatcher());

  // Test that the lambda is destroyed after being called.
  bool flag = false;
  Status status =
      heap_dispatcher.Post([checker = DestructionChecker(&flag)](
                               Context& /*ctx*/, Status /*status*/) {});
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(flag);
  RunUntilIdle();
  EXPECT_TRUE(flag);
}

}  // namespace
}  // namespace pw::async
