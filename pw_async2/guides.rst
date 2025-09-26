.. _module-pw_async2-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_async2

.. _module-pw_async2-guides-pendable-function:

-------------------------------
The pendable function interface
-------------------------------
Any asynchronous operation in ``pw_async2`` is exposed as a "pendable"
function. This is any function or method that can be polled for completion and
may suspend if it cannot make immediate progress. All such functions adhere to
a specific interface and a set of critical invariants.

.. _module-pw_async2-guides-pendable-function-signature:

Signature
=========
A pendable function has the general signature:

.. code-block:: cpp

   Poll<T> PendFoo(Context& cx, ...);

Where:

* ``Poll<T>`` is the return type, indicating whether the operation is complete
  (``Ready(T)``) or not (``Pending()``). For pendable functions that do not
  produce a value upon completion, use ``Poll<>``, returning ``Ready()`` when
  complete.

* ``Context& cx`` is a required first parameter that provides access to the
  asynchronous runtime, including the task's ``Waker``.

* ``...`` represents any additional arguments required by the operation.

For pendable functions you write, while it is not required, it
is strongly recommended to prefix the name of the function with ``Pend``.
This prefix is consistently used by Pigweed to denote a pendable
function.

.. _module-pw_async2-guides-pendable-function-invariants:

Invariants
==========
.. _invariants: https://stackoverflow.com/a/112088

To ensure the correct behavior of the scheduler, all pendable functions must
always uphold the following `invariants`_:

* :ref:`module-pw_async2-guides-pendables-incomplete`
* :ref:`module-pw_async2-guides-pendables-complete`

.. _module-pw_async2-guides-pendables-incomplete:

Arranging future completion of incomplete tasks
-----------------------------------------------
When a pendable function can't yet complete:

#. Do one of the following to make sure the task rewakes when it's ready to
   make more progress:

   * Delegate waking to a subtask. Arrange for that subtask's
     pendable function to wake this task when appropriate.

   * Arrange an external wakeup. Use :c:macro:`PW_ASYNC_STORE_WAKER`
     to store the task's waker somewhere, and then call
     :cc:`Wake <pw::async2::Waker::Wake>` from an interrupt or another
     thread once the event that the task is waiting for has completed.

   * Re-enqueue the task with :cc:`ReEnqueue
     <pw::async2::Context::ReEnqueue>`. This is a rare case. Usually, you
     should just create an immediately invoked ``Waker``.

#. Make sure to return :cc:`Pending <pw::async2::Pending>` to signal that
   the task is incomplete.

In other words, whenever your pendable function returns
:cc:`Pending <pw::async2::Pending>`, you must guarantee that ``Wake()``
is called once in the future.

For example, one implementation of a delayed task might arrange for a timer to
wake its ``Waker`` once some time has passed. Another case might be a messaging
library which calls ``Wake()`` on the receiving task once a sender has placed a
message in a queue.

Failure to arrange for a wake-up before returning ``Pending()`` is a bug and
will result in a crash.

.. _module-pw_async2-guides-pendable-function-invariants-multiple-callers:

Handling multiple callers
-------------------------
A pendable function may be polled multiple times before the underlying
operation completes. This can happen if multiple tasks are waiting on the same
operation, or if a combinator like ``Select`` or ``Join`` re-polls an operation
that is already pending. Implementations must be prepared for this.

There are several strategies for handling wakers from multiple callers:

* **Single Waker (Assert)**: If you are certain that an operation will only
  ever have one task waiting on it at a time (common in application-specific
  code), you can use a single :cc:`Waker <pw::async2::Waker>` and the
  :cc:`PW_ASYNC_STORE_WAKER` macro. This macro will crash if a second
  task attempts to store its waker before the first one has been woken, which
  can help enforce design assumptions.

* **Single Waker (Try)**: A more robust approach for single-waiter
  operations is to use :cc:`PW_ASYNC_TRY_STORE_WAKER`. This macro
  returns ``false`` if a waker is already stored, allowing the function to
  gracefully signal that it is busy (e.g., by returning
  ``PollResult<T>(Status::Unavailable())``).

