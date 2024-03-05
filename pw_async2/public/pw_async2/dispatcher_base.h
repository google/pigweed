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

#include <mutex>
#include <optional>

#include "pw_assert/assert.h"
#include "pw_async2/poll.h"
#include "pw_chrono/system_clock.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_toolchain/no_destructor.h"

namespace pw::async2 {

/// A lock guarding the ``Task`` queue and ``Waker`` lists.
///
/// This is an ``InterruptSpinLock`` in order to allow posting work from ISR
/// contexts.
///
/// This lock is global rather than per-dispatcher in order to allow ``Task``
/// and ``Waker`` to take out the lock without dereferencing their
/// ``Dispatcher*`` fields, which are themselves guarded by the lock in order
/// to allow the ``Dispatcher`` to ``Deregister`` itself upon destruction.
inline pw::sync::InterruptSpinLock& dispatcher_lock() {
  static NoDestructor<pw::sync::InterruptSpinLock> lock;
  return *lock;
}

class DispatcherBase;
class Waker;
class WaitReason;

// Forward-declare ``Dispatcher``.
// This concrete type must be provided by a backend.
class Dispatcher;

/// Context for an asynchronous ``Task``.
///
/// This object contains resources needed for scheduling asynchronous work,
/// such as the current ``Dispatcher`` and the ``Waker`` for the current task.
///
/// ``Context`` s are most often created by ``Dispatcher`` s, which pass them
/// into ``Task::Pend``.
class Context {
 public:
  /// Creates a new ``Context`` containing the currently-running ``Dispatcher``
  /// and a ``Waker`` for the current ``Task``.
  Context(Dispatcher& dispatcher, Waker& waker)
      : dispatcher_(&dispatcher), waker_(&waker) {}

  /// The ``Dispatcher`` on which the current ``Task`` is executing.
  ///
  /// This can be used for spawning new tasks using
  /// ``dispatcher().Post(task);``.
  Dispatcher& dispatcher() { return *dispatcher_; }

  /// Queues the current ``Task::Pend`` to run again in the future, possibly
  /// after other work is performed.
  ///
  /// This may be used by ``Task`` implementations that wish to provide
  /// additional fairness by yielding to the dispatch loop rather than perform
  /// too much work in a single iteration.
  ///
  /// This is semantically equivalent to calling ``GetWaker(...).Wake()``
  void ReEnqueue();

  /// Returns a ``Waker`` which, when awoken, will cause the current task to be
  /// ``Pend``'d by its dispatcher.
  Waker GetWaker(WaitReason reason);

 private:
  Dispatcher* dispatcher_;
  Waker* waker_;
};

/// A task which may complete one or more asynchronous operations.
///
/// The ``Task`` interface is commonly implemented by users wishing to schedule
/// work on an  asynchronous ``Dispatcher``. To do this, users may subclass
/// ``Task``, providing an implementation of the ``DoPend`` method which
/// advances the state of the ``Task`` as far as possible before yielding back
/// to the ``Dispatcher``.
///
/// This process works similarly to cooperatively-scheduled green threads or
/// coroutines, with a ``Task`` representing a single logical "thread" of
/// execution. Unlike some green thread or coroutine implementations, ``Task``
/// does not imply a separately-allocated stack: ``Task`` state is most
/// commonly stored in fields of the ``Task`` subclass.
///
/// Once defined by a user, ``Task`` s may be run by passing them to a
/// ``Dispatcher`` via ``Dispatcher::Post``. The ``Dispatcher`` will then
/// ``Pend`` the ``Task`` every time that the ``Task`` indicates it is able
/// to make progress.
///
/// Note that ``Task`` objects *must not* be destroyed while they are actively
/// being ``Pend``'d by a ``Dispatcher``. The best way to ensure this is to
/// create ``Task`` objects that continue to live until they receive a
/// ``DoDestroy`` call or which outlive their associated ``Dispatcher``.
class Task {
  friend class Waker;
  friend class DispatcherBase;
  template <typename T>
  friend class DispatcherImpl;

