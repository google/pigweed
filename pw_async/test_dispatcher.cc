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
#include "pw_async/test_dispatcher.h"

#include "pw_async/dispatcher_basic.h"
#include "pw_log/log.h"

using namespace std::chrono_literals;

namespace pw::async {

void TestDispatcher::RunUntilIdle() {
  while (!task_queue_.empty()) {
    // Only advance to the due time of the next task because new tasks can be
    // scheduled in the next task.
    now_ = BasicDispatcher::DueTime(task_queue_.front());
    RunLoopOnce();
  }
}

void TestDispatcher::RunUntil(chrono::SystemClock::time_point end_time) {
  while (!task_queue_.empty() &&
         BasicDispatcher::DueTime(task_queue_.front()) <= end_time) {
    now_ = BasicDispatcher::DueTime(task_queue_.front());
    RunLoopOnce();
  }

  if (now_ < end_time) {
    now_ = end_time;
  }
}

void TestDispatcher::RunFor(chrono::SystemClock::duration duration) {
  RunUntil(Now() + duration);
}

void TestDispatcher::RunLoopOnce() {
  while (!task_queue_.empty() &&
         BasicDispatcher::DueTime(task_queue_.front()) <= Now()) {
    Task& task = task_queue_.front();
    task_queue_.pop_front();

    if (BasicDispatcher::IsPeriodic(task)) {
      PostTaskInternal(
          task,
          BasicDispatcher::DueTime(task) + BasicDispatcher::SetInterval(task));
    }

    Context ctx{this, &task};
    task(ctx);
  }
}

void TestDispatcher::RequestStop() {
  PW_LOG_DEBUG("stop requested");
  task_queue_.clear();
}

void TestDispatcher::PostTask(Task& task) { PostTaskForTime(task, Now()); }

void TestDispatcher::PostDelayedTask(Task& task,
                                     chrono::SystemClock::duration delay) {
  PostTaskForTime(task, Now() + delay);
}

void TestDispatcher::PostTaskForTime(Task& task,
                                     chrono::SystemClock::time_point time) {
  PW_LOG_DEBUG("posting task");
  PostTaskInternal(task, time);
}

void TestDispatcher::SchedulePeriodicTask(
    Task& task, chrono::SystemClock::duration interval) {
  SchedulePeriodicTask(task, interval, Now());
}

void TestDispatcher::SchedulePeriodicTask(
    Task& task,
    chrono::SystemClock::duration interval,
    chrono::SystemClock::time_point start_time) {
  BasicDispatcher::SetInterval(task, interval);
  PostTaskForTime(task, start_time);
}

bool TestDispatcher::Cancel(Task& task) { return task_queue_.remove(task); }

void TestDispatcher::PostTaskInternal(
    Task& task, chrono::SystemClock::time_point time_due) {
  BasicDispatcher::SetDueTime(task, time_due);
  auto it_front = task_queue_.begin();
  auto it_behind = task_queue_.before_begin();
  while (it_front != task_queue_.end() &&
         time_due > BasicDispatcher::DueTime(*it_front)) {
    ++it_front;
    ++it_behind;
  }
  task_queue_.insert_after(it_behind, task);
}

}  // namespace pw::async
