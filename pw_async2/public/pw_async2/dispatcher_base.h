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

#include "pw_async2/poll.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
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

class NativeDispatcherBase;
class Waker;

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
  /// This is semantically equivalent to calling:
  ///
  /// ```
  /// Waker waker;
  /// PW_ASYNC_STORE_WAKER(cx, waker, ...);
  /// std::move(waker).Wake();
  /// ```
  void ReEnqueue();

  /// INTERNAL-ONLY: users should use the `PW_ASYNC_STORE_WAKER` macro instead.
  ///
  /// Saves a ``Waker`` into ``waker_out`` which, when awoken, will cause the
  /// current task to be ``Pend``'d by its dispatcher.
  void InternalStoreWaker(Waker& waker_out);

 private:
  Dispatcher* dispatcher_;
  Waker* waker_;
};

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

template <typename T>
using PendOutputOf = typename decltype(std::declval<T>().Pend(
    std::declval<Context&>()))::OutputType;

template <typename, typename = void>
constexpr bool is_pendable = false;

template <typename T>
constexpr bool is_pendable<T, std::void_t<PendOutputOf<T>>> = true;

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
/// being ``Pend``'d by a ``Dispatcher``. To protect against this, be sure to
/// do one of the following:
///
/// - Use dynamic lifetimes by creating ``Task`` objects that continue to live
///   until they receive a ``DoDestroy`` call.
/// - Create ``Task`` objects whose stack-based lifetimes outlive their
///   associated ``Dispatcher``.
/// - Call ``Deregister`` on the ``Task`` prior to its destruction. NOTE that
///   ``Deregister`` may not be called from inside the ``Task``'s own ``Pend``
///   method.
class Task {
  friend class Dispatcher;
  friend class Waker;
  friend class NativeDispatcherBase;

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

  /// A public interface for ``DoPend``.
  ///
  /// ``DoPend`` is normally invoked by a ``Dispatcher`` after a ``Task`` has
  /// been ``Post`` ed.
  ///
  /// This wrapper should only be called by ``Task`` s delegating to other
  /// ``Task`` s.  For example, a ``class MainTask`` might have separate fields
  /// for  ``TaskA`` and ``TaskB``, and could invoke ``Pend`` on these types
  /// within its own ``DoPend`` implementation.
  Poll<> Pend(Context& cx) { return DoPend(cx); }

  /// Whether or not the ``Task`` is registered with a ``Dispatcher``.
  ///
  /// Returns ``true`` after this ``Task`` is passed to ``Dispatcher::Post``
  /// until one of the following occurs:
  ///
  /// - This ``Task`` returns ``Ready`` from its ``Pend`` method.
  /// - ``Task::Deregister`` is called.
  /// - The associated ``Dispatcher`` is destroyed.
  bool IsRegistered() const;

  /// Deregisters this ``Task`` from the linked ``Dispatcher`` and any
  /// associated ``Waker`` values.
  ///
  /// This must not be invoked from inside this task's ``Pend`` function, as
  /// this will result in a deadlock.
  ///
  /// NOTE: If this task's ``Pend`` method is currently being run on the
  /// dispatcher, this method will block until ``Pend`` completes.
  ///
  /// NOTE: This method sadly cannot guard against the dispatcher itself being
  /// destroyed, so this method must not be called concurrently with
  /// destruction of the dispatcher associated with this ``Task``.
  ///
  /// Note that this will *not* destroy the underlying ``Task``.
  void Deregister();

  /// A public interface for ``DoDestroy``.
  ///
  /// ``DoDestroy`` is normally invoked by a ``Dispatcher`` after a ``Post`` ed
  /// ``Task`` has completed.
  ///
  /// This should only be called by ``Task`` s delegating to other ``Task`` s.
  void Destroy() { DoDestroy(); }

 private:
  /// Attempts to deregister this task.
  ///
  /// If the task is currently running, this will return false and the task
  /// will not be deregistered.
  bool TryDeregister() PW_EXCLUSIVE_LOCKS_REQUIRED(dispatcher_lock());

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
  NativeDispatcherBase* dispatcher_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;

  // Pointers for whatever linked-list this ``Task`` is in.
  // These are controlled by the ``Dispatcher``.
  Task* prev_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
  Task* next_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;

  // A pointer to the first element of the linked list of ``Waker`` s that may
  // awaken this ``Task``.
  Waker* wakers_ PW_GUARDED_BY(dispatcher_lock()) = nullptr;
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
