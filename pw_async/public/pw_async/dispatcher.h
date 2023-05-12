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
  ///
  /// A given |task| must only be posted to a single `Dispatcher`.
  virtual void Post(Task& task) { PostAt(task, now()); }

  /// Post caller owned |task| to be run after |delay|.
  ///
  /// If |task| was already posted to run at an earlier time (before |delay|
  /// would expire), |task| must be run at the earlier time, and |task|
  /// *may* also be run at the later time.
  virtual void PostAfter(Task& task, chrono::SystemClock::duration delay) {
    PostAt(task, now() + delay);
  }

  /// Post caller owned |task| to be run at |time|.
  ///
  /// If |task| was already posted to run before |time|,
  /// |task| must be run at the earlier time, and |task| *may* also be run at
  /// the later time.
  virtual void PostAt(Task& task, chrono::SystemClock::time_point time) = 0;

  /// Prevent a `Post`ed task from starting.
  ///
  /// Returns:
  ///   true: the task was successfully canceled and will not be run by the
  ///     dispatcher until `Post`ed again.
  ///   false: the task could not be cancelled because it either was not
  ///     posted, already ran, or is currently running on the `Dispatcher`
  ///     thread.
  virtual bool Cancel(Task& task) = 0;
};

}  // namespace pw::async
