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
#include "pw_containers/intrusive_list.h"

namespace pw::async::test::backend {

class NativeFakeDispatcher final {
 public:
  explicit NativeFakeDispatcher(Dispatcher& test_dispatcher);
  ~NativeFakeDispatcher();

  void RequestStop();

  void PostTask(Task& task);

  void PostDelayedTask(Task& task, chrono::SystemClock::duration delay);

  void PostTaskForTime(Task& task, chrono::SystemClock::time_point time);

  void SchedulePeriodicTask(Task& task, chrono::SystemClock::duration interval);
  void SchedulePeriodicTask(Task& task,
                            chrono::SystemClock::duration interval,
                            chrono::SystemClock::time_point start_time);

  bool Cancel(Task& task);

  void RunUntilIdle();

  void RunUntil(chrono::SystemClock::time_point end_time);

  void RunFor(chrono::SystemClock::duration duration);

  chrono::SystemClock::time_point now() { return now_; }

 private:
  // Insert |task| into task_queue_ maintaining its min-heap property, keyed by
  // |time_due|.
  void PostTaskInternal(::pw::async::backend::NativeTask& task,
                        chrono::SystemClock::time_point time_due);

  // Dequeue and run each task that is due.
  void ExecuteDueTasks();

  Dispatcher& dispatcher_;

  // A priority queue of scheduled tasks sorted by earliest due times first.
  IntrusiveList<::pw::async::backend::NativeTask> task_queue_;

  // Tracks the current time as viewed by the test dispatcher.
  chrono::SystemClock::time_point now_;
};

}  // namespace pw::async::test::backend
