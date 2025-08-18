.. _module-pw_async2-design:

======
Design
======
``pw_async2`` is a cooperatively scheduled asynchronous framework for C++,
optimized for use in resource-constrained systems. It enables developers to
write complex, concurrent applications without the overhead of traditional
preemptive multithreading. The design prioritizes efficiency, minimal resource
usage (especially memory), and testability.

-------------
Core Concepts
-------------
The framework is built upon a few fundamental concepts that work together to
provide a powerful asynchronous runtime.

Task
====
A :doxylink:`pw::async2::Task` is the basic unit of asynchronous work. It can
be thought of as a lightweight, cooperatively scheduled "thread" or a state
machine. Users define tasks by subclassing ``pw::async2::Task`` and
implementing the ``DoPend`` virtual method. This method contains the core logic
of the task and is called by the ``Dispatcher`` to advance the task's state.

Dispatcher
==========
The :doxylink:`pw::async2::Dispatcher` is the scheduler or event loop. Its
primary role is to manage a queue of tasks and run them to completion. It polls
tasks to see if they can make progress and puts them to sleep when they are
blocked waiting for an event.

Poll
====
Asynchronous operations in ``pw_async2`` do not block. Instead, they return a
:doxylink:`pw::async2::Poll\<T> <pw::async2::Poll>` to indicate their status.

* ``Ready(T)``: The operation has completed, and the ``Poll`` object contains
  the result of type ``T``.
* ``Pending()``: The operation has not yet completed. The task should yield
  control back to the ``Dispatcher`` and arrange to be woken up later.

Context
=======
When the ``Dispatcher`` polls a ``Task``, it provides a
:doxylink:`pw::async2::Context` object. This context contains the necessary
information for the task to interact with the asynchronous runtime, most
importantly the ``Waker`` for the current task.

Waker
=====
A :doxylink:`pw::async2::Waker` is a handle that allows a suspended task to be
resumed. When a task's ``DoPend`` method returns ``Pending()``, it must have
first arranged for its ``Waker`` to be stored somewhere accessible to the event
source it is waiting on. When the event occurs (e.g., data arrives, a timer
fires), the event handler calls ``Wake()`` on the stored ``Waker``. This
notifies the ``Dispatcher`` to place the corresponding task back on the run
queue.

-------------------------------
The Pendable Function Interface
-------------------------------
Any asynchronous operation in ``pw_async2`` is exposed as a "pendable"
function. This is any function or method that can be polled for completion and
may suspend if it cannot make immediate progress. All such functions adhere to
a specific interface and a set of critical invariants.

Signature
=========
A pendable function has the general signature:

.. code-block:: cpp

   Poll<T> PendFoo(Context& cx, ...);

Where:

* ``Poll<T>`` is the return type, indicating whether the operation is
  complete (``Ready(T)``) or not (``Pending()``).

* ``Context& cx`` is a required first parameter that provides access to the
  asynchronous runtime, including the task's ``Waker``.

* ``...`` represents any additional arguments required by the operation.

Though not required, the names of these functions are conventionally prefixed
with ``Pend``.

Invariants
==========
To ensure the correct behavior of the scheduler, all pendable functions must
adhere to the following rules.

When Returning ``Pending``
--------------------------
If a pendable function returns ``Pending()``, it signals to the caller that the
operation is not yet complete and the task should be suspended. Before doing
so, it should arrange for the current task to be woken up in the future.

This is done by storing a ``Waker`` for the task from its ``Context``.
Use the :doxylink:`PW_ASYNC_STORE_WAKER` macro to copy the current task's
``Waker`` into a location accessible by the event source (e.g., an interrupt
handler, a timer manager, another task).

Failure to arrange for a wake-up before returning ``Pending()`` is a bug and
will result in a crash.

Handling Multiple Callers
~~~~~~~~~~~~~~~~~~~~~~~~~
A pendable function may be polled multiple times before the underlying
operation completes. This can happen if multiple tasks are waiting on the same
operation, or if a combinator like ``Select`` or ``Join`` re-polls an operation
that is already pending. Implementations must be prepared for this.

There are several strategies for handling wakers from multiple callers:

