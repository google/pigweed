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

#include "gtest/gtest.h"
#include "pw_containers/vector.h"

namespace pw::async2 {
namespace {

class MockTask : public Task {
 public:
  bool should_complete = false;
  int polled = 0;
  int destroyed = 0;
  std::optional<Waker> last_waker = std::nullopt;

 private:
  Poll<> DoPend(Context& cx) override {
    ++polled;
    last_waker = cx.GetWaker(WaitReason::Unspecified());
    if (should_complete) {
      return Ready();
    } else {
      return Pending();
    }
  }
  void DoDestroy() override { ++destroyed; }
};

TEST(Dispatcher, RunUntilStalledPendsPostedTask) {
  MockTask task;
  task.should_complete = true;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 1);
}

TEST(Dispatcher, RunUntilStalledReturnsOnNotReady) {
  MockTask task;
  task.should_complete = false;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);
}

TEST(Dispatcher, RunUntilStalledDoesNotPendSleepingTask) {
  MockTask task;
  task.should_complete = false;
  Dispatcher dispatcher;
  dispatcher.Post(task);

  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);

  task.should_complete = true;
  EXPECT_FALSE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 0);

  std::move(*task.last_waker).Wake();
  EXPECT_TRUE(dispatcher.RunUntilStalled(task).IsReady());
  EXPECT_EQ(task.polled, 2);
  EXPECT_EQ(task.destroyed, 1);
}

TEST(Dispatcher, RunUntilCompletePendsMultipleTasks) {
  class CounterTask : public Task {
   public:
    CounterTask(pw::Vector<Waker>* wakers, int* counter, int until)
        : counter_(counter), until_(until), wakers_(wakers) {}
    int* counter_;
    int until_;
    pw::Vector<Waker>* wakers_;

   private:
    Poll<> DoPend(Context& cx) override {
      ++(*counter_);
      if (*counter_ >= until_) {
        for (auto& waker : *wakers_) {
          std::move(waker).Wake();
        }
        return Ready();
      } else {
        wakers_->push_back(cx.GetWaker(WaitReason::Unspecified()));
        return Pending();
      }
    }
  };

  int counter = 0;
  constexpr const int num_tasks = 3;
  pw::Vector<Waker, num_tasks> wakers;
  CounterTask task_one(&wakers, &counter, num_tasks);
  CounterTask task_two(&wakers, &counter, num_tasks);
  CounterTask task_three(&wakers, &counter, num_tasks);
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
}

TEST(Dispatcher, RunToCompletionPendsPostedTask) {
  MockTask task;
  task.should_complete = true;
  Dispatcher dispatcher;
  dispatcher.Post(task);
  dispatcher.RunToCompletion(task);
  EXPECT_EQ(task.polled, 1);
  EXPECT_EQ(task.destroyed, 1);
}

}  // namespace
}  // namespace pw::async2
