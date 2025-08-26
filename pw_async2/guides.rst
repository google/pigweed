.. _module-pw_async2-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_async2

.. _module-pw_async2-guides-implementing-tasks:

------------------
Implementing tasks
------------------
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

------------------------------
How a dispatcher manages tasks
------------------------------
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

----------------------------------------------
Implementing invariants for pendable functions
----------------------------------------------
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
===============================================
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
==========================
When your pendable function has completed, make sure to return
:doxylink:`pw::async2::Ready` to signal that the task is complete.

.. _module-pw_async2-guides-passing-data:

--------------------------
Passing data between tasks
--------------------------
Astute readers will have noticed that the ``Wake`` method does not take any
arguments, and ``DoPoll`` does not provide the task being polled with any
values!

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
Once a value is sent, it will never change, and the task can either go on to
waiting on something else or can complete.

In this pattern, the task would typically hold storage for the value, and you
would implement some custom interface to set the value once one is available.
Setting a value would additionally set some internal flag indicating a value was
set, and also arrange to wake the task via its ``Waker``.

``pw_async2`` provides helpers for implementing this pattern which add a
useful layer of abstraction, as well as ensure the pattern is implemented
correctly.

For the first pair of helpers, the storage for the value is owned by the
receiver helper :doxylink:`OnceReceiver <pw::async2::OnceReceiver>`, and is
tightly linked to the paired :doxylink:`OnceSender <pw::async2::OnceSender>`
which provides the interface for sending a value.

Construct a linked pair of these helpers by calling
:doxylink:`MakeOnceSenderAndReceiver <pw::async2::MakeOnceSenderAndReceiver>`,
which is templated on the value type.

Ownership of the receiver is typically transferred to your Task using
``std::move``. Note that as a side-effect of the move, the linked sender is
modified to point at the new location of the receiver, maintaining the link.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-construction]
   :end-before: [pw_async2-examples-once-send-recv-construction]

The sender helper can also be similarly and safely moved to transfer ownership
of the sender.

The receiving task should use :doxylink:`PW_TRY_READY_ASSIGN` to wait for a
value of type ``Result<T>``, which will either contain a copy of the value
received, or will indicate an error if the sender was destroyed without a value
being sent.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-receiving]
   :end-before: [pw_async2-examples-once-send-recv-receiving]

A value can be sent to the task via the :doxylink:`emplace()
<pw::async2::OnceSender>` member function which allows constructing a value in
place, if that is necessary.

.. literalinclude:: examples/once_send_recv_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-once-send-recv-send-value]
   :end-before: [pw_async2-examples-once-send-recv-send-value]

If your type is expensive to copy, ``pw_async2`` also provides another pair of
helpers in the form of :doxylink:`OnceRefSender <pw::async2::OnceRefSender>`
and :doxylink:`OnceRefReceiver <pw::async2::OnceRefReceiver>`, where the
storage for the type ``T`` is externally owned.

For :doxylink:`OnceRefReceiver <pw::async2::OnceRefReceiver>`, the receiving
task will still use :doxylink:`Pend() <pw::async2::OnceRefReceiver::Pend>` to
get a ``Poll<Status>`` value indicating if the value has been set, but will have
to actually access the value itself through its own pointer or reference to it.

When the sender uses :doxylink:`OnceRefSender <pw::async2::OnceRefSender>` to
set the value, do note that it does require making a copy into the external
value storage, though this can be a shallow copy through a ``std::move`` if
supported.

A complete example showing how to use these helpers can be found in
`//pw_async2/examples/once_send_recv_test.cc`_, and you can try it yourself
with:

.. code-block:: sh

   bazelisk run --config=cxx20 //pw_async2/examples:once_send_recv_test

.. _//pw_async2/examples/once_send_recv_test.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/once_send_recv_test.cc

.. _module-pw_async2-guides-passing-single-values-other:

Other primitives
----------------
More primitives (such as ``MultiSender`` and ``MultiReceiver``) are
in-progress. Users who find that they need other async primitives are
encouraged to contribute them upstream to ``pw::async2``!

.. _module-pw_async2-guides-passing-multiple-values:

Multiple values
===============
If your tasks need to send or receive multiple values, then you can use the
awaitable interface of :doxylink:`pw::InlineAsyncQueue` or
:doxylink:`pw::InlineAsyncDeque` from ``pw_containers``.

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

A complete example for using :doxylink:`pw::InlineAsyncQueue` this way can be
found in `//pw_async2/examples/inline_async_queue_with_tasks_test.cc`_, and you
can try it yourself with:

.. code-block:: sh

   bazelisk run //pw_async2/examples:inline_async_queue_with_tasks_test

.. _//pw_async2/examples/inline_async_queue_with_tasks_test.cc: https://cs.opensource.google/pigweed/pigweed/+/main:pw_async2/examples/inline_async_queue_with_tasks_test.cc

.. _module-pw_async2-guides-timing:

------
Timing
------
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

------------------------------------------------------------
Interacting with async2 from non-async2 code using callbacks
------------------------------------------------------------
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

.. _module-pw_async2-guides-debugging:

---------
Debugging
---------
Tasks registered to a dispatcher can be inspected by calling
``Dispatcher::LogRegisteredTasks()``, which outputs logs for each task in the
dispatcher's pending and sleeping queues.

Sleeping tasks will log information about their assigned wakers, with the
wait reason provided for each.

If space is a concern, the module configuration option
:doxylink:`PW_ASYNC2_DEBUG_WAIT_REASON` can be set to ``0``, disabling wait
reason storage and logging. Under this configuration, only the waker count of a
sleeping task is logged.
