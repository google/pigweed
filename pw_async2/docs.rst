.. _module-pw_async2:

=========
pw_async2
=========
.. pigweed-module::
   :name: pw_async2

   - **Simple Ownership**: Say goodbye to that jumble of callbacks and shared
     state! Complex tasks with many concurrent elements can be expressed by
     simply combining smaller tasks.
   - **Efficient**: No dynamic memory allocation required.
   - **Pluggable**: Your existing event loop, work queue, or task scheduler
     can run the ``Dispatcher`` without any extra threads.
   - **Coroutine-capable**: C++20 coroutines work just like other tasks, and can
     easily plug into an existing ``pw_async2`` systems.

:cpp:class:`pw::async2::Task` is Pigweed's async primitive. ``Task`` objects
are cooperatively-scheduled "threads" which yield to the
:cpp:class:`pw::async2::Dispatcher` when waiting. When the ``Task`` is able to make
progress, the ``Dispatcher`` will run it again. For example:

.. tab-set::

   .. tab-item:: Manual ``Task`` State Machine

      .. literalinclude:: examples/basic.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-basic-manual]
         :end-before: [pw_async2-examples-basic-manual]

   .. tab-item:: Coroutine Function

      .. literalinclude:: examples/basic.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-basic-coro]
         :end-before: [pw_async2-examples-basic-coro]

Tasks can then be run on a :cpp:class:`pw::async2::Dispatcher` using the
:cpp:func:`pw::async2::Dispatcher::Post` method:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-dispatcher]
   :end-before: [pw_async2-examples-basic-dispatcher]

.. _module-pw_async2-concepts:

------
Guides
------

.. _module-pw_async2-tasks:

Dispatchers and tasks
=====================
The purpose of a :cpp:class:`pw::async2::Dispatcher` is to keep track of a set
of :cpp:class:`pw::async2::Task` objects and run them to completion. The
dispatcher is essentially a scheduler for cooperatively-scheduled
(non-preemptive) threads (tasks).

While a dispatcher is running, it waits for one or more tasks to awaken and then
advances each task by invoking its :cpp:func:`pw::async2::Task::DoPend` method.
The ``DoPend`` method is typically implemented manually by users, though it is
automatically provided by coroutines.

If the task is able to complete, ``DoPend`` will return ``Ready``, in which case
the task is then deregistered from the dispatcher.

If the task is unable to complete, ``DoPend`` must return ``Pending`` and arrange
for the task to be woken up when it is able to make progress again. Once the
task is reawoken, the task is re-added to the ``Dispatcher`` queue. The
dispatcher will then invoke ``DoPend`` once more, continuing the cycle until
``DoPend`` returns ``Ready`` and the task is completed.

.. _module-pw_async2-waking:

Waking
======
When a task is unable to complete without waiting, the implementor of
``DoPend`` must return ``Pending`` and should arrange for the task to be reawoken
once ``DoPend`` may be able to make more progress. This is done by calling
:cpp:func:`pw::async2::Context::GetWaker` to get a
:cpp:class:`pw::async2::Waker` for the current task. In order to wake the
task up and put it back on the dispatcher's queue,
:cpp:func:`pw::async2::Waker::Wake` must be called.

For example, one implementation of a delayed task might arrange for its ``Waker``
to be awoken by a timer once some time has passed. Another case might be a
messaging library which calls ``Wake`` on the receiving task once a sender has
placed a message in a queue.

.. _module-pw_async2-passing-data:

Passing data between tasks
==========================
Astute readers will have noticed that the ``Wake`` method does not take any
arguments, and ``DoPoll`` does not provide the task being polled with any
values!

Unlike callback-based interfaces, tasks (and the libraries they use)
are responsible for storage of the inputs and outputs of events. A common
technique is for a task implementation to provide storage for outputs of an
event. Then, upon completion of the event, the outputs will be stored in the
task before it is awoken. The task will then be invoked again by the
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

.. _module-pw_async2-coroutines:

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

For a more detailed explanation of Pigweed's coroutine support, see the
documentation on the :cpp:class:`pw::async2::Coro` type.

.. _module-pw_async2-timing:

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
timing-dependent test flakes, as well as ensure that tests are fast since they
don't need to wait for real-world time to elapse.

-----------------
C++ API reference
-----------------
.. doxygenclass:: pw::async2::Task
  :members:

.. doxygenclass:: pw::async2::Poll
  :members:

.. doxygenfunction:: pw::async2::Ready()

.. doxygenfunction:: pw::async2::Ready(std::in_place_t, Args&&... args)

.. doxygenfunction:: pw::async2::Ready(T&& value)

.. doxygenfunction:: pw::async2::Pending()

.. doxygenclass:: pw::async2::Context
  :members:

.. doxygenclass:: pw::async2::Waker
  :members:

.. doxygenclass:: pw::async2::Dispatcher
  :members:

.. doxygenclass:: pw::async2::Coro
  :members:

.. doxygenclass:: pw::async2::CoroContext
  :members:

.. doxygenclass:: pw::async2::TimeProvider
   :members:

.. doxygenfunction:: pw::async2::GetSystemTimeProvider

.. doxygenclass:: pw::async2::SimulatedTimeProvider
   :members:

-------------
C++ Utilities
-------------
.. doxygenfunction:: pw::async2::AllocateTask(pw::allocator::Allocator& allocator, Pendable&& pendable)

.. doxygenfunction:: pw::async2::AllocateTask(pw::allocator::Allocator& allocator, Args&&... args)

.. doxygenclass:: pw::async2::CoroOrElseTask
  :members:

.. doxygenclass:: pw::async2::PendFuncTask
  :members:

.. doxygenclass:: pw::async2::PendableAsTask
  :members:


.. doxygenfunction:: pw::async2::MakeOnceSenderAndReceiver

.. doxygenclass:: pw::async2::OnceSender
  :members:

.. doxygenclass:: pw::async2::OnceReceiver
  :members:

.. doxygenfunction:: pw::async2::MakeOnceRefSenderAndReceiver

.. doxygenclass:: pw::async2::OnceRefSender
  :members:

.. doxygenclass:: pw::async2::OnceRefReceiver
  :members:

.. toctree::
   :hidden:
   :maxdepth: 1

   Backends <backends>
