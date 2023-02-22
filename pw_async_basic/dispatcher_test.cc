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
  auto inc_count = [&tp]([[maybe_unused]] Context& c) { ++tp.count; };

  Task task(inc_count);
  dispatcher.PostTask(task);

  Task task2(inc_count);
  dispatcher.PostTask(task2);

  Task task3([&tp]([[maybe_unused]] Context& c) {
    ++tp.count;
    tp.notification.release();
  });
  dispatcher.PostTask(task3);

  tp.notification.acquire();
  dispatcher.RequestStop();
  work_thread.join();

  ASSERT_TRUE(tp.count == 3);
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

  TaskPair tp;

  Task task0([&tp](Context& c) {
    ++tp.count;

    c.dispatcher->PostTask(tp.task_a);
  });

  tp.task_a.set_function([&tp](Context& c) {
    ++tp.count;

    c.dispatcher->PostTask(tp.task_b);
  });

  tp.task_b.set_function([&tp]([[maybe_unused]] Context& c) {
    ++tp.count;
    tp.notification.release();
  });

  dispatcher.PostTask(task0);

  tp.notification.acquire();
  dispatcher.RequestStop();
  work_thread.join();

  ASSERT_TRUE(tp.count == 3);
}

// Test RequestStop() from inside task.
TEST(DispatcherBasic, RequestStopInsideTask) {
  BasicDispatcher dispatcher;
  thread::Thread work_thread(thread::stl::Options(), dispatcher);

  TestPrimitives tp;
  auto inc_count = [&tp]([[maybe_unused]] Context& c) { ++tp.count; };

  // These tasks are never executed and cleaned up in RequestStop().
  Task task0(inc_count), task1(inc_count);
  dispatcher.PostDelayedTask(task0, 20ms);
  dispatcher.PostDelayedTask(task1, 21ms);

  Task stop_task([&tp]([[maybe_unused]] Context& c) {
    ++tp.count;
    c.dispatcher->RequestStop();
    tp.notification.release();
  });
  dispatcher.PostTask(stop_task);

  tp.notification.acquire();
  work_thread.join();

  ASSERT_TRUE(tp.count == 1);
}

}  // namespace pw::async
