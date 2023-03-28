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

#define ASSERT_OK(status) ASSERT_EQ(OkStatus(), status)
#define ASSERT_CANCELLED(status) ASSERT_EQ(Status::Cancelled(), status)

using namespace std::chrono_literals;

namespace pw::async::test {

TEST(FakeDispatcher, PostTasks) {
  FakeDispatcher dispatcher;

  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++count;
  };

  Task task(inc_count);
  dispatcher.Post(task);

  Task task2(inc_count);
  dispatcher.Post(task2);

  Task task3(inc_count);
  dispatcher.Post(task3);

  // Should not run; RunUntilIdle() does not advance time.
  Task task4([&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  });
  dispatcher.PostAfter(task4, 1ms);

  dispatcher.RunUntilIdle();
  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 4);
}

// Lambdas can only capture one ptr worth of memory without allocating, so we
// group the data we want to share between tasks and their containing tests
// inside one struct.
struct TaskPair {
  Task task_a;
  Task task_b;
  int count = 0;
};

TEST(FakeDispatcher, DelayedTasks) {
  FakeDispatcher dispatcher;
  TaskPair tp;

  Task task0([&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    tp.count = tp.count * 10 + 4;
  });
  dispatcher.PostAfter(task0, 200ms);

  Task task1([&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    tp.count = tp.count * 10 + 1;
    c.dispatcher->PostAfter(tp.task_a, 50ms);
    c.dispatcher->PostAfter(tp.task_b, 25ms);
  });
  dispatcher.PostAfter(task1, 100ms);

  tp.task_a.set_function([&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    tp.count = tp.count * 10 + 3;
  });
  tp.task_b.set_function([&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    tp.count = tp.count * 10 + 2;
  });

  dispatcher.RunFor(200ms);
  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  ASSERT_EQ(tp.count, 1234);
}

TEST(FakeDispatcher, CancelTasks) {
  FakeDispatcher dispatcher;

  auto shouldnt_run = []([[maybe_unused]] Context& c,
                         [[maybe_unused]] Status status) { FAIL(); };

  TaskPair tp;
  // This task gets canceled in cancel_task.
  tp.task_a.set_function(shouldnt_run);
  dispatcher.PostAfter(tp.task_a, 40ms);

  // This task gets canceled immediately.
  Task task1(shouldnt_run);
  dispatcher.PostAfter(task1, 10ms);
  ASSERT_TRUE(dispatcher.Cancel(task1));

  // This task cancels the first task.
  Task cancel_task([&tp](Context& c, Status status) {
    ASSERT_OK(status);
    ASSERT_TRUE(c.dispatcher->Cancel(tp.task_a));
    ++tp.count;
  });
  dispatcher.PostAfter(cancel_task, 20ms);

  dispatcher.RunFor(50ms);
  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  ASSERT_EQ(tp.count, 1);
}

// Test RequestStop() from inside task.
TEST(FakeDispatcher, RequestStopInsideTask) {
  FakeDispatcher dispatcher;

  int count = 0;
  auto cancelled_cb = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };

  // These tasks are never executed and cleaned up in RequestStop().
  Task task0(cancelled_cb), task1(cancelled_cb);
  dispatcher.PostAfter(task0, 20ms);
  dispatcher.PostAfter(task1, 21ms);

  Task stop_task([&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++count;
    static_cast<FakeDispatcher*>(c.dispatcher)->RequestStop();
    static_cast<FakeDispatcher*>(c.dispatcher)->RunUntilIdle();
  });
  dispatcher.Post(stop_task);

  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 3);
}

TEST(FakeDispatcher, PeriodicTasks) {
  FakeDispatcher dispatcher;

  int count = 0;
  Task periodic_task([&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++count;
  });
  dispatcher.PostPeriodicAt(periodic_task, 20ms, dispatcher.now() + 50ms);

  // Cancel periodic task after it has run thrice, at +50ms, +70ms, and +90ms.
  Task cancel_task([&periodic_task](Context& c, Status status) {
    ASSERT_OK(status);
    c.dispatcher->Cancel(periodic_task);
  });
  dispatcher.PostAfter(cancel_task, 100ms);

  dispatcher.RunFor(300ms);
  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 3);
}

TEST(FakeDispatcher, PostPeriodicAfter) {
  FakeDispatcher dispatcher;

  int count = 0;
  Task periodic_task([&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++count;
  });
  dispatcher.PostPeriodicAfter(periodic_task, /*interval=*/5ms, /*delay=*/20ms);

  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 0);
  dispatcher.RunFor(20ms);
  ASSERT_EQ(count, 1);
  dispatcher.RunFor(10ms);
  ASSERT_EQ(count, 3);
  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 3);
}

TEST(FakeDispatcher, TasksCancelledByDispatcherDestructor) {
  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };
  Task task0(inc_count), task1(inc_count), task2(inc_count);

  {
    FakeDispatcher dispatcher;
    dispatcher.PostAfter(task0, 10s);
    dispatcher.PostAfter(task1, 10s);
    dispatcher.PostAfter(task2, 10s);
  }

  ASSERT_EQ(count, 3);
}

TEST(DispatcherBasic, TasksCancelledByRunFor) {
  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };
  Task task0(inc_count), task1(inc_count), task2(inc_count);

  FakeDispatcher dispatcher;
  dispatcher.PostAfter(task0, 10s);
  dispatcher.PostAfter(task1, 10s);
  dispatcher.PostAfter(task2, 10s);

  dispatcher.RequestStop();
  dispatcher.RunFor(5s);
  ASSERT_EQ(count, 3);
}

}  // namespace pw::async::test