* **Single Waker (Assert)**: If you are certain that an operation will only
  ever have one task waiting on it at a time (common in application-specific
  code), you can use a single :doxylink:`pw::async2::Waker` and the
  :doxylink:`PW_ASYNC_STORE_WAKER` macro. This macro will crash if a second
  task attempts to store its waker before the first one has been woken, which
  can help enforce design assumptions.

* **Single Waker (Try)**: A more robust approach for single-waiter
  operations is to use :doxylink:`PW_ASYNC_TRY_STORE_WAKER`. This macro
  returns ``false`` if a waker is already stored, allowing the function to
  gracefully signal that it is busy (e.g., by returning
  ``Poll<Result<T>>(Status::Unavailable())``).

* **Multiple Wakers**: For operations that support multiple concurrent waiters,
  use a :doxylink:`pw::async2::WakerQueue`. This is a fixed-size queue that can
  store multiple wakers. When the operation completes, you can choose to wake
  the first (``WakeOne()``), a specific number (``WakeMany(n)``), or all
  (``WakeAll()``) of the waiting tasks. The same macros work with a
  ``WakerQueue``; ``PW_ASYNC_STORE_WAKER`` will crash if the queue is full,
  while ``PW_ASYNC_TRY_STORE_WAKER`` will return ``false``.

Importantly, it is always safe to call these macros with a waker from a task
that is *already* waiting on the operation. In this case, the macros will
recognize the existing waker and the call will be a no-op, preventing crashes
or erroneous "busy" states.

When Returning ``Ready``
------------------------
If a pendable function is able to complete, it should return ``Ready(value)``,
or just ``Ready()`` for ``Poll<>``.

It is up to the implementer of the pendable function to define its behavior
after returning ``Ready``. For a one-shot operation, it may be an error to poll
it again. For a stream-like operation (e.g., reading from a channel), polling
again after a ``Ready`` result is the way to receive the next value. This
behavior should be clearly documented.

---------------
Execution Model
---------------
The execution model of ``pw_async2`` revolves around the interaction between
``Task`` objects and the ``Dispatcher``.

1. **Posting**: A ``Task`` is scheduled to run by passing it to the
   :doxylink:`pw::async2::Dispatcher::Post` method. This adds the task to the
   dispatcher's queue of runnable tasks.

2. **Polling**: The ``Dispatcher`` runs its event loop (e.g., via
   ``RunToCompletion()``). It pulls a task from the queue and calls its
   ``Pend`` method, passing it a ``Context``.

3.  **Suspending**: If the task cannot complete its work (e.g., it's waiting
    for I/O), its logic must ensure the task can be re-woken before returning
    ``Pending()``. This means:

    a. A ``Waker`` for the current task must be stored where the asynchronous
       event source can access it. This storage can happen directly within the
       task's ``DoPend`` method or in a nested pendable function that
       ``DoPend`` calls.

    b. ``DoPend`` returns ``Pending()``. The ``Dispatcher`` then removes the
       task from the run queue and puts it into a sleeping state, awaiting the
       wake-up call.

4. **Waking**: When the external event completes, the event handler retrieves
   the stored ``Waker`` and calls ``Wake()`` on it. This moves the task from
   the sleeping state back into the ``Dispatcher``'s run queue.

5. **Resuming**: The ``Dispatcher``, now aware that the task can make progress,
   will eventually call its ``DoPend`` method again.

7. **Completing**: Once the task has finished all its work, ``DoPend`` returns
   ``Ready()``. The ``Dispatcher`` then considers the task complete, removes
   it permanently, and may trigger its destruction.

This cycle of polling, suspending, and waking continues until the task
completes, allowing many tasks to run concurrently on a single thread without
blocking.

-------------
Memory Model
-------------
``pw_async2`` is designed to be memory-safe and efficient, especially in
resource-constrained environments. It avoids hidden dynamic memory allocations
in its core components.

Task Lifetime and Storage
=========================
The memory for a ``Task`` object itself is managed by the user. This provides
flexibility in how tasks are allocated and stored. Common patterns include:

* **Static or Member Storage**: For tasks that live for the duration of the
  application or are part of a long-lived object, they can be allocated
  statically or as class members. This is the most common and memory-safe
  approach. The user must ensure the ``Task`` object is not destroyed while it
  is still registered with a ``Dispatcher``. Calling
  :doxylink:`pw::async2::Task::Deregister` before destruction guarantees
  safety.