* **Multiple Wakers**: For operations that support multiple concurrent waiters,
  use a :cc:`WakerQueue <pw::async2::WakerQueue>`. This is a fixed-size
  queue that can store multiple wakers. When the operation completes, you can
  choose to wake the first (``WakeOne()``), a specific number (``WakeMany(n)``),
  or all (``WakeAll()``) of the waiting tasks. The same macros work with a
  ``WakerQueue``; ``PW_ASYNC_STORE_WAKER`` will crash if the queue is full,
  while ``PW_ASYNC_TRY_STORE_WAKER`` will return ``false``.

Importantly, it is always safe to call these macros with a waker from a task
that is *already* waiting on the operation. In this case, the macros will
recognize the existing waker and the call will be a no-op, preventing crashes
or erroneous "busy" states.

.. _module-pw_async2-guides-pendables-complete:

Cleaning up complete tasks
--------------------------
If a pendable function is able to complete, it should return ``Ready(value)``,
or just ``Ready()`` for ``Poll<>``.

It is up to the implementer of the pendable function to define its behavior
after returning ``Ready``. For a one-shot operation, it may be an error to poll
it again. For a stream-like operation (e.g. reading from a channel), polling
again after a ``Ready`` result is the way to receive the next value. This
behavior should be clearly documented.

.. _module-pw_async2-guides-implementing-tasks:

------------------
Implementing tasks
------------------
:cc:`Task <pw::async2::Task>` instances complete one or more asynchronous
operations. They are the top-level "thread" primitives of ``pw_async2``.

You can use one of the concrete subclasses of ``Task`` that Pigweed provides:

* :cc:`CoroOrElseTask <pw::async2::CoroOrElseTask>`: Delegates to a
  provided coroutine and executes an ``or_else`` handler function on failure.
* :cc:`PendFuncTask <pw::async2::PendFuncTask>`: Delegates to a provided
  function.
* :cc:`PendableAsTask <pw::async2::PendableAsTask>`: Delegates to a type
  with a ``Pend`` method.
* :cc:`AllocateTask <pw::async2::AllocateTask>`: Creates a concrete
  subclass of ``Task``, just like ``PendableAsTask``, but the created task is
  dynamically allocated and frees the associated memory upon completion.

Or you can subclass ``Task`` yourself. See :cc:`Task <pw::async2::Task>`
for more guidance on subclassing.

.. _module-pw_async2-guides-tasks:

------------------------------
How a dispatcher manages tasks
------------------------------
The purpose of a :cc:`Dispatcher <pw::async2::Dispatcher>` is to keep
track of a set of :cc:`Task <pw::async2::Task>` objects and run them to
completion. The dispatcher is essentially a scheduler for cooperatively
scheduled (non-preemptive) threads (tasks).

While a dispatcher is running, it waits for one or more tasks to waken and then
advances each task by invoking its :cc:`DoPend <pw::async2::Task::DoPend>`
method. The ``DoPend`` method is typically implemented manually by users, though
it is automatically provided by coroutines.

If the task is able to complete, ``DoPend`` will return ``Ready``, in which
case the dispatcher will deregister the task.

If the task is unable to complete, ``DoPend`` must return ``Pending`` and
arrange for the task to be woken up when it is able to make progress again.
Once the task is rewoken, the task is re-added to the ``Dispatcher`` queue. The
dispatcher will then invoke ``DoPend`` once more, continuing the cycle until
``DoPend`` returns ``Ready`` and the task is completed.

The following sequence diagram summarizes the basic workflow:

.. mermaid::

   sequenceDiagram
       participant e as External Event e.g. Interrupt
       participant d as Dispatcher
       participant t as Task
       e->>t: Init Task
       e->>d: Register task via Dispatcher::Post(Task)
       d->>d: Add task to queue
       d->>t: Run task via Task::DoPend()
       t->>t: Task is waiting for data and can't yet complete
       t->>e: Arrange for rewake via PW_ASYNC_STORE_WAKER
       t->>d: Indicate that task is not complete via Pending()
       d->>d: Remove task from queue
       d->>d: Go to sleep because task queue is empty
       e->>e: The data that the task needs has arrived
       e->>d: Rewake via Waker::Wake()
       d->>d: Re-add task to queue
       d->>t: Run task via Task::DoPend()
       t->>t: Task runs to completion
       t->>d: Indicate that task is complete via Ready()
       d->>d: Deregister the task

