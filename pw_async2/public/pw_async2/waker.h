// Copyright 2025 The Pigweed Authors
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

#include "pw_async2/internal/lock.h"
#include "pw_sync/lock_annotations.h"

namespace pw::async2 {

class Task;

/// Stores a waker associated with the current context in ``waker_out``.
/// When ``waker_out`` is later awoken with :cpp:func:`pw::async2::Waker::Wake`,
/// the :cpp:class:`pw::async2::Task` associated with ``cx`` will be awoken and
/// its ``DoPend`` method will be invoked again.
///
/// NOTE: wait_reason_string is currently unused, but will be used for debugging
/// in the future.
#define PW_ASYNC_STORE_WAKER(context, waker_out, wait_reason_string)           \
  do {                                                                         \
    [[maybe_unused]] constexpr const char* __MUST_BE_STR = wait_reason_string; \
    context.InternalStoreWaker(waker_out);                                     \
  } while (0)

/// Stores a waker associated with ``waker_in`` in ``waker_out``.
/// When ``waker_out`` is later awoken with :cpp:func:`pw::async2::Waker::Wake`,
/// the :cpp:class:`pw::async2::Task` associated with ``waker_in`` will be
/// awoken and its ``DoPend`` method will be invoked again.
///
/// NOTE: wait_reason_string is currently unused, but will be used for debugging
/// in the future.
#define PW_ASYNC_CLONE_WAKER(waker_in, waker_out, wait_reason_string)          \
  do {                                                                         \
    [[maybe_unused]] constexpr const char* __MUST_BE_STR = wait_reason_string; \
    waker_in.InternalCloneInto(waker_out);                                     \
  } while (0)

/// An object which can respond to asynchronous events by queueing work to
/// be done in response, such as placing a ``Task`` on a ``Dispatcher`` loop.
///
/// ``Waker`` s are often held by I/O objects, custom concurrency primitives,
/// or interrupt handlers. Once the thing the ``Task`` was waiting for is
/// available, ``Wake`` should be called so that the ``Task`` is alerted and
/// may process the event.
///
/// ``Waker`` s may be held for any lifetime, and will be automatically
/// nullified when the underlying ``Dispatcher`` or ``Task`` is deleted.
///
/// ``Waker`` s are most commonly created by ``Dispatcher`` s, which pass them
/// into ``Task::Pend`` via its ``Context`` argument.
class Waker {
  friend class Task;
  friend class NativeDispatcherBase;

 public:
  constexpr Waker() = default;
  Waker(Waker&& other) noexcept PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// Replace this ``Waker`` with another.
  ///
  /// This operation is guaranteed to be thread-safe.
  Waker& operator=(Waker&& other) noexcept PW_LOCKS_EXCLUDED(dispatcher_lock());

  ~Waker() noexcept { RemoveFromTaskWakerList(); }

  /// Wakes up the ``Waker``'s creator, alerting it that an asynchronous
  /// event has occurred that may allow it to make progress.
  ///
  /// ``Wake`` operates on an rvalue reference (``&&``) in order to indicate
  /// that the event that was waited on has been complete. This makes it
  /// possible to track the outstanding events that may cause a ``Task`` to
  /// wake up and make progress.
  ///
  /// This operation is guaranteed to be thread-safe.
  void Wake() && PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// INTERNAL-ONLY: users should use the `PW_ASYNC_CLONE_WAKER` macro.
  ///
  /// Creates a second ``Waker`` from this ``Waker``.
  ///
  /// ``Clone`` is made explicit in order to allow for easier tracking of
  /// the different ``Waker``s that may wake up a ``Task``.
  ///
  /// This operation is guaranteed to be thread-safe.
  void InternalCloneInto(Waker& waker_out) &
      PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// Returns whether this ``Waker`` is empty.
  ///
  /// Empty wakers are those that perform no action upon wake. These may be
  /// created either via the default no-argument constructor or by
  /// calling ``Clear`` or ``std::move`` on a ``Waker``, after which the
  /// moved-from ``Waker`` will be empty.
  ///
  /// This operation is guaranteed to be thread-safe.
  [[nodiscard]] bool IsEmpty() const PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// Clears this ``Waker``.
  ///
  /// After this call, ``Wake`` will no longer perform any action, and
  /// ``IsEmpty`` will return ``true``.
  ///
  /// This operation is guaranteed to be thread-safe.
  void Clear() PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    RemoveFromTaskWakerList();
  }

 private:
  Waker(Task& task) PW_LOCKS_EXCLUDED(dispatcher_lock()) : task_(&task) {
    InsertIntoTaskWakerList();
  }

  void InsertIntoTaskWakerList();
  void InsertIntoTaskWakerListLocked()
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());
  void RemoveFromTaskWakerList();
  void RemoveFromTaskWakerListLocked()
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // The ``Task`` to poll when awoken.
  Task* task_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;

  // The next ``Waker`` that may awaken this ``Task``.
  // This list is controlled by the corresponding ``Task``.
  Waker* next_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
};

}  // namespace pw::async2
