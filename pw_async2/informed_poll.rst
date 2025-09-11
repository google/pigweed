.. _module-pw_async2-informed-poll:

=============
Informed poll
=============
.. pigweed-module-subpage::
   :name: pw_async2

The *informed poll* programming model is the core design philosophy behind
``pw_async2``. Informed poll provides an alternative to callback-based
asynchronous programming that is highly efficient, requires no dynamic memory
allocation, and simplifies state management for complex concurrent operations.
This model was popularized by Rust's `Future`_ trait and was first proposed for
Pigweed in :ref:`SEED 0112 <seed-0112>`. We find that informed poll is very
well-suited for resource-constrained embedded systems. It's not yet a
common paradigm in embedded C++ codebases, though, so we strongly encourage all
``pw_async2`` users to read this page and internalize the informed poll
worldview before attempting to use ``pw_async2``!

-------
Summary
-------
The central idea is that asynchronous work is encapsulated in objects called
:doxylink:`Tasks <pw::async2::Task>`. Instead of registering callbacks for
different events, a central :doxylink:`Dispatcher <pw::async2::Dispatcher>`
*polls* these tasks to see if they can make progress. The polling is *informed*
because the task coordinates with its event source regarding when it's ready
to make more progress. The event source notifies the dispatcher when the task
is ready to proceed and therefore should be polled again.

---------------
Core components
---------------
``pw_async2`` is built upon a few fundamental concepts that work together to
provide a powerful asynchronous runtime.

Tasks, the basic async primitive
================================
A :doxylink:`Task <pw::async2::Task>` is the basic unit of execution, analogous
to a cooperatively scheduled thread. It's an object that represents a job to be
done, like reading from a sensor or processing a network packet. Users
implement a task's logic in its :doxylink:`DoPend() <pw::async2::Task::DoPend>`
method.

Cooperative scheduling with Dispatchers
=======================================
The :doxylink:`Dispatcher <pw::async2::Dispatcher>` is the cooperative
scheduler. It maintains a queue of tasks that are ready to be polled.

Running tasks and communicating task state
==========================================
The dispatcher runs a task by calling ``Pend()``, which is a non-virtual
wrapper around ``DoPend()``.  The task attempts to make progress and
communicates to the dispatcher what state it's in by returning one of these
values:

* :doxylink:`Ready() <pw::async2::Ready>`: The task has finished its work. The
  ``Dispatcher`` should not poll it again.
* :doxylink:`Pending() <pw::async2::Pending>`: The task is not yet finished
  because it is waiting for an external event. E.g. it's waiting for a timer to
  finish or for data to arrive. The dispatcher should put the task to sleep and
  then run it again later.

.. note::

   A user writing a task implements ``DoPend()``, but they too call ``Pend()``
   when calling other tasks. ``Pend()`` is also the interface of "pendable
   objects" used throughout ``pw_async2`` such as :doxylink:`Select
   <pw::async2::Select>`, :doxylink:`Join <pw::async2::Join>`, and
   :ref:`coroutines <module-pw_async2-coro>`.

"Informed" polling with wakers
==============================
When a task's ``DoPend`` method returns ``Pending()``, the task must ensure
that something will eventually trigger it to be run again. This is the
"informed" part of the model: the task *informs* the ``Dispatcher`` when it's
ready to be polled again. This is achieved using a :doxylink:`Waker
<pw::async2::Waker>`.

1. Before returning ``Pending()``, the task must obtain its ``Waker`` from the
   :doxylink:`Context <pw::async2::Context>` and store it somewhere that's
   accessible to the event source. Common event sources include interrupt
   handlers and timer managers.
2. When the event occurs, the event source calls :doxylink:`Waker::Wake()
   <pw::async2::Waker::Wake>` on the stored ``Waker``.
3. The ``Wake()`` call notifies the ``Dispatcher`` that the task is ready to
   make progress.
4. The ``Dispatcher`` moves the task back into its run queue and polls it
   again in the future.

This mechanism prevents the ``Dispatcher`` from having to wastefully poll tasks
that aren't ready, allowing it to sleep and save power when no work can be
done.

The following diagram illustrates the interaction between these components:

.. mermaid::

   sequenceDiagram
     participant e as Event
     participant d as Dispatcher
     participant t as Task
     e->>d: Post(Task)
     d->>d: Add task to run queue
     d->>t: Run task via Task::DoPend()
     t->>t: Task is waiting for data, cannot complete
     t->>e: Store Waker for future wake-up
     t->>d: Return Pending()
     d->>d: Remove task from run queue (now sleeping)
     e->>e: The data the task needs arrives
     e->>d: Wake task via Waker::Wake()
     d->>d: Re-add task to run queue
     d->>t: Run task again via Task::DoPend()
     t->>t: Task uses data and runs to completion
     t->>d: Return Ready()
     d->>d: Deregister the completed task

---------------------------------------
Comparison with Rust's informed polling
---------------------------------------
The basic idea of informed poll is the same: register a waker to be notified
when to poll.

Async Rust is built around the `Future`_ trait, which ``pw_async2`` doesn't
have. ``pw_async2`` informally has "pendable" objects, but unlike Rust's
`Future`_ (or `Stream`_), the semantics are unspecified. We plan to formalize
these concepts in ``pw_async2`` and narrow the conceptual gap with Rust.

.. _Future: https://doc.rust-lang.org/std/future/trait.Future.html
.. _Stream: https://docs.rs/futures/latest/futures/prelude/trait.Stream.html
