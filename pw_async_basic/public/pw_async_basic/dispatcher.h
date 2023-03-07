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
#pragma once

#include "pw_async/dispatcher.h"
#include "pw_async/task.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/thread_core.h"

namespace pw::async {

class BasicDispatcher final : public Dispatcher, public thread::ThreadCore {
 public:
  explicit BasicDispatcher() : stop_requested_(false) {}
  ~BasicDispatcher() override { RequestStop(); }

  // Dispatcher overrides:
  void RequestStop() override PW_LOCKS_EXCLUDED(lock_);
  void PostTask(Task& task) override;
  void PostDelayedTask(Task& task,
                       chrono::SystemClock::duration delay) override;
  void PostTaskForTime(Task& task,
                       chrono::SystemClock::time_point time) override;
  void SchedulePeriodicTask(Task& task,
                            chrono::SystemClock::duration interval) override;
  void SchedulePeriodicTask(
      Task& task,
      chrono::SystemClock::duration interval,
      chrono::SystemClock::time_point start_time) override;
  bool Cancel(Task& task) override PW_LOCKS_EXCLUDED(lock_);
  void RunUntilIdle() override;
  void RunUntil(chrono::SystemClock::time_point end_time) override;
  void RunFor(chrono::SystemClock::duration duration) override;

  // VirtualSystemClock overrides:
  chrono::SystemClock::time_point now() override {
    return chrono::SystemClock::now();
  }

  // ThreadCore overrides:
  void Run() override PW_LOCKS_EXCLUDED(lock_);

 private:
  // Insert |task| into task_queue_ maintaining its min-heap property, keyed by
  // |time_due|.
  void PostTaskInternal(backend::NativeTask& task,
                        chrono::SystemClock::time_point time_due)
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // If no tasks are due, sleep until a notification is received, the next task
  // comes due, or a timeout elapses; whichever occurs first.
  void MaybeSleep() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Dequeue and run each task that is due.
  void ExecuteDueTasks() PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  sync::InterruptSpinLock lock_;
  sync::TimedThreadNotification timed_notification_;
  bool stop_requested_ PW_GUARDED_BY(lock_);
  // A priority queue of scheduled Tasks sorted by earliest due times first.
  IntrusiveList<backend::NativeTask> task_queue_ PW_GUARDED_BY(lock_);
};

}  // namespace pw::async
