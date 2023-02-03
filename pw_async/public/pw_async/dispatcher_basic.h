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
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/thread_core.h"

namespace pw::async {

class BasicDispatcher final : public Dispatcher, public thread::ThreadCore {
 public:
  explicit BasicDispatcher() : stop_requested_(false) {}
  ~BasicDispatcher() override { RequestStop(); }

  void RequestStop() override PW_LOCKS_EXCLUDED(lock_);

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
  bool Cancel(Task& task) override PW_LOCKS_EXCLUDED(lock_);

  // Execute tasks until the Dispatcher enters a state where none are queued.
  void RunUntilIdle() override;

  // Run the Dispatcher until Now() has reached `end_time`, executing all tasks
  // that come due before then.
  void RunUntil(chrono::SystemClock::time_point end_time) override;
  void RunFor(chrono::SystemClock::duration duration) override;

  // VirtualSystemClock overrides:

  // Returns the current time as viewed by the BasicDispatcher.
  chrono::SystemClock::time_point now() override {
    return chrono::SystemClock::now();
  }

 private:
  // TestDispatcher uses BasicDispatcher methods operating on Task state.
  friend class TestDispatcher;

  void Run() override PW_LOCKS_EXCLUDED(lock_);

  // Insert |task| into task_queue_ maintaining its min-heap property, keyed by
  // |time_due|. Must be holding lock_ when calling this function.
  void PostTaskInternal(Task& task, chrono::SystemClock::time_point time_due)
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // If no tasks are due, sleeps until a notification is received or until the
  // due time of the next task.
  //
  // If at least one task is due, dequeues and runs each task that is due.
  //
  // Must be holding lock_ when calling this function.
  void RunLoopOnce() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Below are several static methods that operate on Tasks to maintain per Task
  // state as necessary for the operation of BasicDispatcher.
  //
  // BasicDispatcher reserves 3 contiguous fields at the beginning of the Task
  // state memory block for these three data:
  //
  // DUE TIME | RECURRANCE INTERVAL FOR PERIODIC TASKS | PERIODICITY FLAG
  //
  // with types:
  //
  // chrono::SystemClock::time_point | chrono::SystemClock::duration | bool
  //
  // Thus, the offsets at which these fields are located in Task state memory:
  static constexpr uint8_t kDueTimeOffset = 0;
  static constexpr uint8_t kIntervalOffset =
      sizeof(chrono::SystemClock::time_point);
  static constexpr uint8_t kPeriodicityOffset =
      kIntervalOffset + sizeof(chrono::SystemClock::duration);

  static chrono::SystemClock::time_point GetDueTime(Task& task) {
    chrono::SystemClock::time_point time;
    memcpy(static_cast<void*>(&time),
           task.State() + kDueTimeOffset,
           sizeof(chrono::SystemClock::time_point));
    return time;
  }
  static void SetDueTime(Task& task, chrono::SystemClock::time_point due_time) {
    memcpy(task.State() + kDueTimeOffset,
           &due_time,
           sizeof(chrono::SystemClock::time_point));
  }

  static chrono::SystemClock::duration GetInterval(Task& task) {
    chrono::SystemClock::duration duration;
    memcpy(static_cast<void*>(&duration),
           task.State() + kIntervalOffset,
           sizeof(chrono::SystemClock::duration));
    return duration;
  }
  static void SetInterval(Task& task, chrono::SystemClock::duration interval) {
    memcpy(task.State() + kIntervalOffset,
           &interval,
           sizeof(chrono::SystemClock::duration));
    // Set periodicity flag as well.
    bool true_value = true;
    memcpy(task.State() + kPeriodicityOffset, &true_value, sizeof(bool));
  }

  static bool IsPeriodic(Task& task) {
    bool periodic;
    memcpy(&periodic, task.State() + kPeriodicityOffset, sizeof(bool));
    return periodic;
  }

  sync::InterruptSpinLock lock_;
  sync::TimedThreadNotification timed_notification_;
  bool stop_requested_ PW_GUARDED_BY(lock_);
  // A priority queue of scheduled Tasks sorted by earliest due times first.
  IntrusiveList<Task> task_queue_ PW_GUARDED_BY(lock_);
};

}  // namespace pw::async
