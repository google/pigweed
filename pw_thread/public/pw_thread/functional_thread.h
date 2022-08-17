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

#include "pw_function/function.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"

namespace pw::thread {

// ThreadCore that runs the void() pw::Function passed to it in the constructor.
class FunctionalThreadCore final : public ThreadCore {
 public:
  FunctionalThreadCore(Function<void()>&& func);

  // Exchanges the underlying functions.
  void swap(FunctionalThreadCore& other) { func_.swap(other.func_); }

 protected:
  void Run() override;

 private:
  Function<void()> func_;
};

// Wrapper for pw::thread::Thread to be able to run threads with pw::Function.
// It allows to use lambdas with capture lists.
//
// This wrapper is available only for toolchains with
// PW_THREAD_JOINING_ENABLED=1.
//
// This wrapper doesn't allow detaching, only joining, otherwise the API and
// guarantees are the same as for the pw::thread::Thread.
class FunctionalThread final {
 public:
  FunctionalThread(const Options& options, Function<void()>&& func);

  // Postcondition: The other thread no longer represents a thread of execution.
  FunctionalThread& operator=(FunctionalThread&& other) = default;

  // Precondition: The thread must have been joined.
  ~FunctionalThread() = default;

  FunctionalThread(const FunctionalThread&) = delete;
  FunctionalThread(FunctionalThread&&) = delete;
  FunctionalThread& operator=(const FunctionalThread&) = delete;

  // Returns a value of pw::thread::Id identifying the thread associated with
  // *this.
  // See pw::thread::Thread for more details.
  Id get_id() const { return thread_.get_id(); }

  // Checks if the Thread object identifies an active thread of execution which
  // has not yet been detached.
  // See pw::thread::Thread for more details.
  bool joinable() const { return thread_.joinable(); }

  // Blocks the current thread until the thread identified by *this finishes its
  // execution.
  // See pw::thread::Thread for more details.
  void join() { thread_.join(); }

  // Exchanges the underlying handles and functions of two thread objects.
  void swap(FunctionalThread& other) {
    core_.swap(other.core_);
    thread_.swap(other.thread_);
  }

 private:
  template <typename>
  static constexpr std::bool_constant<PW_THREAD_JOINING_ENABLED>
      kJoiningEnabled = {};

  // Compile-time check that MUST be called from the FunctionalThread
  // constructor.
  template <typename Unused = void>
  static constexpr void CheckEligibility() {
    static_assert(kJoiningEnabled<Unused>,
                  "The selected pw_thread_THREAD backend does not have join() "
                  "enabled (AKA PW_THREAD_JOINING_ENABLED = 1)");
  }

  FunctionalThreadCore core_;
  Thread thread_;
};
}  // namespace pw::thread
