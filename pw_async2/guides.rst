.. _module-pw_async2-quickstart-guides:

===================
Quickstart & guides
===================
.. pigweed-module-subpage::
   :name: pw_async2

.. _module-pw_async2-quickstart:

----------
Quickstart
----------
.. _//pw_async2/examples/count.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/count.cc
.. _//pw_async2/examples/BUILD.bazel: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/BUILD.bazel
.. _//pw_async2/examples/BUILD.gn: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/BUILD.gn

This quickstart outlines the general workflow for integrating ``pw_async2``
into a project. It's based on the following files in upstream Pigweed:

* `//pw_async2/examples/count.cc`_
* `//pw_async2/examples/BUILD.bazel`_
* `//pw_async2/examples/BUILD.gn`_

The example app can be built and run in upstream Pigweed with the
following command:

.. code-block:: sh

   bazelisk run //pw_async2/examples:count --config=cxx20

.. _module-pw_async2-quickstart-rules:

1. Set up build rules
=====================
All ``pw_async2`` projects must add a dependency on the ``dispatcher`` target.
This target defines the :doxylink:`pw::async2::Task` class, an asynchronous
unit of work analogous to a thread, as well as the
:doxylink:`pw::async2::Dispatcher` class, an event loop used to run ``Task``
instances to completion.

.. tab-set::

   .. tab-item:: Bazel

      Add a dependency on ``@pigweed//pw_async2:dispatcher`` in
      ``BUILD.bazel``:

      .. literalinclude:: examples/BUILD.bazel
         :language: py
         :linenos:
         :emphasize-lines: 10
         :start-after: count-example-start
         :end-before: count-example-end

   .. tab-item:: GN

      Add a dependency on ``$dir_pw_async2:dispatcher`` in ``BUILD.gn``:

      .. literalinclude:: examples/BUILD.gn
         :language: py
         :linenos:
         :emphasize-lines: 7
         :start-after: count-example-start
         :end-before: count-example-end

.. _module-pw_async2-quickstart-dependencies:

2. Inject dependencies
======================
Interfaces which wish to add new tasks to the event loop should accept and
store a ``Dispatcher&`` reference.

.. literalinclude:: examples/count.cc
   :language: cpp
   :linenos:
   :start-after: examples-constructor-start
   :end-before: examples-constructor-end

This allows the interface to call ``dispatcher->Post(some_task)`` in order to
run asynchronous work on the dispatcher's event loop.

.. _module-pw_async2-quickstart-oneshot:

3. Post one-shot work to the dispatcher
=======================================
Simple, one-time work can be queued on the dispatcher via
:doxylink:`pw::async2::EnqueueHeapFunc`.

.. _module-pw_async2-quickstart-tasks:

4. Post tasks to the dispatcher
===============================
Async work that involves a series of asynchronous operations should be
made into a task. This can be done by either implementing a custom task
(see :ref:`module-pw_async2-guides-implementing-tasks`) or
by writing a C++20 coroutine (see :doxylink:`pw::async2::Coro`) and storing it
in a :doxylink:`pw::async2::CoroOrElseTask`.

.. literalinclude:: examples/count.cc
   :language: cpp
   :linenos:
   :start-after: examples-task-start
   :end-before: examples-task-end

The resulting task must either be stored somewhere that has a lifetime longer
than the async operations (such as in a static or as a member of a long-lived
class) or dynamically allocated using :doxylink:`pw::async2::AllocateTask`.

Finally, the interface instructs the dispatcher to run the task by invoking
:doxylink:`pw::async2::Dispatcher::Post`.

See `//pw_async2/examples/count.cc`_ to view the complete example.

.. _module-pw_async2-quickstart-toolchain:

5. Build with an appropriate toolchain
======================================
If using coroutines, remember to build your project with a toolchain
that supports C++20 at minimum (the first version of C++ with coroutine
support). For example, in upstream Pigweed a ``--config=cxx20`` must be
provided when building and running the example:

.. tab-set::

   .. tab-item:: Bazel

      .. code-block:: sh

         bazelisk build //pw_async2/examples:count --config=cxx20

