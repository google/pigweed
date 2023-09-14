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
#pragma once

#include "gtest/gtest.h"
#include "pw_async/fake_dispatcher.h"

namespace pw::async::test {

/// Test fixture that is a simple wrapper around a FakeDispatcher.
///
/// Example:
/// @code{.cpp}
///  using ExampleTest = pw::async::test::FakeDispatcherFixture;
///
///  TEST_F(ExampleTest, Example) {
///    MyClass obj(dispatcher());
///
///    obj.ScheduleSomeTasks();
///    EXPECT_TRUE(RunUntilIdle());
///    EXPECT_TRUE(some condition);
///
///    obj.ScheduleTaskToRunIn30Seconds();
///    EXPECT_TRUE(RunFor(30s));
///    EXPECT_TRUE(task ran);
///  }
/// @endcode
class FakeDispatcherFixture : public ::testing::Test {
 public:
  /// Returns the FakeDispatcher that should be used for dependency injection.
  FakeDispatcher& dispatcher() { return dispatcher_; }

  /// Returns the current fake time.
  chrono::SystemClock::time_point now() { return dispatcher_.now(); }

  /// Dispatches all tasks with due times up until `now()`.
  /// Returns true iff any tasks were invoked during the run.
  bool RunUntilIdle() { return dispatcher_.RunUntilIdle(); }

  /// Dispatches all tasks with due times up to `end_time`, progressively
  /// advancing the fake clock.
  /// Returns true iff any tasks were invoked during the run.
  bool RunUntil(chrono::SystemClock::time_point end_time) {
    return dispatcher_.RunUntil(end_time);
  }

  /// Dispatches all tasks with due times up to `now() + duration`,
  /// progressively advancing the fake clock.
  /// Returns true iff any tasks were invoked during the run.
  bool RunFor(chrono::SystemClock::duration duration) {
    return dispatcher_.RunFor(duration);
  }

 private:
  FakeDispatcher dispatcher_;
};

}  // namespace pw::async::test
