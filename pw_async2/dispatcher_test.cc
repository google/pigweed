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

#include "pw_async2/dispatcher.h"

#include "pw_containers/vector.h"
#include "pw_unit_test/framework.h"

namespace pw::async2 {
namespace {

class MockTask : public Task {
 public:
  bool should_complete = false;
  bool unschedule = false;
  int polled = 0;
  int destroyed = 0;
  Waker last_waker;

  MockTask() : Task(PW_ASYNC_TASK_NAME("MockTask")) {}

 private:
  Poll<> DoPend(Context& cx) override {
    ++polled;
    if (unschedule) {
      return cx.Unschedule();
    }
    PW_ASYNC_STORE_WAKER(cx, last_waker, "MockTask is waiting for last_waker");
    if (should_complete) {
      return Ready();
    }
    return Pending();
  }

  void DoDestroy() override { ++destroyed; }
};

class MockPendable {
 public:
  MockPendable(Poll<int> value) : value_(value) {}
  Poll<int> Pend(Context& cx) {
    PW_ASYNC_STORE_WAKER(
        cx, last_waker_, "MockPendable is waiting for last_waker");
    return value_;
  }

 private:
  Waker last_waker_;
  Poll<int> value_;
};

TEST(Dispatcher, RunUntilStalledPendsPostedTask) {
  MockTask task;
  task.should_complete = true;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(task.IsRegistered());
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 1);
  EXPECT_FALSE(task.IsRegistered());
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_EQ(dispatcher.tasks_completed(), 1u);
}

TEST(Dispatcher, RunUntilStalledReturnsOnNotReady) {
  MockTask task;
  task.should_complete = false;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_EQ(dispatcher.tasks_completed(), 0u);
}

TEST(Dispatcher, RunUntilStalledDoesNotPendSleepingTask) {
  MockTask task;
  task.should_complete = false;
  Dispatcher dispatcher;
  dispatcher.Post(task);

  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_EQ(dispatcher.tasks_completed(), 0u);

  task.should_complete = true;
  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
  EXPECT_EQ(dispatcher.tasks_completed(), 0u);

  std::move(task.last_waker).Wake();
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 2);
  EXPECT_EQ(task.destroyed, 1);
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
  EXPECT_EQ(dispatcher.tasks_completed(), 1u);
}

TEST(Dispatcher, RunUntilStalledWithNoTasksReturnsReady) {
  Dispatcher dispatcher;
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(dispatcher.tasks_polled(), 0u);
  EXPECT_EQ(dispatcher.tasks_completed(), 0u);
}

TEST(Dispatcher, RunToCompletionPendsMultipleTasks) {
  class CounterTask : public Task {
   public:
    CounterTask(pw::span<Waker> wakers,
                size_t this_waker_i,
                int* counter,
                int until)
        : counter_(counter),
          this_waker_i_(this_waker_i),
          until_(until),
          wakers_(wakers) {}
    int* counter_;
    size_t this_waker_i_;
    int until_;
    pw::span<Waker> wakers_;

   private:
    Poll<> DoPend(Context& cx) override {
      ++(*counter_);
      if (*counter_ >= until_) {
        for (auto& waker : wakers_) {
          std::move(waker).Wake();
        }
        return Ready();
      } else {
        PW_ASYNC_STORE_WAKER(cx,
                             wakers_[this_waker_i_],
                             "CounterTask is waiting for counter_ >= until_");
        return Pending();
      }
    }
  };

  int counter = 0;
  constexpr const int kNumTasks = 3;
  std::array<Waker, kNumTasks> wakers;
  CounterTask task_one(wakers, 0, &counter, kNumTasks);
  CounterTask task_two(wakers, 1, &counter, kNumTasks);
  CounterTask task_three(wakers, 2, &counter, kNumTasks);
  Dispatcher dispatcher;
  dispatcher.Post(task_one);
  dispatcher.Post(task_two);
  dispatcher.Post(task_three);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  // We expect to see 5 total calls to `Pend`:
  // - two which increment counter and return pending
  // - one which increments the counter, returns complete, and wakes the
  //   others
  // - two which have woken back up and complete
  EXPECT_EQ(counter, 5);
  EXPECT_EQ(dispatcher.tasks_polled(), 5u);
}

TEST(Dispatcher, RunPendableUntilStalledReturnsOutputOnReady) {
  MockPendable pollable(Ready(5));
  Dispatcher dispatcher;
  Poll<int> result = dispatcher.RunPendableUntilStalled(pollable);
  EXPECT_EQ(result, Ready(5));
}

TEST(Dispatcher, RunPendableUntilStalledReturnsPending) {
  MockPendable pollable(Pending());
  Dispatcher dispatcher;
  Poll<int> result = dispatcher.RunPendableUntilStalled(pollable);
  EXPECT_EQ(result, Pending());
}

TEST(Dispatcher, RunPendableToCompletionReturnsOutput) {
  MockPendable pollable(Ready(5));
  Dispatcher dispatcher;
  int result = dispatcher.RunPendableToCompletion(pollable);
  EXPECT_EQ(result, 5);
}

TEST(Dispatcher, PostToDispatcherFromInsidePendSucceeds) {
  class TaskPoster : public Task {
   public:
    TaskPoster(Task& task_to_post) : task_to_post_(&task_to_post) {}

   private:
    Poll<> DoPend(Context& cx) override {
      cx.dispatcher().Post(*task_to_post_);
      return Ready();
    }
    Task* task_to_post_;
  };

  MockTask posted_task;
  posted_task.should_complete = true;
  TaskPoster task_poster(posted_task);

  Dispatcher dispatcher;
  dispatcher.Post(task_poster);
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(posted_task.polled, 1);
  EXPECT_EQ(posted_task.destroyed, 1);
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
}

TEST(Dispatcher, RunToCompletionPendsPostedTask) {
  MockTask task;
  task.should_complete = true;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  dispatcher.RunToCompletion(task);
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 1);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);
}

TEST(Dispatcher, RunToCompletionIgnoresDeregisteredTask) {
  Dispatcher dispatcher;
  MockTask task;
  task.should_complete = false;
  dispatcher.Post(task);
  EXPECT_TRUE(task.IsRegistered());
  task.Deregister();
  EXPECT_FALSE(task.IsRegistered());
  dispatcher.RunToCompletion();
  EXPECT_EQ(task.polled, 0);
  EXPECT_EQ(task.destroyed, 0);
  EXPECT_EQ(dispatcher.tasks_polled(), 0u);
}

TEST(Dispatcher, UnscheduleAllowsRepost) {
  Dispatcher dispatcher;
  MockTask task;
  task.should_complete = false;
  task.unschedule = true;
  dispatcher.Post(task);
  EXPECT_TRUE(task.IsRegistered());

  // The dispatcher returns Ready() since the task has opted out of being woken,
  // so it no longer exists in the dispatcher queues.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(dispatcher.tasks_polled(), 1u);

  // The task must be re-posted to run again.
  task.should_complete = true;
  dispatcher.Post(task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(task.polled, 2);
  EXPECT_EQ(dispatcher.tasks_polled(), 2u);
}

}  // namespace
}  // namespace pw::async2
