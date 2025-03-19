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

#include "pw_async2/dispatcher_base.h"  // IWYU pragma: export
#include "pw_async2/dispatcher_native.h"

namespace pw::async2 {
namespace internal {

template <typename Pendable>
class PendableAsTaskWithOutput : public Task {
 public:
  using OutputType = PendOutputOf<Pendable>;
  PendableAsTaskWithOutput(Pendable& pendable)
      : pendable_(pendable), output_(Pending()) {}
  OutputType&& TakeOutput() { return std::move(*output_); }

 private:
  Poll<> DoPend(Context& cx) final {
    output_ = pendable_.Pend(cx);
    return output_.Readiness();
  }
  Pendable& pendable_;
  Poll<OutputType> output_;
};

}  // namespace internal

/// A single-threaded cooperatively-scheduled runtime for async tasks.
class Dispatcher {
 public:
  /// Constructs a new async Dispatcher.
  Dispatcher() = default;
  Dispatcher(Dispatcher&) = delete;
  Dispatcher(Dispatcher&&) = delete;
  Dispatcher& operator=(Dispatcher&) = delete;
  Dispatcher& operator=(Dispatcher&&) = delete;
  ~Dispatcher() { native_.Deregister(); }

  /// Tells the ``Dispatcher`` to run ``Task`` to completion.
  /// This method does not block.
  ///
  /// After ``Post`` is called, ``Task::Pend`` will be invoked once.
  /// If ``Task::Pend`` does not complete, the ``Dispatcher`` will wait
  /// until the ``Task`` is "awoken", at which point it will call ``Pend``
  /// again until the ``Task`` completes.
  ///
  /// This method is thread-safe and interrupt-safe.
  void Post(Task& task) PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    native_.Post(task);
  }

  /// Runs tasks until none are able to make immediate progress.
  Poll<> RunUntilStalled() PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    return native_.DoRunUntilStalled(*this, nullptr);
  }

  /// Runs tasks until none are able to make immediate progress, or until
  /// ``task`` completes.
  ///
  /// Returns whether ``task`` completed.
  Poll<> RunUntilStalled(Task& task)
      PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    return native_.DoRunUntilStalled(*this, &task);
  }

  /// Runs tasks until none are able to make immediate progress, or until
  /// ``pendable`` completes.
  ///
  /// Returns a ``Poll`` containing the possible output of ``pendable``.
  template <typename Pendable>
  Poll<PendOutputOf<Pendable>> RunPendableUntilStalled(Pendable& pendable)
      PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    internal::PendableAsTaskWithOutput task(pendable);
    Post(task);
    if (RunUntilStalled(task).IsReady()) {
      return task.TakeOutput();
    }
    // Ensure that the task is no longer registered, as it will be destroyed
    // once we return.
    //
    // This operation will not block because we are on the dispatcher thread
    // and the dispatcher is not currently running (we just ran it).
    task.Deregister();
    return Pending();
  }

  /// Runs until all tasks complete.
  void RunToCompletion() PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    native_.DoRunToCompletion(*this, nullptr);
  }

  /// Runs until ``task`` completes.
  void RunToCompletion(Task& task) PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    native_.DoRunToCompletion(*this, &task);
  }

  /// Runs until ``pendable`` completes, returning the output of ``pendable``.
  template <typename Pendable>
  PendOutputOf<Pendable> RunPendableToCompletion(Pendable& pendable)
      PW_LOCKS_EXCLUDED(impl::dispatcher_lock()) {
    internal::PendableAsTaskWithOutput task(pendable);
    Post(task);
    native_.DoRunToCompletion(*this, &task);
    return task.TakeOutput();
  }

  /// Returns a reference to the native backend-specific dispatcher type.
  pw::async2::backend::NativeDispatcher& native() { return native_; }

 private:
  pw::async2::backend::NativeDispatcher native_;
};

}  // namespace pw::async2
