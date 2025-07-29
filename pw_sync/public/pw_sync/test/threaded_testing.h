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

#include "pw_sync/thread_notification.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"

namespace pw::sync::test {

enum class ThreadingRequirement { kRequired, kOptional };

/// Test fixture that can perform actions on a spawned thread when thread
/// joining is enabled.
///
/// This fixture can be used to test synchronization primitives concurrently.
///
/// Tests must indicate whether threading is required or optional. When thread
/// joining is disabled, the actions must be able to be performed on the main
/// test thread.
///
/// Examples:
///   - It is allowable to release a thread notification from the same thread
///     that subsequently tries to acquire it. For such a test, threading may
///     be optional.
///   - It is undefined behavior to try to lock a mutex from the same thread
///     that already holds a lock. For such a test, threading is required.
///
/// It is a compile-time error to require threading when joining is not
/// available. Platforms that do not supported thread joining must Unit tests
/// that require threading must
template <ThreadingRequirement kThreading>
class BasicThreadedTest : public ::testing::Test {
 protected:
  void TearDown() override { Stop(); }

  /// Performs initialization actions.
  ///
  /// If joining is enabled, these actions are performed on a spawned thread.
  /// Otherwise, threading must be optional, and the actions are performed on
  /// the main thread.
  ///
  /// This call will block until the starting actions are complete.
  ///
  /// @warning To be thread-safe, this MUST only be called from the thread that
  /// created the test fixture. It also MUST NOT be called twice without an
  /// intervening call to `Stop`.
  void Start() {
    if constexpr (PW_THREAD_JOINING_ENABLED ||
                  kThreading == ThreadingRequirement::kRequired) {
      StartOnNewThread();
    } else {
      StartOnCurrentThread();
    }
  }

  /// Performs finilization actions.
  ///
  /// If joining is enabled, these actions are performed on the previously
  /// spawned thread, which is then joined. Otherwise, threading must be
  /// optional, and the actions are performed on the main thread.
  ///
  /// This call will block until the stopping actions are complete.
  void Stop() {
    if constexpr (PW_THREAD_JOINING_ENABLED ||
                  kThreading == ThreadingRequirement::kRequired) {
      StopOnNewThread();
    } else {
      StopOnCurrentThread();
    }
  }

  /// Runs all initialization and/or finalization actions once.
  void RunOnce() {
    Start();
    Stop();
  }

 private:
  /// Blocks until the starting actions should be performed.
  ///
  /// By default, starts immediately.
  virtual void WaitUntilStart() {}

  /// Performs starting actions..
  ///
  /// By default, does nothing.
  virtual void DoStart() {}

  /// Blocks until the stopping actions should be performed.
  ///
  /// By default, stops immediately.
  virtual void WaitUntilStop() {}

  /// Performs stopping actions.
  ///
  /// By default, does nothing.
  virtual void DoStop() {}

  // Waits until starting actions should be performed, then performs them.
  void StartOnCurrentThread() {
    WaitUntilStart();
    DoStart();
  }

  // Waits until starting actions should be performed, then performs them.
  void StopOnCurrentThread() {
    WaitUntilStop();
    DoStop();
  }

  // Spanws a new thread to run starting and stopping actions.
  void StartOnNewThread() {
    ASSERT_FALSE(thread_.joinable());
    thread_ = thread::Thread(context_, ThreadAttrs(), [this]() {
      started_.release();
      StartOnCurrentThread();
      stopped_.acquire();
      StopOnCurrentThread();
    });
    started_.acquire();
  }

  virtual void StopOnNewThread() {
#if PW_THREAD_JOINING_ENABLED
    // Waits until the stopping actions are complete and joins the thread.
    if (thread_.joinable()) {
      stopped_.release();
      thread_.join();
    }
#else   // !PW_THREAD_JOINING_ENABLED
    // When thread joining is disabled and a test requires threading, this
    // method must be overridden.
    //
    // The thread used for testing must be detached and non-portable options
    // such as `WaitUntilDetachedThreadsCleanedUp` used to ensure the thread
    // completes before the test.
    static_assert(kThreading == ThreadingRequirement::kOptional,
                  "Threading is required for unit tests, but thread joining is "
                  "not enabled and `StopOnNewThread` has not been overridden.");
#endif  // PW_THREAD_JOINING_ENABLED
  }

  thread::Thread thread_;
  DefaultThreadContext context_;
  ThreadNotification started_;
  ThreadNotification stopped_;
};

using ThreadedTest = BasicThreadedTest<ThreadingRequirement::kRequired>;
using OptionallyThreadedTest =
    BasicThreadedTest<ThreadingRequirement::kOptional>;

}  // namespace pw::sync::test