Other examples
==============
.. _quickstart/bazel: https://cs.opensource.google/pigweed/quickstart/bazel
.. _//apps/blinky/: https://cs.opensource.google/pigweed/quickstart/bazel/+/main:apps/blinky/
.. _//modules/blinky/: https://cs.opensource.google/pigweed/quickstart/bazel/+/main:modules/blinky/

To see another example of ``pw_async2`` working in a minimal project,
check out the following directories of Pigweed's `quickstart/bazel`_ repo:

* `//apps/blinky/`_
* `//modules/blinky/`_

.. _module-pw_async2-guides:

------
Guides
------

.. _module-pw_async2-guides-implementing-tasks:

Implementing tasks
==================
:doxylink:`pw::async2::Task` instances complete one or more asynchronous
operations. They are the top-level "thread" primitives of ``pw_async2``.

You can use one of the concrete subclasses of ``Task`` that Pigweed provides:

* :doxylink:`pw::async2::CoroOrElseTask`: Delegates to a provided
  coroutine and executes an ``or_else`` handler function on failure.
* :doxylink:`pw::async2::PendFuncTask`: Delegates to a provided
  function.
* :doxylink:`pw::async2::PendableAsTask`: Delegates to a type
  with a ``Pend`` method.
* :doxylink:`pw::async2::AllocateTask`: Creates a concrete subclass of
  ``Task``, just like ``PendableAsTask``, but the created task is
  dynamically allocated and frees the associated memory upon
  completion.

Or you can subclass ``Task`` yourself. See :doxylink:`pw::async2::Task`
for more guidance on subclassing.

.. _module-pw_async2-guides-tasks:

How a dispatcher manages tasks
==============================
The purpose of a :doxylink:`pw::async2::Dispatcher` is to keep track of a set
of :doxylink:`pw::async2::Task` objects and run them to completion. The
dispatcher is essentially a scheduler for cooperatively-scheduled
(non-preemptive) threads (tasks).

While a dispatcher is running, it waits for one or more tasks to waken and then
advances each task by invoking its :doxylink:`pw::async2::Task::DoPend` method.
The ``DoPend`` method is typically implemented manually by users, though it is
automatically provided by coroutines.

If the task is able to complete, ``DoPend`` will return ``Ready``, in which
case the task is then deregistered from the dispatcher.

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

.. _module-pw_async2-guides-pendables:

Implementing invariants for pendable functions
==============================================
.. _invariants: https://stackoverflow.com/a/112088

Any ``Pend``-like function or method similar to
:doxylink:`pw::async2::Task::DoPend` that can pause when it's not able
to make progress on its task is known as a **pendable function**. When
implementing a pendable function, make sure that you always uphold the
following `invariants`_:

* :ref:`module-pw_async2-guides-pendables-incomplete`
* :ref:`module-pw_async2-guides-pendables-complete`

.. note:: Exactly which APIs are considered pendable?

   If it has the signature ``(Context&, ...) -> Poll<T>``,
   then it's a pendable function.

.. _module-pw_async2-guides-pendables-incomplete:

Arranging future completion of incomplete tasks
-----------------------------------------------
When your pendable function can't yet complete:

#. Do one of the following to make sure the task rewakes when it's ready to
   make more progress:

   * Delegate waking to a subtask. Arrange for that subtask's
     pendable function to wake this task when appropriate.

   * Arrange an external wakeup. Use :c:macro:`PW_ASYNC_STORE_WAKER`
     to store the task's waker somewhere, and then call
     :doxylink:`pw::async2::Waker::Wake` from an interrupt or another thread
     once the event that the task is waiting for has completed.

   * Re-enqueue the task with :doxylink:`pw::async2::Context::ReEnqueue`.
     This is a rare case. Usually, you should just create an immediately
     invoked ``Waker``.

#. Make sure to return :doxylink:`pw::async2::Pending` to signal that the task
   is incomplete.

In other words, whenever your pendable function returns
:doxylink:`pw::async2::Pending`, you must guarantee that
``Wake()`` is called once in the future.

For example, one implementation of a delayed task might arrange for its
``Waker`` to be woken by a timer once some time has passed. Another case might
be a messaging library which calls ``Wake()`` on the receiving task once a
sender has placed a message in a queue.

.. _module-pw_async2-guides-pendables-complete:

Cleaning up complete tasks
--------------------------
When your pendable function has completed, make sure to return
:doxylink:`pw::async2::Ready` to signal that the task is complete.