 public:
  Task() = default;
  Task(Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(Task&) = delete;
  Task& operator=(Task&&) = delete;
  virtual ~Task() {
    // Note: the task must not be registered with a ``Dispatcher` upon
    // destruction. This happens automatically upon ``Task`` completion or upon
    // ``Dispatcher`` destruction.
    //
    // This is necessary to ensure that neither the ``Dispatcher`` nor
    // ``Waker`` reference the ``Task`` object after destruction.
    //
    // Note that the ``~Task`` destructor cannot perform this deregistration,
    // as there is no guarantee that (1) the task is not being actively polled
    // and (2) by the time the ``~Task`` destructor is reached, the subclass
    // destructor has already run, invalidating the subclass state that may be
    // read by the ``Pend`` implementation.
  }

  // A public interface for ``DoPend``.
  //
  // ``DoPend`` is normally invoked by a ``Dispatcher`` after a ``Task`` has
  // been ``Post`` ed.
  //
  // This wrapper should only be called by ``Task`` s delegating to other
  // ``Task`` s.  For example, a ``class MainTask`` might have separate fields
  // for  ``TaskA` and ``TaskB``, and could invoke ``Pend`` on these types
  // within its own ``DoPend`` implementation.
  Poll<> Pend(Context& cx) { return DoPend(cx); }

  // A public interface for ``DoDestroy``.
  //
  // ``DoDestroy`` is normally invoked by a ``Dispatcher`` after a ``Post`` ed
  // ``Task`` has completed.
  //
  // This should only be called by ``Task`` s delegating to other ``Task`` s.
  void Destroy() { DoDestroy(); }

 private:
  /// Attempts to advance this ``Task`` to completion.
  ///
  /// This method should not perform synchronous waiting, as doing so may block
  /// the main ``Dispatcher`` loop and prevent other ``Task`` s from
  /// progressing. Because of this, ``Task`` s should not invoke blocking
  /// ``Dispatcher`` methods such as ``RunUntilComplete``.
  ///
  /// ``Task`` s should also avoid invoking ``RunUntilStalled` on their own
  /// ``Dispatcher``.
  ///
  /// Returns ``Ready`` if complete, or ``Pending`` if the ``Task`` was not yet
  /// able to complete.
  ///
  /// If ``Pending`` is returned, the ``Task`` must ensure it is woken up when
  /// it is able to make progress. To do this, ``Task::Pend`` must arrange for
  /// ``Waker::Wake`` to be called, either by storing a copy of the ``Waker``
  /// away to be awoken by another system (such as an interrupt handler).
  virtual Poll<> DoPend(Context&) = 0;

  /// Performs any necessary cleanup of ``Task`` memory after completion.
  ///
  /// This may include calls to ``std::destroy_at(this)``, and may involve
  /// deallocating the memory for this ``Task`` itself.
  ///
  /// Tasks implementations which wish to be reused may skip self-destruction
  /// here.
  virtual void DoDestroy() {}

  // Unlinks all ``Waker`` objects associated with this ``Task.``
  void RemoveAllWakersLocked() PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // Adds a ``Waker`` to the linked list of ``Waker`` s tracked by this
  // ``Task``.
  void AddWakerLocked(Waker&) PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  // Removes a ``Waker`` from the linked list of ``Waker`` s tracked by this
  // ``Task``
  //
  // Precondition: the provided waker *must* be in the list of ``Waker`` s
  // tracked by this ``Task``.
  void RemoveWakerLocked(Waker&) PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

  enum class State {
    kUnposted,
    kRunning,
    kWoken,
    kSleeping,
  };
  // The current state of the task.
  State state_ PW_GUARDED_BY(dispatcher_lock()) = State::kUnposted;

  // A pointer to the dispatcher this task is associated with.
  //
  // This will be non-null when `state_` is anything other than `kUnposted`.
  //
  // This value must be cleared by the dispatcher upon destruction in order to
  // prevent null access.
  DispatcherBase* dispatcher_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;

  // Pointers for whatever linked-list this ``Task`` is in.
  // These are controlled by the ``Dispatcher``.
  Task* prev_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  Task* next_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;

  // A pointer to the first element of the linked list of ``Waker`` s that may
  // awaken this ``Task``.
  Waker* wakers_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
};

/// An identifier indicating the kind of event a ``Waker`` is waiting for.
///
/// This identifier may be stored for debugging purposes.
class WaitReason {
 public:
  /// Indicates that the wait is happen for an unspecified reason.
  static WaitReason Unspecified() { return WaitReason(); }

