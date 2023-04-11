.. _module-pw_async:

================
pw_async
================

--------
Overview
--------
Pigweed's async module provides portable APIs and utilities for writing
asynchronous code. Currently, it provides:

- Message loop APIs

.. attention::
  This module is still under construction. The API is not yet stable.

----------
Dispatcher
----------
Dispatcher is an API for a message loop that schedules and executes Tasks. See
:bdg-ref-primary-line:`module-pw_async_basic` for an example implementation.

Dispatcher is a pure virtual interface that is implemented by backends and
FakeDispatcher. A virtual interface is used instead of a facade to allow
substituting a FakeDispatcher for a Dispatcher backend in tests.

Dispatcher API
==============
.. doxygenclass:: pw::async::Dispatcher
   :members:


Task API
==============
.. doxygenclass:: pw::async::Task
   :members:

Facade API
==========

Task
----
The Task type represents a work item that is submitted to a Dispatcher. The Task
facade enables Dispatcher backends to specify custom state and methods.

The active Task backend is configured with the GN variable
``pw_async_TASK_BACKEND``. The specified target must define a class
``pw::async::backend::NativeTask`` in the header ``pw_async_backend/task.h``
that meets the interface requirements in ``public/pw_async/task.h``. Task will
then trivially wrap ``NativeTask``.

FakeDispatcher
--------------
The FakeDispatcher facade is a utility for simulating a real Dispatcher
in tests. FakeDispatcher simulates time to allow for reliable, fast testing of
code that uses Dispatcher. FakeDispatcher is a facade instead of a concrete
implementation because it depends on Task state for processing tasks, which
varies across Task backends.

The active Task backend is configured with the GN variable
``pw_async_FAKE_DISPATCHER_BACKEND``. The specified target must define a class
``pw::async::test::backend::NativeFakeDispatcher`` in the header
``pw_async_backend/fake_dispatcher.h`` that meets the interface requirements in
``public/pw_async/task.h``. FakeDispatcher will then trivially wrap
``NativeFakeDispatcher``.

Testing FakeDispatcher
^^^^^^^^^^^^^^^^^^^^^^
The GN template ``fake_dispatcher_tests`` in ``fake_dispatcher_tests.gni``
creates a test target that tests a FakeDispatcher backend. This enables
one test suite to be shared across FakeDispatcher backends and ensures
conformance.

FunctionDispatcher
------------------
.. doxygenclass:: pw::async::FunctionDispatcher
   :members:

HeapDispatcher
--------------
.. doxygenclass:: pw::async::HeapDispatcher
   :members:

Design
======

Task Ownership
--------------
Tasks are owned by clients rather than the Dispatcher. This avoids either
memory allocation or queue size limits in Dispatcher implementations. However,
care must be taken that clients do not destroy Tasks before they have been
executed or canceled.

Getting Started
===============
First, configure the Task backend for the Dispatcher backend you will be using:

.. code-block::

   pw_async_TASK_BACKEND = "$dir_pw_async_basic:task"


Next, create an executable target that depends on the Dispatcher backend you
want to use:

.. code-block::

   pw_executable("hello_world") {
     sources = [ "main.cc" ]
     deps = [ "$dir_pw_async_basic:dispatcher" ]
   }

Next, instantiate the Dispatcher and post a task:

.. code-block:: cpp

   #include "pw_async_basic/dispatcher.h"

   int main() {
     BasicDispatcher dispatcher;

     // Spawn a thread for the dispatcher to run on.
     thread::Thread work_thread(thread::stl::Options(), dispatcher);

     Task task([](pw::async::Context& ctx){
       printf("hello world\n");
       ctx.dispatcher->RequestStop();
     });

     // Execute `task` in 5 seconds.
     dispatcher.PostAfter(task, 5s);

     // Blocks until `task` runs.
     work_thread.join();
     return 0;
   }

The above example runs the dispatcher on a new thread, but it can also run on
the current/main thread:

.. code-block:: cpp

   #include "pw_async_basic/dispatcher.h"

   int main() {
     BasicDispatcher dispatcher;

     Task task([](pw::async::Context& ctx){
       printf("hello world\n");
     });

     // Execute `task` in 5 seconds.
     dispatcher.PostAfter(task, 5s);

     dispatcher.Run();
     return 0;
   }

Fake Dispatcher
===============
To test async code, FakeDispatcher should be dependency injected in place of
Dispatcher. Then, time should be driven in unit tests using the ``Run*()``
methods. For convenience, you can use the test fixture
FakeDispatcherFixture.

.. doxygenclass:: pw::async::test::FakeDispatcherFixture
   :members:

.. attention::

   ``FakeDispatcher::now()`` will return the simulated time.
   ``Dispatcher::now()`` should therefore be used to get the current time in
   async code instead of other sources of time to ensure consistent time values
   and reliable tests.

-------
Roadmap
-------
- Stabilize Task cancellation API
- Utility for dynamically allocated Tasks
- Bazel support
- CMake support
- Support for C++20 coroutines
