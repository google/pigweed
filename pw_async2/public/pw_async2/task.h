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
#include "pw_async2/poll.h"
#include "pw_sync/lock_annotations.h"

namespace pw::async2 {

class NativeDispatcherBase;

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
  Task(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(const Task&) = delete;
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

}  // namespace pw::async2
