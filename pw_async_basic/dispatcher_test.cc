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
#include "pw_async_basic/dispatcher.h"

#include "gtest/gtest.h"
#include "pw_log/log.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

#define ASSERT_OK(status) ASSERT_EQ(OkStatus(), status)
#define ASSERT_CANCELLED(status) ASSERT_EQ(Status::Cancelled(), status)

using namespace std::chrono_literals;

namespace pw::async {

// Lambdas can only capture one ptr worth of memory without allocating, so we
// group the data we want to share between tasks and their containing tests
// inside one struct.
struct TestPrimitives {
  int count = 0;
  sync::ThreadNotification notification;
};

TEST(DispatcherBasic, PostTasks) {
  BasicDispatcher dispatcher;
  thread::Thread work_thread(thread::stl::Options(), dispatcher);

  TestPrimitives tp;
  auto inc_count = [&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++tp.count;
  };

  Task task(inc_count);
  dispatcher.PostTask(task);

  Task task2(inc_count);
  dispatcher.PostTask(task2);

  Task task3([&tp]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++tp.count;
    tp.notification.release();
  });
  dispatcher.PostTask(task3);

  tp.notification.acquire();
  dispatcher.RequestStop();
  work_thread.join();
  ASSERT_EQ(tp.count, 3);
}

struct TaskPair {
  Task task_a;
  Task task_b;
  int count = 0;
  sync::ThreadNotification notification;
};

TEST(DispatcherBasic, ChainedTasks) {
  BasicDispatcher dispatcher;
  thread::Thread work_thread(thread::stl::Options(), dispatcher);

  sync::ThreadNotification notification;
  Task task1([&notification]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    notification.release();
  });

  Task task2([&task1](Context& c, Status status) {
    ASSERT_OK(status);
    c.dispatcher->PostTask(task1);
  });

  Task task3([&task2](Context& c, Status status) {
    ASSERT_OK(status);
    c.dispatcher->PostTask(task2);
  });
  dispatcher.PostTask(task3);

  notification.acquire();
  dispatcher.RequestStop();
  work_thread.join();
}

// Test RequestStop() from inside task.
TEST(DispatcherBasic, RequestStopInsideTask) {
  BasicDispatcher dispatcher;
  thread::Thread work_thread(thread::stl::Options(), dispatcher);

  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };

  // These tasks are never executed and cleaned up in RequestStop().
  Task task0(inc_count), task1(inc_count);
  dispatcher.PostDelayedTask(task0, 20ms);
  dispatcher.PostDelayedTask(task1, 21ms);

  Task stop_task([&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    ++count;
    c.dispatcher->RequestStop();
  });
  dispatcher.PostTask(stop_task);

  work_thread.join();
  ASSERT_EQ(count, 3);
}

TEST(DispatcherBasic, TasksCancelledByRequestStopInDifferentThread) {
  BasicDispatcher dispatcher;
  thread::Thread work_thread(thread::stl::Options(), dispatcher);

  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };

  Task task0(inc_count), task1(inc_count), task2(inc_count);
  dispatcher.PostDelayedTask(task0, 10s);
  dispatcher.PostDelayedTask(task1, 10s);
  dispatcher.PostDelayedTask(task2, 10s);

  dispatcher.RequestStop();
  work_thread.join();
  ASSERT_EQ(count, 3);
}

TEST(DispatcherBasic, TasksCancelledByDispatcherDestructor) {
  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };
  Task task0(inc_count), task1(inc_count), task2(inc_count);

  {
    BasicDispatcher dispatcher;
    dispatcher.PostDelayedTask(task0, 10s);
    dispatcher.PostDelayedTask(task1, 10s);
    dispatcher.PostDelayedTask(task2, 10s);
  }

  ASSERT_EQ(count, 3);
}

TEST(DispatcherBasic, TasksCancelledByRunUntilIdle) {
  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };
  Task task0(inc_count), task1(inc_count), task2(inc_count);

  BasicDispatcher dispatcher;
  dispatcher.PostDelayedTask(task0, 10s);
  dispatcher.PostDelayedTask(task1, 10s);
  dispatcher.PostDelayedTask(task2, 10s);

  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  ASSERT_EQ(count, 3);
}

TEST(DispatcherBasic, TasksCancelledByRunFor) {
  int count = 0;
  auto inc_count = [&count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_CANCELLED(status);
    ++count;
  };
  Task task0(inc_count), task1(inc_count), task2(inc_count);

  BasicDispatcher dispatcher;
  dispatcher.PostDelayedTask(task0, 10s);
  dispatcher.PostDelayedTask(task1, 10s);
  dispatcher.PostDelayedTask(task2, 10s);

  dispatcher.RequestStop();
  dispatcher.RunFor(5s);
  ASSERT_EQ(count, 3);
}

}  // namespace pw::async
