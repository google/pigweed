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

#include "pw_chrono/system_clock.h"

namespace pw::async {

class Task;

/// Abstract base class for an asynchronous dispatcher loop.
///
/// `Dispatcher`s run many short, non-blocking units of work on a single thread.
/// This approach has a number of advantages compared with executing concurrent
/// tasks on separate threads:
///
/// - `Dispatcher`s can make more efficient use of system resources, since they
///   don't need to maintain separate thread stacks.
/// - `Dispatcher`s can run on systems without thread support, such as no-RTOS
///   embedded environments.
/// - `Dispatcher`s allow tasks to communicate with one another without the
///   synchronization overhead of locks, atomics, fences, or `volatile`.
///
/// Thread support: `Dispatcher` methods may be safely invoked from any thread,
/// but the resulting tasks will always execute on a single thread. Whether
/// or not methods may be invoked from interrupt context is
/// implementation-defined.
///
/// `VirtualSystemClock`: `Dispatcher` implements `VirtualSystemClock` in order
/// to provide a consistent source of (possibly mocked) time information to
/// tasks.
///
/// A simple default dispatcher implementation is provided by `pw_async_basic`.
class Dispatcher : public chrono::VirtualSystemClock {
 public:
  ~Dispatcher() override = default;

  /// Post caller-owned |task| to be run on the dispatch loop.
  ///
  /// Posted tasks execute in the order they are posted. This ensures that
  /// tasks can re-post themselves and yield in order to allow other tasks the
  /// opportunity to execute.
  virtual void Post(Task& task) { PostAt(task, now()); }

  /// Post caller owned |task| to be run after |delay|.
  virtual void PostAfter(Task& task, chrono::SystemClock::duration delay) {
    PostAt(task, now() + delay);
  }

  /// Post caller owned |task| to be run at |time|.
  virtual void PostAt(Task& task, chrono::SystemClock::time_point time) = 0;

  /// Post caller owned |task| to be run immediately then rerun at a regular
  /// |interval|.
  virtual void PostPeriodic(Task& task,
                            chrono::SystemClock::duration interval) {
    PostPeriodicAt(task, interval, now());
  }

  /// Post caller owned |task| to be run after |delay| then rerun at a regular
  /// |interval|.
  virtual void PostPeriodicAfter(Task& task,
                                 chrono::SystemClock::duration interval,
                                 chrono::SystemClock::duration delay) {
    PostPeriodicAt(task, interval, now() + delay);
  }

  /// Post caller owned |task| to be run at |time| then rerun at a regular
  /// |interval|. |interval| must not be zero.
  virtual void PostPeriodicAt(Task& task,
                              chrono::SystemClock::duration interval,
                              chrono::SystemClock::time_point time) = 0;

  /// Request that a task not be invoked again.
  ///
  /// Periodic tasks may be posted once more after they are canceled. Tasks may
  /// be canceled from within a `TaskFunction` by calling
  /// `context.dispatcher.Cancel(context.task)`.
  /// @return true if `task` successfully canceled, false otherwise. If
  /// cancelation fails, the task may be running or completed.
  virtual bool Cancel(Task& task) = 0;
};

}  // namespace pw::async