.. _module-pw_async2-guides-passing-data:

Passing data between tasks
==========================
Astute readers will have noticed that the ``Wake`` method does not take any
arguments, and ``DoPoll`` does not provide the task being polled with any
values!

Unlike callback-based interfaces, tasks (and the libraries they use)
are responsible for storage of the inputs and outputs of events. A common
technique is for a task implementation to provide storage for outputs of an
event. Then, upon completion of the event, the outputs will be stored in the
task before it is woken. The task will then be invoked again by the
dispatcher and can then operate on the resulting values.

This common pattern is implemented by the
:doxylink:`pw::async2::OnceSender` and
:doxylink:`pw::async2::OnceReceiver` types (and their ``...Ref`` counterparts).
These interfaces allow a task to asynchronously wait for a value:

.. tab-set::

   .. tab-item:: Manual ``Task`` State Machine

      .. literalinclude:: examples/once_send_recv.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-once-send-recv-manual]
         :end-before: [pw_async2-examples-once-send-recv-manual]

   .. tab-item:: Coroutine Function

      .. literalinclude:: examples/once_send_recv.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-once-send-recv-coro]
         :end-before: [pw_async2-examples-once-send-recv-coro]

More primitives (such as ``MultiSender`` and ``MultiReceiver``) are
in-progress. Users who find that they need other async primitives are
encouraged to contribute them upstream to ``pw::async2``!

.. _module-pw_async2-guides-coroutines:

Coroutines
==========
C++20 users can define tasks using coroutines!

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-coro]
   :end-before: [pw_async2-examples-basic-coro]

Any value with a ``Poll<T> Pend(Context&)`` method can be passed to
``co_await``, which will return with a ``T`` when the result is ready. The
:doxylink:`pw::async2::PendFuncAwaitable` class can also be used to
``co_await`` on a provided delegate function.

To return from a coroutine, ``co_return <expression>`` must be used instead of
the usual ``return <expression>`` syntax. Because of this, the
:c:macro:`PW_TRY` and :c:macro:`PW_TRY_ASSIGN` macros are not usable within
coroutines. :c:macro:`PW_CO_TRY` and :c:macro:`PW_CO_TRY_ASSIGN` should be
used instead.

For a more detailed explanation of Pigweed's coroutine support, see
:doxylink:`pw::async2::Coro`.

.. _module-pw_async2-guides-timing:

Timing
======
When using ``pw::async2``, timing functionality should be injected
by accepting a :doxylink:`pw::async2::TimeProvider` (most commonly
``TimeProvider<SystemClock>`` when using the system's built-in ``time_point``
and ``duration`` types).

:doxylink:`pw::async2::TimeProvider` allows for easily waiting
for a timeout or deadline using the
:doxylink:`pw::async2::TimeProvider::WaitFor` and
:doxylink:`pw::async2::TimeProvider::WaitUntil` methods.

Additionally, code which uses :doxylink:`pw::async2::TimeProvider` for timing
can be tested with simulated time using
:doxylink:`pw::async2::SimulatedTimeProvider`. Doing so helps avoid
timing-dependent test flakes and helps ensure that tests are fast since they
don't need to wait for real-world time to elapse.
.. _module-pw_async2-guides-callbacks:

Interacting with async2 from non-async2 code using callbacks
=============================================================
In a system gradually or partially adopting ``pw_async2``, there are often
cases where non-async2 code needs to run asynchronous operations built with
``pw_async2``.

To facilitate this, ``pw_async2`` provides callback tasks:
:doxylink:`pw::async2::OneshotCallbackTask` and
:doxylink:`pw::async2::RecurringCallbackTask`.

These tasks invoke a :ref:`pendable function
<module-pw_async2-guides-pendables>`, forwarding its result to a provided
callback on completion.

The two variants of callback tasks are:

* :doxylink:`pw::async2::OneshotCallbackTask\<T>
  <pw::async2::OneshotCallbackTask>`: Pends the pendable. When it returns
  ``Ready(value)``, the callback is invoked once with ``value``. After the
  callback finishes, the ``OneshotCallbackTask`` itself completes and is done.
  This is useful for single asynchronous requests.