 private:
  WaitReason() {}
};

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
  friend class DispatcherBase;
  template <typename T>
  friend class DispatcherImpl;

 public:
  Waker() = default;
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

  /// Creates a second ``Waker`` from this ``Waker``.
  ///
  /// ``Clone`` is made explicit in order to allow for easier tracking of
  /// the different ``Waker``s that may wake up a ``Task``.
  ///
  /// The ``WaitReason`` argument can be used to provide information about
  /// what event the ``Waker`` is waiting on. This can be useful for
  /// debugging purposes.
  ///
  /// This operation is guaranteed to be thread-safe.
  Waker Clone(WaitReason reason) & PW_LOCKS_EXCLUDED(dispatcher_lock());

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

/// A base class used by ``Dispatcher`` implementations.
///
/// Note that only one ``Dispatcher`` implementation should exist per
/// toolchain. However, a common base class is used in order to share
/// behavior and standardize the interface of these ``Dispatcher`` s,
/// and to prevent build system cycles due to ``Task`` needing to refer
/// to the ``Dispatcher`` class.
class DispatcherBase {
 public:
  DispatcherBase() = default;
  DispatcherBase(DispatcherBase&) = delete;
  DispatcherBase(DispatcherBase&&) = delete;
  DispatcherBase& operator=(DispatcherBase&) = delete;
  DispatcherBase& operator=(DispatcherBase&&) = delete;
  virtual ~DispatcherBase() {}

 protected:
  /// Check that a task is posted on this ``Dispatcher``.
  bool HasPostedTask(Task& task)
      PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock()) {
    return task.dispatcher_ == this;
  }

  /// Removes references to this ``DispatcherBase`` from all linked ``Task`` s
  /// and ``Waker`` s.
  ///
  /// This must be called by ``Dispatcher`` implementations in their
  /// destructors. It is not called by the ``DispatcherBase`` destructor, as
  /// doing so would allow the ``Dispatcher`` to be referenced between the
  /// calls to ``~Dispatcher`` and ``~DispatcherBase``.
  void Deregister() PW_LOCKS_EXCLUDED(dispatcher_lock());

 private:
  friend class Task;
  friend class Waker;
  template <typename Impl>
  friend class DispatcherImpl;

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

  Task* first_woken_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  Task* last_woken_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  // Note: the sleeping list's order is not significant.
  Task* sleeping_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  bool wants_wake_ PW_GUARDED_BY(dispatcher_lock()) = false;
};

/// Information about whether and when to sleep until as returned by
/// ``DispatcherBase::AttemptRequestWake``.
///
/// This should only be used by ``Dispatcher`` implementations.
class [[nodiscard]] SleepInfo {
  template <typename T>
  friend class DispatcherImpl;

 public:
  bool should_sleep() const { return should_sleep_; }

 private:
  SleepInfo(bool should_sleep) : should_sleep_(should_sleep) {}

  static SleepInfo DontSleep() { return SleepInfo(false); }

  static SleepInfo Indefinitely() { return SleepInfo(true); }

  bool should_sleep_;
};

/// Information about the result of a call to ``RunOneTask``.
///
/// This should only be used by ``Dispatcher`` implementations.
class [[nodiscard]] RunOneTaskResult {
  template <typename T>
  friend class DispatcherImpl;

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

/// A CRTP base class used by ``Dispatcher`` implementations.
///
/// This is used to provide a common public interface for all ``Dispatcher``
/// implementations.
///
/// Note that only one ``Dispatcher`` implementation should exist per
/// toolchain. However, a common base class is used in order to share
/// behavior and standardize the interface of these ``Dispatcher`` s,
/// and to prevent build system cycles due to ``Task`` needing to refer
/// to the ``Dispatcher`` class.
template <typename Impl>
class DispatcherImpl : public DispatcherBase {
 public:
  /// Tells the ``Dispatcher`` to run ``Task`` to completion.
  /// This method does not block.
  ///
  /// After ``Post`` is called, ``Task::Pend`` will be invoked once.
  /// If ``Task::Pend`` does not complete, the ``Dispatcher`` will wait
  /// until the ``Task`` is "awoken", at which point it will call ``Pend``
  /// again until the ``Task`` completes.
  void Post(Task& task) PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    bool wake_dispatcher = false;
    {
      std::lock_guard lock(dispatcher_lock());
      PW_DASSERT(task.state_ == Task::State::kUnposted);
      PW_DASSERT(task.dispatcher_ == nullptr);
      task.state_ = Task::State::kWoken;
      task.dispatcher_ = this;
      AddTaskToWokenList(task);
      if (wants_wake_) {
        wake_dispatcher = true;
        wants_wake_ = false;
      }
    }
    // Note: unlike in ``WakeTask``, here we know that the ``Dispatcher`` will
    // not be destroyed out from under our feet because we're in a method being
    // called on the ``Dispatcher`` by a user.
    if (wake_dispatcher) {
      DoWake();
    }
  }

