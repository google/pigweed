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

/// Asynchronous Dispatcher abstract class. A default implementation is provided
/// in pw_async_basic.
///
/// Dispatcher implements VirtualSystemClock so the Dispatcher's time can be
/// injected into other modules under test. This is useful for consistently
/// simulating time when using FakeDispatcher (rather than using
/// chrono::SimulatedSystemClock separately).
class Dispatcher : public chrono::VirtualSystemClock {
 public:
  ~Dispatcher() override = default;

  /// Post caller owned |task|.
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

  /// Returns true if |task| is succesfully canceled.
  /// If cancelation fails, the task may be running or completed.
  /// Periodic tasks may be posted once more after they are canceled.
  virtual bool Cancel(Task& task) = 0;
};

}  // namespace pw::async