* :doxylink:`pw::async2::RecurringCallbackTask\<T>
  <pw::async2::RecurringCallbackTask>`: Similar to the oneshot version, but
  after the callback is invoked, the ``RecurringCallbackTask`` continues
  polling the pendable function. This is suitable for operations that produce
  a stream of values over time, where you want to process each one.

Example
-------
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
     pw::async2::OneshotCallbackTaskFor<&ReadValue> task([](pw::Result<int> result) {
       if (result.ok()) {
         PW_LOG_INFO("Read value: %d", result.value());
       } else {
         PW_LOG_ERROR("Failed to read value: %s", result.status().str());
       }
     });

     PostTaskToDispatcher(task);

     // In this example, the task is stack allocated, so we would need to wait
     // for it to complete before it goes out of scope. In a real application,
     // the task may be a member of a long-lived object or be statically
     // allocated.
   }

.. _module-pw_async2-guides-interrupts:

Interacting with hardware
=========================
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

Unit testing
============
Unit testing ``pw_async2`` code is different from testing non-async code. Async
code must be run from a :doxylink:`task <pw::async2::Task>` on a
:doxylink:`dispatcher <pw::async2::Dispatcher>`.

To test ``pw_async2`` code:

#. Declare a dispatcher.
#. Create a task to run the async code under test. Either implement
   :doxylink:`pw::async2::Task` or use :doxylink:`pw::async2::PendFuncTask` to
   wrap a lambda.
#. Post the task to the dispatcher.
#. Call :doxylink:`pw::async2::Dispatcher::RunUntilStalled` to execute the
   task.

The following example shows the basic structure of a ``pw_async2`` unit test.

.. literalinclude:: examples/unit_test.cc
   :language: c++
   :start-after: pw_async2-minimal-test
   :end-before: pw_async2-minimal-test

It is usually necessary to run the test task multiple times to advance async
code through its states. This improves coverage and ensures that wakers are
stored and woken properly. To run the test task multiple times:

#. Post the task to the dispatcher.
#. Call :doxylink:`pw::async2::Dispatcher::RunUntilStalled`, which returns
   :doxylink:`pw::async2::Pending`.
#. Perform actions to allow the task to advance.
#. Call :doxylink:`RunUntilStalled() <pw::async2::Dispatcher::RunUntilStalled>`
   again.
#. Repeat until the task runs to completion and :doxylink:`RunUntilStalled()
   <pw::async2::Dispatcher::RunUntilStalled>` returns
   :doxylink:`pw::async2::Ready`.

The example below runs a task multiple times to test waiting for a
``FortuneTeller`` class to produce a fortune.

.. literalinclude:: examples/unit_test.cc
   :language: c++
   :start-after: pw_async2-multi-step-test
   :end-before: pw_async2-multi-step-test

.. _module-pw_async2-guides-inline-async-queue-with-tasks:

Using InlineAsyncQueue and InlineAsyncDeque with tasks
======================================================
When you have two tasks, you may need a way to send data between them. One good
way to do that is to leverage the async-aware containers
:doxylink:`pw::InlineAsyncQueue` or :doxylink:`pw::InlineAsyncDeque` from
``pw_containers``, both of which implement a fixed-size deque.

The following example can be built and run in upstream Pigweed with the
following command:

.. code-block:: sh

   bazelisk run //pw_async2/examples:inline-async-queue-with-tasks

The complete code can be found here:

.. _//pw_async2/examples/inline_async_queue_with_tasks_test.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/inline_async_queue_with_tasks_test.cc

* `//pw_async2/examples/inline_async_queue_with_tasks_test.cc`_

The C++ code simulates a producer and consumer task setup, where the producer
writes to the queue, and the consumer reads it. For purposes of this example,
the data is just integers, with a fixed sequence sent by the producer.

To start with, here are the basic declarations for the queue and the two tasks.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-declarations]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-declarations]

The producer ``DoPend()`` member function coordinates writing to the queue, and
has to ensure there is available space in it for the remaining data. It also
writes the special ``kTerminal`` value signal that the end of the data stream.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-producer-do-pend]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-producer-do-pend]

The consumer ``DoPend()`` member function coordinates reading from the queue,
and has to ensure there is data to read before reading it.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-consumer-do-pend]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-consumer-do-pend]