* **Dynamic Allocation**: For tasks with a dynamic lifetime, ``pw_async2``
  provides the :doxylink:`pw::async2::AllocateTask` helper. This function
  allocates a task using a provided :doxylink:`pw::Allocator` and
  wraps it in a concrete ``Task`` implementation that automatically calls the
  allocator's ``Delete`` method upon completion. This simplifies memory
  management for "fire-and-forget" tasks.

.. code-block:: cpp

   // This task will be deallocated from the provided allocator when it's done.
   Task* task = AllocateTask<MyPendable>(my_allocator, arg1, arg2);
   dispatcher.Post(*task);

Coroutine Memory
================
When using C++20 coroutines, the compiler generates code to save the
coroutine's state (including local variables) across suspension points
(``co_await``). ``pw_async2`` hooks into this mechanism to control where this
state is stored.

A :doxylink:`pw::async2::CoroContext`, which holds a
:doxylink:`pw::Allocator`, must be passed to any function that
returns a :doxylink:`pw::async2::Coro`. This allocator is used to allocate the
coroutine frame. If allocation fails, the resulting ``Coro`` will be invalid
and will immediately return a ``Ready(Status::Internal())`` result when polled.
This design makes coroutine memory usage explicit and controllable.

----------------
Interoperability
----------------
``pw_async2`` is designed to integrate smoothly with existing codebases,
including those that use traditional callback-based asynchronous patterns.

Integrating with Callback-Based APIs
====================================
It's common to have a system where some parts use ``pw_async2`` and others use
callbacks. To bridge this gap, ``pw_async2`` provides helpers to wrap a
pendable function and invoke a callback with its result.

* :doxylink:`pw::async2::OneshotCallbackTask`: Polls a pendable function
  until it completes. When the function returns ``Ready(value)``, invokes a
  provided callback with the ``value`` and then finishes the task. This is
  ideal for request/response patterns.

* :doxylink:`pw::async2::RecurringCallbackTask`: This task is similar but
  reschedules itself after the callback is invoked. This allows it to handle
  pendable functions that produce a stream of values over time.

This allows non-``pw_async2`` code to initiate and receive results from
asynchronous operations without needing to be structured as a ``Task`` itself.

.. code-block:: cpp

   // A pendable function from the async part of the system.
   Poll<Result<int>> ReadSensorAsync(Context&);

   // Non-async code wants to read the sensor.
   void ReadAndPrintSensor() {
     // Create a task that will call our lambda when the sensor read is done.
     auto callback_task = OneshotCallbackTaskFor<&ReadSensorAsync>(
       [](Result<int> result) {
         if (result.ok()) {
           printf("Sensor value: %d\n", *result);
         }
       });

     // Post the task to the system's dispatcher.
     GetMainDispatcher().Post(callback_task);

     // The task must outlive the operation. Here, we might block or wait
     // on a semaphore for the callback to signal completion.
   }

Considerations for Callback-Based Integration
---------------------------------------------
While ``CallbackTask`` helpers are convenient, there are design implications
to consider:

* **Separate Tasks**: Each ``CallbackTask`` is a distinct ``Task`` from the
  perspective of the ``Dispatcher``. If a pendable function is called by both a
  "native" ``pw_async2`` task and a ``CallbackTask``, that pendable function
  must be designed to handle multiple concurrent callers (see
  `Handling Multiple Callers`_).

* **Transitional Tool**: These helpers are primarily intended as a transitional
  tool for gradually migrating a codebase to ``pw_async2``. They provide a
  quick way to bridge the two paradigms.

* **Robust Callback APIs**: If an asynchronous operation needs to expose a
  robust, primary API based on callbacks to non-``pw_async2`` parts of a
  system, a more integrated solution is recommended. Instead of using
  standalone ``CallbackTask`` objects, the core ``Task`` that manages the
  operation should natively support registering and managing a list of
  callbacks. This provides a clearer and more efficient interface for external
  consumers.

---------------
Time and Timers
---------------
Asynchronous systems often need to interact with time, for example to implement
timeouts, delays, or periodic tasks. ``pw_async2`` provides a flexible and
testable mechanism for this through the :doxylink:`pw::async2::TimeProvider`
interface.

``TimeProvider``
================
The :doxylink:`pw::async2::TimeProvider` is an abstract interface that acts as
a factory for timers. Its key responsibilities are:

* **Providing the current time**: The ``now()`` method returns the current
  time according to a specific clock.
* **Creating timers**: The ``WaitUntil(timestamp)`` and ``WaitFor(delay)``
  methods return a :doxylink:`pw::async2::TimeFuture` object.

This design is friendly to dependency injection. By providing different
implementations of ``TimeProvider``, code that uses timers can be tested with a
simulated clock (like ``pw::chrono::SimulatedClock``), allowing for
fast and deterministic tests without real-world delays. For production code,
the :doxylink:`pw::async2::GetSystemTimeProvider` function returns a global
``TimeProvider`` that uses the configured system clock.

``TimeFuture``
==============
A :doxylink:`pw::async2::TimeFuture` is a pendable object that completes at a
specific time. A task can ``Pend`` on a ``TimeFuture`` to suspend itself until
the time designated by the future. When the time is reached, the
``TimeProvider`` wakes the task, and its next poll of the ``TimeFuture`` will
return ``Ready(timestamp)``.

Example
=======
Here is an example of a task that logs a message, sleeps for one second, and
then logs another message.

.. code-block:: cpp

   #include "pw_async2/dispatcher.h"
   #include "pw_async2/system_time_provider.h"
   #include "pw_async2/task.h"
   #include "pw_chrono/system_clock.h"
   #include "pw_log/log.h"

   using namespace std::chrono_literals;

   class LoggingTask : public pw::async2::Task {
    public:
     LoggingTask() : state_(State::kLogFirstMessage) {}

    private:
     enum class State {
       kLogFirstMessage,
       kSleeping,
       kLogSecondMessage,
       kDone,
     };

     Poll<> DoPend(Context& cx) override {
       while (true) {
         switch (state_) {
           case State::kLogFirstMessage:
             PW_LOG_INFO("Hello, async world!");
             future_ = GetSystemTimeProvider().WaitFor(1s);
             state_ = State::kSleeping;
             continue;

           case State::kSleeping:
             if (future_.Pend(cx).IsPending()) {
               return Pending();
             }
             state_ = State::kLogSecondMessage;
             continue;

           case State::kLogSecondMessage:
             PW_LOG_INFO("Goodbye, async world!");
             state_ = State::kDone;
             continue;

           case State::kDone:
             return Ready();
         }
       }
     }

     State state_;
     pw::async2::TimeFuture<pw::chrono::SystemClock> future_;
   };

------------------------
Primitives and Utilities
------------------------
On top of these core concepts, ``pw_async2`` provides a suite of higher-level
primitives to make asynchronous programming easier and more expressive.

Coroutines (``Coro<T>``)
========================
For projects using C++20, ``pw_async2`` provides first-class support for
coroutines via :doxylink:`pw::async2::Coro`. This allows you to write
asynchronous logic in a sequential, synchronous style, eliminating the need to
write explicit state machines. The ``co_await`` keyword is used to suspend
execution until an asynchronous operation is ``Ready``.

.. code-block:: cpp

   Coro<Status> ReadAndSend(Reader& reader, Writer& writer) {
     // co_await suspends the coroutine until the Read operation completes.
     Result<Data> data = co_await reader.Read();
     if (!data.ok()) {
       co_return data.status();
     }

     // The coroutine resumes here and continues.
     co_await writer.Write(*data);
     co_return OkStatus();
   }

Data Passing (``OnceSender`` / ``OnceReceiver``)
================================================
This pair of types provides a simple, single-use channel for passing a value
from one task to another. The receiving task pends on the
:doxylink:`pw::async2::OnceReceiver` until the producing task sends a value
through the :doxylink:`pw::async2::OnceSender`.

Combinators (``Join`` and ``Select``)
=====================================
These powerful utilities allow for the composition of multiple asynchronous
operations:

* :doxylink:`pw::async2::Join`: Waits for *all* of a set of pendable
  operations to complete.

* :doxylink:`pw::async2::Select`: Waits for the *first* of a set of pendable
  operations to complete, returning its result.

Poll aliases
============
Fallible pendable functions often return ``Poll<pw::Result<T>>`` or
``Poll<std::optional<T>>``. The :doxylink:`pw::async2::PollResult` and
:doxylink:`pw::async2::PollOptional` aliases are provided to simplify these
cases.