.. _module-pw_async2-guides-passing-data:

--------------------------
Passing data between tasks
--------------------------
Astute readers will have noticed that the ``Wake`` method takes zero arguments,
and ``DoPoll`` does not provide the task it polls with any values!

Unlike callback-based interfaces, tasks (and the libraries they use)
are responsible for storage of their inputs and outputs, and you are responsible
for defining how that happens.

It is also important to ensure that the receiver puts itself to sleep correctly
when the it is waiting for a value. The sender likewise must wait for available
storage before it sends a value.

There are two patterns for doing this, depending on how much data you want to
send.

.. _module-pw_async2-guides-passing-single-values:

Single values
=============
This pattern is for when you have a task that receives a single, one-time value.
Once the value is received, that value is immutable. The task can either go on
to waiting for something else, or can complete.

In this pattern, the task would typically hold storage for the value, and you
would implement some custom interface to set the value once one is available.
Setting a value would additionally set some internal flag indicating a value was
set, and also arrange to wake the task via its ``Waker``.

``pw_async2`` provides helpers for this pattern which add a useful layer of
abstraction and ensure you implement it correctly.

For the first pair of helpers, the receiver helper
:cc:`OnceReceiver <pw::async2::OnceReceiver>` owns the storage for a value
and is linked on creation to a :cc:`OnceSender <pw::async2::OnceSender>`
which provides the interface for sending a value.

Construct a linked pair of these helpers by calling
:cc:`MakeOnceSenderAndReceiver<T> <pw::async2::MakeOnceSenderAndReceiver>`,
using value type as the template argument.

You would then typically transfer ownership of the receiver to your Task using
``std::move``. Note that as a side-effect, the move modifies the linked sender
to point at the new location of the receiver, maintaining the link.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-construction]
   :end-before: [pw_async2-examples-once-send-recv-construction]

You can also similarly and safely move the sender to transfer its ownership.

When implementing the task, you should prefer to use
:cc:`PW_TRY_READY_ASSIGN` to automate handling the
:cc:`Pending <pw::async2::Pending>` return value of
:cc:`OnceReceiver::Pend() <pw::async2::OnceReceiver::Pend>`, leaving you
to decide how to handle the error case if no value being sent (which can happen
if the sender is destroyed), and more typically what to do with the received
value.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-receiving]
   :end-before: [pw_async2-examples-once-send-recv-receiving]

You can send a value to the task via the
:cc:`emplace() <pw::async2::OnceSender::emplace>` member function. Note
that this allows you to ``std::move`` the value, or even construct it in-place
if that makes sense to do.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-send-value]
   :end-before: [pw_async2-examples-once-send-recv-send-value]

If your type is expensive to copy, ``pw_async2`` also provides another pair of
helpers, :cc:`OnceRefSender <pw::async2::OnceRefSender>` and
:cc:`OnceRefReceiver <pw::async2::OnceRefReceiver>`, where an external
component owns the storage for the type ``T``.

For :cc:`OnceRefReceiver <pw::async2::OnceRefReceiver>`, the receiving
task will still use :cc:`Pend() <pw::async2::OnceRefReceiver::Pend>` to
get a ``Poll<Status>`` value indicating if the sender has set the value, but it
will have to access the value itself through its own pointer or reference.

When the sender uses :cc:`OnceRefSender <pw::async2::OnceRefSender>` to
set the value, do note that it does require making a copy into the external
value storage, though this can be a shallow copy through a ``std::move`` if
supported.

You can find a complete example showing how to use these helpers in
:cs:`pw_async2/examples/once_send_recv_test.cc`, and you can try it for
yourself with:

.. code-block:: sh

   bazelisk run --config=cxx20 //pw_async2/examples:once_send_recv_test

.. _module-pw_async2-guides-passing-single-values-other:

Other primitives
----------------
More primitives (such as ``MultiSender`` and ``MultiReceiver``) are
in-progress. We encourage users who need further async primitives to contribute
them upstream to ``pw::async2``!

.. _module-pw_async2-guides-passing-multiple-values:

Multiple values
===============
If your tasks need to send or receive multiple values, then you can use the
awaitable interface of :cc:`pw::InlineAsyncQueue` or
:cc:`pw::InlineAsyncDeque` from ``pw_containers``.

If your needs are simple, this is a perfectly good way of handling a simple
byte stream or message queue.

.. topic:: ``pw_channel``

   If complex data streams or message handling is a key component of your task,
   consider using :ref:`pw_channel <module-pw_channel>` instead as it
   implements zero-copy abstractions.

Both queue container types expose two key functions to allow interoperability
with ``pw_async2``:

.. list-table:: Pendable queue interface

   *  - ``PendHasSpace``
      - Allows waiting until the queue has space for more values.
   *  - ``PendNotEmpty``
      - Allows waiting until the queue has values.

For sending, the producing task has to wait for there to be space before trying
to add to the queue.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-pend-space]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-pend-space]

Receiving values is similar. The receiving task has to wait for there to be
values before trying to remove them from the queue.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-pend-values]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-pend-values]

You can find a complete example for using
:cc:`InlineAsyncQueue <pw::InlineAsyncQueue>` this way in
:cs:`pw_async2/examples/inline_async_queue_with_tasks_test.cc`, and you can try
it for yourself with:

.. code-block:: sh

   bazelisk run //pw_async2/examples:inline_async_queue_with_tasks_test

.. _module-pw_async2-guides-timing:

------
Timing
------
When using ``pw_async2``, you should inject timing functionality by accepting
a :cc:`TimeProvider <pw::async2::TimeProvider>` (most commonly
``TimeProvider<SystemClock>`` when using the system's built-in ``time_point``
and ``duration`` types).

:cc:`TimeProvider <pw::async2::TimeProvider>` allows for easily waiting
for a timeout or deadline using the
:cc:`WaitFor <pw::async2::TimeProvider::WaitFor>` and
:cc:`WaitUntil <pw::async2::TimeProvider::WaitUntil>` methods.
Additionally, you can test code that uses
:cc:`TimeProvider <pw::async2::TimeProvider>` for timing with simulated
time using
:cc:`SimulatedTimeProvider <pw::async2::SimulatedTimeProvider>`. Doing so
helps avoid timing-dependent test flakes and helps ensure that tests are fast
since they don't need to wait for real-world time to elapse.

.. _module-pw_async2-guides-callbacks:

------------------------------------------------------------
Interacting with async2 from non-async2 code using callbacks
------------------------------------------------------------
In a system gradually or partially adopting ``pw_async2``, there are often
cases where non-async2 code needs to run asynchronous operations built with
``pw_async2``.

To facilitate this, ``pw_async2`` provides callback tasks:
:cc:`OneshotCallbackTask <pw::async2::OneshotCallbackTask>` and
:cc:`RecurringCallbackTask <pw::async2::RecurringCallbackTask>`.

These tasks invoke a :ref:`pendable function
<module-pw_async2-guides-pendable-function>`, forwarding its result to a provided
callback on completion.

The two variants of callback tasks are:

* :cc:`OneshotCallbackTask\<T\> <pw::async2::OneshotCallbackTask>`: Pends
  the pendable. When the pendable returns ``Ready(value)``, the task invokes
  the callback once with ``value``. After the callback finishes, the
  ``OneshotCallbackTask`` itself completes. This is useful for single,
  asynchronous requests.

* :cc:`RecurringCallbackTask\<T\> <pw::async2::RecurringCallbackTask>`:
  Similar to the oneshot version, but after the task invokes the callback, the
  ``RecurringCallbackTask`` continues polling the pendable function. This is
  suitable for operations that produce a stream of values over time, where you
  want to process each one.

