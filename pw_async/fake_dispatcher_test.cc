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
#include "pw_containers/vector.h"
#include "pw_string/to_string.h"

#define ASSERT_OK(status) ASSERT_EQ(OkStatus(), status)
#define ASSERT_CANCELLED(status) ASSERT_EQ(Status::Cancelled(), status)

using namespace std::chrono_literals;

struct CallCounts {
  int ok = 0;
  int cancelled = 0;
  bool operator==(const CallCounts& other) const {
    return ok == other.ok && cancelled == other.cancelled;
  }
};

namespace pw {
template <>
StatusWithSize ToString<CallCounts>(const CallCounts& value,
                                    span<char> buffer) {
  return string::Format(buffer,
                        "CallCounts {.ok = %d, .cancelled = %d}",
                        value.ok,
                        value.cancelled);
}
}  // namespace pw

namespace pw::async::test {
namespace {

struct CallCounter {
  CallCounts counts;
  auto fn() {
    return [this](Context&, Status status) {
      if (status.ok()) {
        this->counts.ok++;
      } else if (status.IsCancelled()) {
        this->counts.cancelled++;
      }
    };
  }
};

TEST(FakeDispatcher, UnpostedTasksDontRun) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{});
}

TEST(FakeDispatcher, PostedTaskRunsOnce) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.Post(task);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.ok = 1});
}

TEST(FakeDispatcher, TaskPostedTwiceBeforeRunningRunsOnce) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.Post(task);
  dispatcher.Post(task);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.ok = 1});
}

TEST(FakeDispatcher, TaskRepostedAfterRunningRunsTwice) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.Post(task);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.ok = 1});
  dispatcher.Post(task);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.ok = 2});
}

TEST(FakeDispatcher, TwoPostedTasksEachRunOnce) {
  FakeDispatcher dispatcher;
  CallCounter counter_1;
  Task task_1(counter_1.fn());
  CallCounter counter_2;
  Task task_2(counter_2.fn());
  dispatcher.Post(task_1);
  dispatcher.Post(task_2);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter_1.counts, CallCounts{.ok = 1});
  EXPECT_EQ(counter_2.counts, CallCounts{.ok = 1});
}

TEST(FakeDispatcher, PostedTasksRunInOrderForFairness) {
  FakeDispatcher dispatcher;
  pw::Vector<uint8_t, 3> task_run_order;
  Task task_1([&task_run_order](auto...) { task_run_order.push_back(1); });
  Task task_2([&task_run_order](auto...) { task_run_order.push_back(2); });
  Task task_3([&task_run_order](auto...) { task_run_order.push_back(3); });
  dispatcher.Post(task_1);
  dispatcher.Post(task_2);
  dispatcher.Post(task_3);
  dispatcher.RunUntilIdle();
  pw::Vector<uint8_t, 3> expected_run_order({1, 2, 3});
  EXPECT_EQ(task_run_order, expected_run_order);
}

TEST(FakeDispatcher, RequestStopQueuesPreviouslyPostedTaskWithCancel) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.Post(task);
  dispatcher.RequestStop();
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.cancelled = 1});
}

TEST(FakeDispatcher, RequestStopQueuesNewlyPostedTaskWithCancel) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task(counter.fn());
  dispatcher.RequestStop();
  dispatcher.Post(task);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{.cancelled = 1});
}

TEST(FakeDispatcher, RunUntilIdleDoesNotRunFutureTask) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  // Should not run; RunUntilIdle() does not advance time.
  Task task(counter.fn());
  dispatcher.PostAfter(task, 1ms);
  dispatcher.RunUntilIdle();
  EXPECT_EQ(counter.counts, CallCounts{});
}

TEST(FakeDispatcher, PostAfterRunsTasksInSequence) {
  FakeDispatcher dispatcher;
  pw::Vector<uint8_t, 3> task_run_order;
  Task task_1([&task_run_order](auto...) { task_run_order.push_back(1); });
  Task task_2([&task_run_order](auto...) { task_run_order.push_back(2); });
  Task task_3([&task_run_order](auto...) { task_run_order.push_back(3); });
  dispatcher.PostAfter(task_1, 50ms);
  dispatcher.PostAfter(task_2, 25ms);
  dispatcher.PostAfter(task_3, 100ms);
  dispatcher.RunFor(125ms);
  pw::Vector<uint8_t, 3> expected_run_order({2, 1, 3});
  EXPECT_EQ(task_run_order, expected_run_order);
}

TEST(FakeDispatcher, CancelInsideOtherTaskCancelsTaskWithoutRunningIt) {
  FakeDispatcher dispatcher;

  CallCounter cancelled_task_counter;
  Task cancelled_task(cancelled_task_counter.fn());

  Task canceling_task([&cancelled_task](Context& c, Status status) {
    ASSERT_OK(status);
    ASSERT_TRUE(c.dispatcher->Cancel(cancelled_task));
  });

  dispatcher.Post(canceling_task);
  dispatcher.Post(cancelled_task);
  dispatcher.RunUntilIdle();

  // NOTE:  the cancelled task is *not* run with `Cancel`.
  // This is likely to produce strange behavior, and this contract should
  // be revisited and carefully documented.
  EXPECT_EQ(cancelled_task_counter.counts, CallCounts{});
}

TEST(FakeDispatcher, CancelInsideCurrentTaskFails) {
  FakeDispatcher dispatcher;

  Task self_cancel_task;
  self_cancel_task.set_function([&self_cancel_task](Context& c, Status status) {
    ASSERT_OK(status);
    ASSERT_FALSE(c.dispatcher->Cancel(self_cancel_task));
  });
  dispatcher.Post(self_cancel_task);
  dispatcher.RunUntilIdle();
}

TEST(FakeDispatcher, RequestStopInsideOtherTaskCancelsOtherTask) {
  FakeDispatcher dispatcher;

  // This task is never executed and is cleaned up in RequestStop().
  CallCounter task_counter;
  Task task(task_counter.fn());

  int stop_count = 0;
  Task stop_task([&stop_count]([[maybe_unused]] Context& c, Status status) {
    ASSERT_OK(status);
    stop_count++;
    static_cast<FakeDispatcher*>(c.dispatcher)->RequestStop();
  });

  dispatcher.Post(stop_task);
  dispatcher.Post(task);

  dispatcher.RunUntilIdle();
  EXPECT_EQ(stop_count, 1);
  EXPECT_EQ(task_counter.counts, CallCounts{.cancelled = 1});
}

TEST(FakeDispatcher, TasksCancelledByDispatcherDestructor) {
  CallCounter counter;
  Task task0(counter.fn()), task1(counter.fn()), task2(counter.fn());

  {
    FakeDispatcher dispatcher;
    dispatcher.PostAfter(task0, 10s);
    dispatcher.PostAfter(task1, 10s);
    dispatcher.PostAfter(task2, 10s);
  }

  ASSERT_EQ(counter.counts, CallCounts{.cancelled = 3});
}

TEST(DispatcherBasic, TasksCancelledByRunFor) {
  FakeDispatcher dispatcher;
  CallCounter counter;
  Task task0(counter.fn()), task1(counter.fn()), task2(counter.fn());
  dispatcher.PostAfter(task0, 10s);
  dispatcher.PostAfter(task1, 10s);
  dispatcher.PostAfter(task2, 10s);

  dispatcher.RequestStop();
  dispatcher.RunFor(5s);
  ASSERT_EQ(counter.counts, CallCounts{.cancelled = 3});
}

}  // namespace
}  // namespace pw::async::test
