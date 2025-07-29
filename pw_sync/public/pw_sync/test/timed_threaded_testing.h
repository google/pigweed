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
#include "pw_sync/test/threaded_testing.h"
#include "pw_thread/sleep.h"

namespace pw::sync::test {

/// Test fixture that provides a detached thread for testing synchronization
/// primitives concurrently.
///
/// When writing unit tests for timed synchronization primitives, do not rely on
/// elapsed time for synchronization. In other, do not assume that a thread will
/// be able to complete some activity before another thread awakens after
/// sleeping for some duration. Instead, use this test fixture to write tests
/// that validate whether the primitive correctly waits for some other
/// condition, e.g. that it only acquires a lock after another thread that was
/// holding and sleeping awoke and released it.
///
/// Also, keep durations limited to a few milliseconds at most, as longer
/// durations will slow overall tests execution.
template <ThreadingRequirement kThreading>
class BasicTimedThreadedTest : public BasicThreadedTest<kThreading> {
 private:
  using Base = BasicThreadedTest<kThreading>;

 protected:
  using Duration = chrono::SystemClock::duration;
  using TimePoint = chrono::SystemClock::time_point;

  /// Like ThreadedTest::Start`, but performs starting actions  `after` a
  /// given duration.
  void StartAfter(Duration after) {
    start_at_ = chrono::SystemClock::now() + after;
    Base::Start();
  }

  /// Like ThreadedTest::Start`, but performs starting actions  `at` a
  /// given time.
  void StartAt(TimePoint at) {
    start_at_ = at;
    Base::Start();
  }

  /// Like ThreadedTest::Stop`, but performs stopping actions  `after` a
  /// given duration.
  void StopAfter(Duration after) {
    stop_at_ = chrono::SystemClock::now() + after;
    Base::Stop();
  }

  /// Like ThreadedTest::Stop`, but performs stopping actions  `at` a
  /// given time.
  void StopAt(TimePoint at) {
    stop_at_ = at;
    Base::Stop();
  }

  /// Like ThreadedTest::RunOnce`, but performs actions  `after` a given
  /// duration.
  void RunOnceAfter(Duration after) {
    StartAfter(after);
    Base::Stop();
  }

  /// Like ThreadedTest::RunOnce`, but performs actions  `at` a given time.
  void RunOnceAt(TimePoint at) {
    StartAt(at);
    Base::Stop();
  }

  template <typename Func>
  Duration Measure(Func&& func) {
    TimePoint start = chrono::SystemClock::now();
    func();
    return chrono::SystemClock::now() - start;
  }

 private:
  /// @copydoc ThreadedTest::WaitUntilStart
  void WaitUntilStart() override { this_thread::sleep_until(start_at_); }

  /// @copydoc ThreadedTest::WaitUntilStop
  void WaitUntilStop() override { this_thread::sleep_until(stop_at_); }

  TimePoint start_at_;
  TimePoint stop_at_;
};

using TimedThreadedTest =
    BasicTimedThreadedTest<ThreadingRequirement::kRequired>;
using TimedOptionallyThreadedTest =
    BasicTimedThreadedTest<ThreadingRequirement::kOptional>;

}  // namespace pw::sync::test
