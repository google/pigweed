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

#include "pw_async2/context.h"
#include "pw_async2/internal/lock.h"
#include "pw_async2/task.h"
#include "pw_async2/waker.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::async2 {

// Forward-declare ``Dispatcher``.
// This concrete type must be provided by a backend.
class Dispatcher;

template <typename T>
using PendOutputOf = typename decltype(std::declval<T>().Pend(
    std::declval<Context&>()))::OutputType;

template <typename, typename = void>
constexpr bool is_pendable = false;

template <typename T>
constexpr bool is_pendable<T, std::void_t<PendOutputOf<T>>> = true;

// Windows GCC doesn't realize the nonvirtual destructor is protected.
PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wnon-virtual-dtor");

/// A base class used by ``Dispatcher`` implementations.
///
/// Note that only one ``Dispatcher`` implementation should exist per
/// toolchain. However, a common base class is used in order to share
/// behavior and standardize the interface of these ``Dispatcher`` s,
/// and to prevent build system cycles due to ``Task`` needing to refer
/// to the ``Dispatcher`` class.
class NativeDispatcherBase {
 public:
  NativeDispatcherBase() = default;
  NativeDispatcherBase(NativeDispatcherBase&) = delete;
  NativeDispatcherBase(NativeDispatcherBase&&) = delete;
  NativeDispatcherBase& operator=(NativeDispatcherBase&) = delete;
  NativeDispatcherBase& operator=(NativeDispatcherBase&&) = delete;

 protected:
  ~NativeDispatcherBase() = default;

  /// Check that a task is posted on this ``Dispatcher``.
  bool HasPostedTask(Task& task)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock()) {
    return task.dispatcher_ == this;
  }

  /// Removes references to this ``NativeDispatcherBase`` from all linked
  /// ``Task`` s and ``Waker`` s.
  ///
  /// This must be called by ``Dispatcher`` implementations in their
  /// destructors. It is not called by the ``NativeDispatcherBase`` destructor,
  /// as doing so would allow the ``Dispatcher`` to be referenced between the
  /// calls to ``~Dispatcher`` and ``~NativeDispatcherBase``.
  void Deregister() PW_LOCKS_EXCLUDED(dispatcher_lock());

  void Post(Task& task) PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// Information about whether and when to sleep until as returned by
  /// ``NativeDispatcherBase::AttemptRequestWake``.
  ///
  /// This should only be used by ``Dispatcher`` implementations.
  class [[nodiscard]] SleepInfo {
    friend class NativeDispatcherBase;

   public:
    bool should_sleep() const { return should_sleep_; }

   private:
    SleepInfo(bool should_sleep) : should_sleep_(should_sleep) {}

    static SleepInfo DontSleep() { return SleepInfo(false); }

    static SleepInfo Indefinitely() { return SleepInfo(true); }

    bool should_sleep_;
  };

  /// Indicates that this ``Dispatcher`` is about to go to sleep and
  /// requests that it be awoken when more work is available in the future.
  ///
  /// Dispatchers must invoke this method before sleeping in order to ensure
  /// that they receive a ``DoWake`` call when there is more work to do.
  ///
  /// The returned ``SleepInfo`` will describe whether and for how long the
  /// ``Dispatcher`` implementation should go to sleep. Notably it will return
  /// that the ``Dispatcher`` should not sleep if there is still more work to
  /// be done.
  ///
  /// @param  allow_empty Whether or not to allow sleeping when no tasks are
  ///                     registered.
  SleepInfo AttemptRequestWake(bool allow_empty)
      PW_LOCKS_EXCLUDED(dispatcher_lock());

  /// Information about the result of a call to ``RunOneTask``.
  ///
  /// This should only be used by ``Dispatcher`` implementations.
  class [[nodiscard]] RunOneTaskResult {
   public:
    RunOneTaskResult(bool completed_all_tasks,
                     bool completed_main_task,
                     bool ran_a_task)
        : completed_all_tasks_(completed_all_tasks),
          completed_main_task_(completed_main_task),
          ran_a_task_(ran_a_task) {}
    bool completed_all_tasks() const { return completed_all_tasks_; }
    bool completed_main_task() const { return completed_main_task_; }
    bool ran_a_task() const { return ran_a_task_; }

   private:
    bool completed_all_tasks_;
    bool completed_main_task_;
    bool ran_a_task_;
  };

  /// Attempts to run a single task, returning whether any tasks were
  /// run, and whether `task_to_look_for` was run.
  [[nodiscard]] RunOneTaskResult RunOneTask(Dispatcher& dispatcher,
                                            Task* task_to_look_for);

 private:
  friend class Dispatcher;
  friend class Task;
  friend class Waker;

  /// Sends a wakeup signal to this ``Dispatcher``.
  ///
  /// This method's implementation should ensure that the ``Dispatcher`` comes
  /// back from sleep and begins invoking ``RunOneTask`` again.
  ///
  /// Note: the ``dispatcher_lock()`` may or may not be held here, so it must
  /// not be acquired by ``DoWake``, nor may ``DoWake`` assume that it has been
  /// acquired.
  virtual void DoWake() = 0;

  static void UnpostTaskList(Task*)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());
  static void RemoveTaskFromList(Task&)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());
  void RemoveWokenTaskLocked(Task&)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());
  void RemoveSleepingTaskLocked(Task&)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // For use by ``WakeTask`` and ``DispatcherImpl::Post``.
  void AddTaskToWokenList(Task&) PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // For use by ``RunOneTask``.
  void AddTaskToSleepingList(Task&)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // For use by ``Waker``.
  void WakeTask(Task&) PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // For use by ``RunOneTask``.
  Task* PopWokenTask() PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // A lock guarding ``Task`` execution.
  //
  // This will be acquired prior to pulling any tasks off of the ``Task``
  // queue, and only released after they have been run and possibly
  // destroyed.
  //
  // If acquiring this lock and ``dispatcher_lock()``, this lock must
  // be acquired first in order to avoid deadlocks.
  //
  // Acquiring this lock may be a slow process, as it must wait until
  // the running task has finished executing ``Task::Pend``.
  pw::sync::Mutex task_execution_lock_;

  Task* first_woken_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  Task* last_woken_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  // Note: the sleeping list's order is not significant.
  Task* sleeping_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  bool wants_wake_ PW_GUARDED_BY(dispatcher_lock()) = false;
};

PW_MODIFY_DIAGNOSTICS_POP();

}  // namespace pw::async2
