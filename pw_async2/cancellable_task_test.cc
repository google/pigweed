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

#include "pw_async2/cancellable_task.h"

#include "pw_async2/dispatcher.h"
#include "pw_function/function.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::async2::CancellableTask;
using pw::async2::Context;
using pw::async2::Dispatcher;
using pw::async2::Pending;
using pw::async2::Poll;
using pw::async2::Ready;
using pw::async2::Task;
using pw::async2::Waker;

class MockTask : public Task {
 public:
  bool should_complete = false;
  bool destroyed = false;
  Waker waker;

  MockTask(pw::Closure callback)
      : Task(PW_ASYNC_TASK_NAME("MockTask")), callback_(std::move(callback)) {}

 protected:
  Poll<> DoPend(Context& cx) override {
    PW_ASYNC_STORE_WAKER(cx, waker, "MockTask is waiting for waker");
    if (should_complete) {
      callback_();
      return Ready();
    }
    return Pending();
  }

  void DoDestroy() override { destroyed = true; }

 private:
  pw::Closure callback_;
};

TEST(CancellableTask, CancelsPendingTask) {
  bool task_completed = false;

  CancellableTask<MockTask> task(
      [&task_completed]() { task_completed = true; });
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_FALSE(task_completed);
  EXPECT_FALSE(task.destroyed);

  task.Cancel();
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
  EXPECT_FALSE(task.IsRegistered());
  EXPECT_FALSE(task_completed);
  EXPECT_TRUE(task.destroyed);
}

TEST(CancellableTask, DoesNothingWithCompletedTask) {
  bool task_completed = false;

  CancellableTask<MockTask> task(
      [&task_completed]() { task_completed = true; });
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsPending());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_FALSE(task_completed);
  EXPECT_FALSE(task.destroyed);

  task.should_complete = true;
  std::move(task.waker).Wake();
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
  EXPECT_TRUE(task_completed);
  EXPECT_TRUE(task.destroyed);

  task.Cancel();
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
}

TEST(CancellableTask, CancelsTaskBeforePosting) {
  bool task_completed = false;

  CancellableTask<MockTask> task(
      [&task_completed]() { task_completed = true; });
  task.should_complete = true;
  task.Cancel();

  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_FALSE(task_completed);
  EXPECT_TRUE(task.destroyed);
}

TEST(CancellableTask, CancelsTaskBeforeRunning) {
  bool task_completed = false;

  CancellableTask<MockTask> task(
      [&task_completed]() { task_completed = true; });
  Dispatcher dispatcher;
  dispatcher.Post(task);

  task.should_complete = true;
  task.Cancel();

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_FALSE(task_completed);
  EXPECT_TRUE(task.destroyed);
}

}  // namespace
