// Copyright 2022 The Pigweed Authors
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
#include "pw_async/fake_dispatcher.h"

#include "gtest/gtest.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

using namespace std::chrono_literals;

namespace pw::async::test {

// Lambdas can only capture one ptr worth of memory without allocating, so we
// group the data we want to share between tasks and their containing tests
// inside one struct.
struct TestPrimitives {
  int count = 0;
  sync::ThreadNotification notification;
};

TEST(FakeDispatcher, PostTasks) {
  FakeDispatcher dispatcher;

  TestPrimitives tp;
  auto inc_count = [&tp]([[maybe_unused]] Context& c) { ++tp.count; };

  Task task(inc_count);
  dispatcher.PostTask(task);

  Task task2(inc_count);
  dispatcher.PostTask(task2);

  Task task3([&tp]([[maybe_unused]] Context& c) { ++tp.count; });
  dispatcher.PostTask(task3);

  dispatcher.RunUntilIdle();
  dispatcher.RequestStop();

  ASSERT_TRUE(tp.count == 3);
}

struct TaskPair {
  Task task_a;
  Task task_b;
  int count = 0;
  sync::ThreadNotification notification;
};

TEST(FakeDispatcher, DelayedTasks) {
  FakeDispatcher dispatcher;
  TaskPair tp;

  Task task0(
      [&tp]([[maybe_unused]] Context& c) { tp.count = tp.count * 10 + 4; });

  dispatcher.PostDelayedTask(task0, 200ms);

  Task task1([&tp]([[maybe_unused]] Context& c) {
    tp.count = tp.count * 10 + 1;
    c.dispatcher->PostDelayedTask(tp.task_a, 50ms);
    c.dispatcher->PostDelayedTask(tp.task_b, 25ms);
  });

  dispatcher.PostDelayedTask(task1, 100ms);

  tp.task_a.set_function(
      [&tp]([[maybe_unused]] Context& c) { tp.count = tp.count * 10 + 3; });

  tp.task_b.set_function(
      [&tp]([[maybe_unused]] Context& c) { tp.count = tp.count * 10 + 2; });

  dispatcher.RunUntilIdle();
  dispatcher.RequestStop();

  ASSERT_TRUE(tp.count == 1234);
}

TEST(FakeDispatcher, CancelTasks) {
  FakeDispatcher dispatcher;

  TestPrimitives tp;
  auto inc_count = [&tp]([[maybe_unused]] Context& c) { ++tp.count; };

  // This task gets canceled in the last task.
  Task task0(inc_count);
  dispatcher.PostDelayedTask(task0, 40ms);

  // This task gets canceled immediately.
  Task task1(inc_count);
  dispatcher.PostDelayedTask(task1, 10ms);
  ASSERT_TRUE(dispatcher.Cancel(task1));

  // This task cancels the first task.
  Task cancel_task(
      [&task0](Context& c) { ASSERT_TRUE(c.dispatcher->Cancel(task0)); });
  dispatcher.PostDelayedTask(cancel_task, 20ms);

  dispatcher.RunUntilIdle();
  dispatcher.RequestStop();

  ASSERT_TRUE(tp.count == 0);
}

// Test RequestStop() from inside task.
TEST(FakeDispatcher, RequestStopInsideTask) {
  FakeDispatcher dispatcher;

  TestPrimitives tp;
  auto inc_count = [&tp]([[maybe_unused]] Context& c) { ++tp.count; };

  // These tasks are never executed and cleaned up in RequestStop().
  Task task0(inc_count), task1(inc_count);
  dispatcher.PostDelayedTask(task0, 20ms);
  dispatcher.PostDelayedTask(task1, 21ms);

  Task stop_task([&tp]([[maybe_unused]] Context& c) {
    ++tp.count;
    c.dispatcher->RequestStop();
  });
  dispatcher.PostTask(stop_task);

  dispatcher.RunUntilIdle();

  ASSERT_TRUE(tp.count == 1);
}

TEST(FakeDispatcher, PeriodicTasks) {
  FakeDispatcher dispatcher;

  TestPrimitives tp;

  Task periodic_task([&tp]([[maybe_unused]] Context& c) { ++tp.count; });
  dispatcher.SchedulePeriodicTask(periodic_task, 20ms, dispatcher.now() + 50ms);

  // Cancel periodic task after it has run thrice, at +50ms, +70ms, and +90ms.
  Task cancel_task(
      [&periodic_task](Context& c) { c.dispatcher->Cancel(periodic_task); });
  dispatcher.PostDelayedTask(cancel_task, 100ms);

  dispatcher.RunUntilIdle();
  dispatcher.RequestStop();

  ASSERT_TRUE(tp.count == 3);
}

}  // namespace pw::async::test
