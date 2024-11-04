.. _module-pw_async2-quickstart-guides:

===================
Quickstart & guides
===================
.. pigweed-module-subpage::
   :name: pw_async2

.. _module-pw_async2-guides:

------
Guides
------

Implementing tasks
==================
:cpp:class:`pw::async2::Task` instances complete one or more asynchronous
operations. They are the top-level "thread" primitives of ``pw_async2``.

You can use one of the concrete subclasses of ``Task`` that Pigweed provides:

* :cpp:class:`pw::async2::CoroOrElseTask`: Delegates to a provided
  coroutine and executes an ``or_else`` handler function on failure.
* :cpp:class:`pw::async2::PendFuncTask`: Delegates to a provided
  function.
* :cpp:class:`pw::async2::PendableAsTask`: Delegates to a type
  with a :cpp:func:`pw::async2::Pend` method.
* :cpp:func:`pw::async2::AllocateTask`: Creates a concrete subclass of
  ``Task``, just like ``PendableAsTask``, but the created task is
  dynamically allocated and frees the associated memory upon
  completion.

Or you can subclass ``Task`` yourself. See :cpp:class:`pw::async2::Task`
for more guidance on subclassing.

.. _module-pw_async2-guides-tasks:

How a dispatcher manages tasks
==============================
The purpose of a :cpp:class:`pw::async2::Dispatcher` is to keep track of a set
of :cpp:class:`pw::async2::Task` objects and run them to completion. The
dispatcher is essentially a scheduler for cooperatively-scheduled
(non-preemptive) threads (tasks).

While a dispatcher is running, it waits for one or more tasks to waken and then
advances each task by invoking its :cpp:func:`pw::async2::Task::DoPend` method.
The ``DoPend`` method is typically implemented manually by users, though it is
automatically provided by coroutines.

If the task is able to complete, ``DoPend`` will return ``Ready``, in which case
the task is then deregistered from the dispatcher.

If the task is unable to complete, ``DoPend`` must return ``Pending`` and arrange
for the task to be woken up when it is able to make progress again. Once the
task is rewoken, the task is re-added to the ``Dispatcher`` queue. The
dispatcher will then invoke ``DoPend`` once more, continuing the cycle until
``DoPend`` returns ``Ready`` and the task is completed.

.. _module-pw_async2-guides-pendables:

Implementing invariants for pendable functions
==============================================
.. _invariants: https://stackoverflow.com/a/112088

Any ``Pend``-like function or method similar to
:cpp:func:`pw::async2::Task::DoPend` that can pause when it's not able
to make progress on its task is known as a **pendable function**. When
implementing a pendable function, make sure that you always uphold the
following `invariants`_:

* :ref:`module-pw_async2-guides-pendables-incomplete`
* :ref:`module-pw_async2-guides-pendables-complete`

.. note:: Exactly which APIs are considered pendable?

   If you see ``Pend`` in the function name, it's probably
   pendable.

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
     :cpp:func:`pw::async2::Waker::Wake` from an interrupt or another thread
     once the event that the task is waiting for has completed.

   * Re-enqueue the task with :cpp:func:`pw::async2::Context::ReEnqueue`.
     This is a rare case. Usually, you should just create an immediately
     invoked ``Waker``.

#. Make sure to return :cpp:type:`pw::async2::Pending` to signal that the task
   is incomplete.

In other words, whenever your pendable function returns
:cpp:type:`pw::async2::Pending`, you must guarantee that
:cpp:func:`pw::async2::Context::Wake` is called once in the future.

For example, one implementation of a delayed task might arrange for its ``Waker``
to be woken by a timer once some time has passed. Another case might be a
messaging library which calls ``Wake()`` on the receiving task once a sender has
placed a message in a queue.

.. _module-pw_async2-guides-pendables-complete:

Cleaning up complete tasks
--------------------------
When your pendable function has completed, make sure to return
:cpp:type:`pw::async2::Ready` to signal that the task is complete.

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
:cpp:class:`pw::async2::OnceSender` and
:cpp:class:`pw::async2::OnceReceiver` types (and their ``...Ref`` counterparts).
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

More primitives (such as ``MultiSender`` and ``MultiReceiver``) are in-progress.
Users who find that they need other async primitives are encouraged to
contribute them upstream to ``pw::async2``!

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
``co_await``, which will return with a ``T`` when the result is ready.

To return from a coroutine, ``co_return <expression>`` must be used instead of
the usual ``return <expression>`` syntax. Because of this, the
:c:macro:`PW_TRY` and :c:macro:`PW_TRY_ASSIGN` macros are not usable within
coroutines. :c:macro:`PW_CO_TRY` and :c:macro:`PW_CO_TRY_ASSIGN` should be
used instead.

For a more detailed explanation of Pigweed's coroutine support, see
:cpp:class:`pw::async2::Coro`.

.. _module-pw_async2-guides-timing:

Timing
======
When using ``pw::async2``, timing functionality should be injected
by accepting a :cpp:class:`pw::async2::TimeProvider` (most commonly
``TimeProvider<SystemClock>`` when using the system's built-in ``time_point``
and ``duration`` types).

:cpp:class:`pw::async2::TimeProvider` allows for easily waiting
for a timeout or deadline using the
:cpp:func:`pw::async2::TimePoint::WaitFor` and
:cpp:func:`pw::async2::TimePoint::WaitUntil` methods.

Additionally, code which uses :cpp:class:`pw::async2::TimeProvider` for timing
can be tested with simulated time using
:cpp:class:`pw::async2::SimulatedTimeProvider`. Doing so helps avoid
timing-dependent test flakes and helps ensure that tests are fast since they
don't need to wait for real-world time to elapse.

.. _module-pw_async2-guides-faqs:

---------------------------------
Frequently asked questions (FAQs)
---------------------------------
