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
#pragma once

#include "pw_async/dispatcher.h"

namespace pw::async {

class TestDispatcher final : public Dispatcher {
 public:
  explicit TestDispatcher() {}
  ~TestDispatcher() override { RequestStop(); }

  void RequestStop() override;

  // Post caller owned |task|.
  void PostTask(Task& task) override;

  // Post caller owned |task| to be run after |delay|.
  void PostDelayedTask(Task& task,
                       chrono::SystemClock::duration delay) override;

  // Post caller owned |task| to be run at |time|.
  void PostTaskForTime(Task& task,
                       chrono::SystemClock::time_point time) override;

  // Post caller owned |task| to be run immediately then rerun at a regular
  // |interval|.
  void SchedulePeriodicTask(Task& task,
                            chrono::SystemClock::duration interval) override;
  // Post caller owned |task| to be run at |start_time| then rerun at a regular
  // |interval|.
  void SchedulePeriodicTask(
      Task& task,
      chrono::SystemClock::duration interval,
      chrono::SystemClock::time_point start_time) override;

  // Returns true if |task| is succesfully canceled.
  // If cancelation fails, the task may be running or completed.
  // Periodic tasks may run once more after they are canceled.
  bool Cancel(Task& task) override;

  // Execute tasks until the Dispatcher enters a state where none are queued.
  void RunUntilIdle() override;

  // Run the Dispatcher until Now() has reached `end_time`, executing all tasks
  // that come due before then.
  void RunUntil(chrono::SystemClock::time_point end_time) override;

  // Run the Dispatcher until `duration` has elapsed, executing all tasks that
  // come due in that period.
  void RunFor(chrono::SystemClock::duration duration) override;

  // VirtualSystemClock overrides:

  chrono::SystemClock::time_point now() override { return now_; }

 private:
  // Insert |task| into task_queue_ maintaining its min-heap property, keyed by
  // |time_due|.
  void PostTaskInternal(Task& task, chrono::SystemClock::time_point time_due);

  // If no tasks are due, sleeps until a notification is received or until the
  // due time of the next task.
  //
  // If at least one task is due, dequeues and runs each task that is due.
  void RunLoopOnce();

  // A priority queue of scheduled Tasks sorted by earliest due times first.
  IntrusiveList<Task> task_queue_;

  // Tracks the current time as viewed by the test dispatcher.
  chrono::SystemClock::time_point now_;
};

}  // namespace pw::async