At that point, it is straightforward to set up the dispatcher to run the two
tasks.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-tasks-run]
   :end-before: [pw_async2-examples-inline-async-queue-with-tasks-run]

Running the example should produce the following output.

.. literalinclude:: examples/inline_async_queue_with_tasks_test.expected
   :start-after: [ RUN      ] ExampleTests.InlineAsyncQueueWithTasks
   :end-before: [       OK ] ExampleTests.InlineAsyncQueueWithTasks

Notice how the producer DoPend() function fills up the queue with four values,
then the consumer ``DoPend()`` gets a chance to empty the queue before the
writer ``DoPend()`` is invoked again.

.. _module-pw_async2-guides-inline-async-queue-with-coro:

Using InlineAsyncQueue and InlineAsyncDeque with coroutine tasks
================================================================
If you choose to use C++20 coroutines, you can also use an async2 dispatcher,
as well as awaiting on the pendable interface for the queue.

The following example can be built and run in upstream Pigweed with the
following command:

.. code-block:: sh

   bazelisk run //pw_async2/examples:inline-async-queue-with-coro --config=cxx20

The complete code can be found here:


.. _//pw_async2/examples/inline_async_queue_with_coro_test.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/inline_async_queue_with_coro_test.cc

* `//pw_async2/examples/inline_async_queue_with_coro_test.cc`_

The C++ code simulates a producer and consumer task setup, where the producer
writes to the queue, and the consumer reads it. For purposes of this example,
the data is just integers, with a fixed sequence sent by the producer.

To start with, here are the basic declarations for the queue and a special
terminal sentinel value.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-declarations]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-declarations]

To use the :doxylink:`PendHasSpace
<pw::containers::internal::AsyncCountAndCapacity::PendHasSpace>`, and
:doxylink:`PendNotEmpty
<pw::containers::internal::AsyncCountAndCapacity::PendNotEmpty>` functions with
``co_await``, we need to use :doxylink:`pw::async2::PendFuncAwaitable` as an
adapter between the async2 polling system and the C++20 coroutine framework.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-adapters]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-adapters]

The producer coroutine just needs to return a ``Coro<Status>`` to turn
it into a coroutine, and to use the ``QueueHasSpace`` adapter we define
to wait for there to be space in the queue. Once it is done, it should
``co_return`` a status value to indicate it is complete.

Compare this to the inline_async_queue_with_task.cc example, where the
``Producer::DoPend`` function has to be written in a way that allows
the function to be called fresh at any time, and has to figure out what it
should do next.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-producer]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-producer]

The consumer coroutine similarly needs to return a ``Coro<Status>``
value, and to use the ``QueueNotEmpty`` adapter we define to wait there
to be content in the queue. Once it is done, it should ``co_return`` a status
value to indicate it is complete.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-consumer]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-consumer]

At that point, it is straightforward to set up the dispatcher to run the two
coroutines. Notice however that the :doxylink:`pw::async2::CoroContext` also
needs to allocate memory dynamically when the coroutine is first created. For
this example, we use :doxylink:`LibCAllocator <pw::allocator::LibCAllocator>`.

.. literalinclude:: examples/inline_async_queue_with_coro_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-inline-async-queue-with-coro-run]
   :end-before: [pw_async2-examples-inline-async-queue-with-coro-run]

Running the example should produce the following output.

.. literalinclude:: examples/inline_async_queue_with_coro_test.expected
   :start-after: [ RUN      ] ExampleTests.InlineAsyncQueueWithCoro
   :end-before: [       OK ] ExampleTests.InlineAsyncQueueWithCoro

Notice how the producer fills up the queue with four values, then the consumer
gets a chance to empty the queue before the writer gets another chance to run.

Debugging
=========
Tasks registered to a dispatcher can be inspected by calling
``Dispatcher::LogRegisteredTasks()``, which outputs logs for each task in the
dispatcher's pending and sleeping queues.

Sleeping tasks will log information about their assigned wakers, with the
wait reason provided for each.

If space is a concern, the module configuration option
:doxylink:`PW_ASYNC2_DEBUG_WAIT_REASON` can be set to ``0``, disabling wait
reason storage and logging. Under this configuration, only the waker count of a
sleeping task is logged.

.. _module-pw_async2-guides-faqs:

---------------------------------
Frequently asked questions (FAQs)
---------------------------------