Example
=======
.. code-block:: cpp

   #include "pw_async2/callback_task.h"
   #include "pw_log/log.h"
   #include "pw_result/result.h"

   // Assume the async2 part of the system exposes a function to post tasks to
   // its dispatcher.
   void PostTaskToDispatcher(pw::async2::Task& task);

   // The async2 function we'd like to call.
   pw::async2::Poll<pw::Result<int>> ReadValue(pw::async2::Context&);

   // Non-async2 code.
   int ReadAndPrintAsyncValue() {
     pw::async2::OneshotCallbackTaskFor<&ReadValue> task(
         [](pw::Result<int> result) {
           if (result.ok()) {
             PW_LOG_INFO("Read value: %d", result.value());
           } else {
             PW_LOG_ERROR("Failed to read value: %s", result.status().str());
           }
         });

     PostTaskToDispatcher(task);

     // In this example, the code allocates the task on the stack, so we would
     // need to wait for it to complete before it goes out of scope. In a real
     // application, the task may be a member of a long-lived object, or you
     // might choose to statically allocate it.
   }

.. _module-pw_async2-guides-interrupts:

-------------------------
Interacting with hardware
-------------------------
A common use case for ``pw_async2`` is interacting with hardware that uses
interrupts. The following example demonstrates this by creating a fake UART
device with an asynchronous reading interface and a separate thread that
simulates hardware interrupts.

The example can be built and run in upstream Pigweed with the
following command:

.. code-block:: sh

   bazelisk run //pw_async2/examples:interrupt

``FakeUart`` simulates an interrupt-driven UART with an asynchronous interface
for reading bytes (``ReadByte``). The ``HandleReceiveInterrupt`` method would
be called from an ISR. (In the example, this is simulated via keyboard input.)

.. literalinclude:: examples/interrupt.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-interrupt-uart]
   :end-before: [pw_async2-examples-interrupt-uart]

A reader task polls the fake UART until it receives data.

.. literalinclude:: examples/interrupt.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-interrupt-reader]
   :end-before: [pw_async2-examples-interrupt-reader]

This example shows how to bridge the gap between low-level, interrupt-driven
hardware and the high-level, cooperative multitasking model of ``pw_async2``.

------------
Unit testing
------------
Unit testing ``pw_async2`` code is different from testing non-async code. You
must run async code from a :cc:`Task <pw::async2::Task>` on a
:cc:`Dispatcher <pw::async2::Dispatcher>`.

To test ``pw_async2`` code:

#. Declare a dispatcher.
#. Create a task to run the async code under test. Either implement
   :cc:`Task <pw::async2::Task>` or use
   :cc:`PendFuncTask <pw::async2::PendFuncTask>` to wrap a lambda.
#. Post the task to the dispatcher.
#. Call :cc:`RunUntilStalled <pw::async2::Dispatcher::RunUntilStalled>`
   to execute the task.

The following example shows the basic structure of a ``pw_async2`` unit test.

.. literalinclude:: examples/unit_test.cc
   :language: c++
   :start-after: pw_async2-minimal-test
   :end-before: pw_async2-minimal-test

It is usually necessary to run the test task multiple times to advance async
code through its states. This improves coverage and ensures that wakers are
stored and woken properly.

To run the test task multiple times:

#. Post the task to the dispatcher.
#. Call :cc:`RunUntilStalled() <pw::async2::Dispatcher::RunUntilStalled>`,
   which returns :cc:`Pending <pw::async2::Pending>`.
#. Perform actions to allow the task to advance.
#. Call :cc:`RunUntilStalled() <pw::async2::Dispatcher::RunUntilStalled>`
   again.