  /// Runs tasks until none are able to make immediate progress.
  Poll<> RunUntilStalled() PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    return self().DoRunUntilStalled(nullptr);
  }

  /// Runs tasks until none are able to make immediate progress, or until
  /// ``task`` completes.
  ///
  /// Returns whether ``task`` completed.
  Poll<> RunUntilStalled(Task& task) PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    return self().DoRunUntilStalled(&task);
  }

  /// Runs until all tasks complete.
  void RunToCompletion() PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    self().DoRunToCompletion(nullptr);
  }

  /// Runs until ``task`` completes.
  void RunToCompletion(Task& task) PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    self().DoRunToCompletion(&task);
  }

 protected:
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
  SleepInfo AttemptRequestWake() PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    std::lock_guard lock(dispatcher_lock());
    // Don't allow sleeping if there are already tasks waiting to be run.
    if (first_woken_ != nullptr) {
      return SleepInfo::DontSleep();
    }
    /// Indicate that the ``Dispatcher`` is sleeping and will need a ``DoWake``
    /// call once more work can be done.
    wants_wake_ = true;
    // Once timers are added, this should check them.
    return SleepInfo::Indefinitely();
  }

  /// Attempts to run a single task, returning whether any tasks were
  /// run, and whether `task_to_look_for` was run.
  [[nodiscard]] RunOneTaskResult RunOneTask(Task* task_to_look_for)
      PW_LOCKS_EXCLUDED(dispatcher_lock()) {
    Task* task;
    {
      std::lock_guard lock(dispatcher_lock());
      task = PopWokenTask();
      if (task == nullptr) {
        return RunOneTaskResult(false, false, false);
      }
      task->state_ = Task::State::kRunning;
    }

    bool complete;
    {
      Waker waker(*task);
      Context context(self(), waker);
      complete = task->Pend(context).IsReady();
    }
    if (complete) {
      bool all_complete;
      {
        std::lock_guard lock(dispatcher_lock());
        switch (task->state_) {
          case Task::State::kUnposted:
          case Task::State::kSleeping:
            PW_DASSERT(false);
            PW_UNREACHABLE;
          case Task::State::kRunning:
            break;
          case Task::State::kWoken:
            RemoveWokenTaskLocked(*task);
            break;
        }
        task->state_ = Task::State::kUnposted;
        task->dispatcher_ = nullptr;
        task->RemoveAllWakersLocked();
        all_complete = first_woken_ == nullptr && sleeping_ == nullptr;
      }
      task->DoDestroy();
      return RunOneTaskResult(
          /*completed_all_tasks=*/all_complete,
          /*completed_main_task=*/task == task_to_look_for,
          /*ran_a_task=*/true);
    } else {
      std::lock_guard lock(dispatcher_lock());
      if (task->state_ == Task::State::kRunning) {
        task->state_ = Task::State::kSleeping;
        AddTaskToSleepingList(*task);
      }
      return RunOneTaskResult(
          /*completed_all_tasks=*/false,
          /*completed_main_task=*/false,
          /*ran_a_task=*/true);
    }
  }

 private:
  /// Returns ``this`` as a base class reference.
  Impl& self() { return *static_cast<Impl*>(this); }
};

}  // namespace pw::async2