#. Repeat until the task runs to completion and :cc:`RunUntilStalled()
   <pw::async2::Dispatcher::RunUntilStalled>` returns
   :cc:`Ready <pw::async2::Ready>`.

The example below runs a task multiple times to test waiting for a
``FortuneTeller`` class to produce a fortune.

.. literalinclude:: examples/unit_test.cc
   :language: c++
   :start-after: pw_async2-multi-step-test
   :end-before: pw_async2-multi-step-test

.. _module-pw_async2-guides-debugging:

---------
Debugging
---------
You can inspect tasks registered to a dispatcher by calling
::cc:`Dispatcher::LogRegisteredTasks()
<pw::async2::Dispatcher::LogRegisteredTasks>`, which logs information for each
task in the dispatcher's pending and sleeping queues.

Sleeping tasks will log information about their assigned wakers, with the
wait reason provided for each.

If space is a concern, you can set the module configuration option
:cc:`PW_ASYNC2_DEBUG_WAIT_REASON` to ``0`` to disable wait reason storage
and logging. Under this configuration, the dispatcher only logs the waker count
of a sleeping task.

.. _module-pw_async2-guides-memory-model:

-------------
Memory model
-------------
``pw_async2`` is designed to be memory-safe and efficient, especially in
resource-constrained environments. It avoids hidden dynamic memory allocations
in its core components.

.. _module-pw_async2-guides-memory-model-tasks:

Task lifetime and storage
=========================
The memory for a ``Task`` object itself is managed by the user. This provides
flexibility in how tasks are allocated and stored. Common patterns include:

* **Static or Member Storage**: For tasks that live for the duration of the
  application or are part of a long-lived object, they can be allocated
  statically or as class members. This is the most common and memory-safe
  approach. The user must ensure the ``Task`` object is not destroyed while it
  is still registered with a ``Dispatcher``. Calling
  :cc:`Task::Deregister() <pw::async2::Task::Deregister>` before
  destruction guarantees safety.

* **Dynamic Allocation**: For tasks with a dynamic lifetime, ``pw_async2``
  provides the :cc:`AllocateTask <pw::async2::AllocateTask>` helper. This
  function allocates a task using a provided :cc:`pw::Allocator` and
  wraps it in a concrete ``Task`` implementation that automatically calls the
  allocator's ``Delete`` method upon completion. This simplifies memory
  management for "fire-and-forget" tasks.

.. code-block:: cpp

   // This task will be deallocated from the provided allocator when it's done.
   Task* task = AllocateTask<MyPendable>(my_allocator, arg1, arg2);
   dispatcher.Post(*task);

.. _module-pw_async2-guides-interop:

----------------
Interoperability
----------------
``pw_async2`` is designed to integrate smoothly with existing codebases,
including those that use traditional callback-based asynchronous patterns.

.. _module-pw_async2-guides-interop-callbacks:

Integrating with callback-based APIs
====================================
It's common to have a system where some parts use ``pw_async2`` and others use
callbacks. To bridge this gap, ``pw_async2`` provides helpers to wrap a
pendable function and invoke a callback with its result.

* :cc:`OneShotCallbackTask <pw::async2::OneshotCallbackTask>`: Polls a
  pendable function until it completes. When the function returns
  ``Ready(value)``, invokes a provided callback with the ``value`` and then
  finishes the task. This is ideal for request/response patterns.

* :cc:`RecurringCallbackTask <pw::async2::RecurringCallbackTask>`: This
  task is similar but reschedules itself after the callback is invoked. This
  allows it to handle pendable functions that produce a stream of values over
  time.

This allows non-``pw_async2`` code to initiate and receive results from
asynchronous operations without needing to be structured as a ``Task`` itself.

.. code-block:: cpp

   // A pendable function from the async part of the system.
   Poll<Result<int>> ReadSensorAsync(Context&);

   // Non-async code wants to read the sensor.
   void ReadAndPrintSensor() {
     // Create a task that will call our lambda when the sensor read is done.
     auto callback_task =
         OneshotCallbackTaskFor<&ReadSensorAsync>([](Result<int> result) {
           if (result.ok()) {
             printf("Sensor value: %d\n", *result);
           }
         });

     // Post the task to the system's dispatcher.
     GetMainDispatcher().Post(callback_task);

     // The task must outlive the operation. Here, we might block or wait
     // on a semaphore for the callback to signal completion.
   }

.. _module-pw_async2-guides-interop-callbacks-considerations:

Considerations for callback-based integration
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

.. _module-pw_async2-guides-time-and-timers:

---------------
Time and timers
---------------
Asynchronous systems often need to interact with time, for example to implement
timeouts, delays, or periodic tasks. ``pw_async2`` provides a flexible and
testable mechanism for this through the :cc:`TimeProvider
<pw::async2::TimeProvider>` interface.

.. _module-pw_async2-guides-time-and-timers-time-provider:

TimeProvider, timer factory
===========================
The :cc:`TimeProvider <pw::async2::TimeProvider>` is an abstract
interface that acts as a factory for timers. Its key responsibilities are:

* **Providing the current time**: The ``now()`` method returns the current
  time according to a specific clock.
* **Creating timers**: The ``WaitUntil(timestamp)`` and ``WaitFor(delay)``
  methods return a :cc:`TimeFuture <pw::async2::TimeFuture>` object.

This design is friendly to dependency injection. By providing different
implementations of ``TimeProvider``, code that uses timers can be tested with a
simulated clock (like ``pw::chrono::SimulatedClock``), allowing for fast and
deterministic tests without real-world delays. For production code, the
:cc:`GetSystemTimeProvider() <pw::async2::GetSystemTimeProvider>`
function returns a global ``TimeProvider`` that uses the configured system
clock.

.. _module-pw_async2-guides-time-and-timers-time-future:

TimeFuture, time-bound pendable objects
=======================================
A :cc:`TimeFuture <pw::async2::TimeFuture>` is a pendable object that
completes at a specific time. A task can ``Pend`` on a ``TimeFuture`` to
suspend itself until the time designated by the future. When the time is
reached, the ``TimeProvider`` wakes the task, and its next poll of the
``TimeFuture`` will return ``Ready(timestamp)``.

.. _module-pw_async2-guides-time-and-timers-example:

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

.. _module-pw_async2-guides-primitives:

------------------------
Primitives and utilities
------------------------
On top of these core concepts, ``pw_async2`` provides a suite of higher-level
primitives to make asynchronous programming easier and more expressive.

.. _module-pw_async2-guides-primitives-data-passing:

Data Passing with OnceSender and OnceReceiver
=============================================
This pair of types provides a simple, single-use channel for passing a value
from one task to another. The receiving task pends on the
:cc:`OnceReceiver <pw::async2::OnceReceiver>` until the producing task
sends a value through the :cc:`OnceSender <pw::async2::OnceSender>`.

.. _module-pw_async2-guides-primitives-combinators:

Combinators (Join and Select)
=============================
These powerful utilities allow for the composition of multiple asynchronous
operations:

* :cc:`Join <pw::async2::Join>`: Waits for *all* of a set of pendable
  operations to complete.

* :cc:`Select <pw::async2::Select>`: Waits for the *first* of a set of
  pendable operations to complete, returning its result.

.. _module-pw_async2-guides-primitives-aliases:

Poll aliases
============
Fallible pendable functions often return ``Poll<pw::Result<T>>`` or
``Poll<std::optional<T>>``. The :cc:`PollResult <pw::async2::PollResult>`
and :cc:`PollOptional <pw::async2::PollOptional>` aliases are provided to
simplify these cases.

.. _module-pw_async2-guides-primitives-wakers:

Setting up wakers
=================
You can set up a waker to a non-empty value using one of four macros we provide:

- :cc:`PW_ASYNC_STORE_WAKER` and :cc:`PW_ASYNC_CLONE_WAKER`

  The first creates a waker for a given context. The second clones an
  existing waker, allowing the original and/or the clone to wake the task.

  This pair of macros ensure a single task will be woken. They will assert if
  a waker for a different task is created (or cloned) when the destination
  waker already is set up for some task.

- :cc:`PW_ASYNC_TRY_STORE_WAKER` and :cc:`PW_ASYNC_TRY_CLONE_WAKER`

  This is an alternative to `PW_ASYNC_STORE_WAKER`, and returns ``false``
  instead of crashing. This lets the pendable to signal to the caller that the
  ``Pend()`` operation failed, so it can be handled in some other way.

One ``Waker`` wakes up one task. If your code needs to wake up multiple
tasks, you should use :cc:`WakerQueue <pw::async2::WakerQueue>`
instead, which allows a fixed capacity list of wakers to be created. Note
that you use the same macros (``PW_ASYNC_STORE_WAKER``, ...) with
``WakerQueue``, at which point they will indicate an error if you run out of
capacity in the queue.
