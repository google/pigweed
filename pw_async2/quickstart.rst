.. _module-pw_async2-quickstart:

==========
Quickstart
==========
.. pigweed-module-subpage::
   :name: pw_async2

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

---------------------------
1. Understand informed poll
---------------------------
Informed poll is the core design philosophy behind ``pw_async2``. Please read
:ref:`module-pw_async2-informed-poll` before attempting to use ``pw_async2``.

.. _module-pw_async2-quickstart-rules:

---------------------
2. Set up build rules
---------------------
All ``pw_async2`` projects must add a dependency on the ``dispatcher`` target.
This target defines the :doxylink:`Task <pw::async2::Task>` class, an
asynchronous unit of work analogous to a thread, as well as the
:doxylink:`Dispatcher <pw::async2::Dispatcher>` class, an event loop used to
run ``Task`` instances to completion.

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

----------------------
3. Inject dependencies
----------------------
Interfaces which wish to add new tasks to the event loop should accept and
store a ``Dispatcher&`` reference.

.. literalinclude:: examples/count.cc
   :language: cpp
   :linenos:
   :start-after: examples-constructor-start
   :end-before: examples-constructor-end

This allows the interface to call ``Dispatcher::Post`` in order to
run asynchronous work on the dispatcher's event loop.

.. _module-pw_async2-quickstart-oneshot:

---------------------------------------
4. Post one-shot work to the dispatcher
---------------------------------------
Simple, one-time work can be queued on the dispatcher via
:doxylink:`EnqueueHeapFunc <pw::async2::EnqueueHeapFunc>`.

.. _module-pw_async2-quickstart-tasks:

-------------------------------
5. Post tasks to the dispatcher
-------------------------------
Async work that involves a series of asynchronous operations should be made
into a task. This can be done by either implementing a custom task (see
:ref:`module-pw_async2-guides-implementing-tasks`) or by writing a C++20
coroutine (see :doxylink:`Coro <pw::async2::Coro>`) and storing it in a
:doxylink:`CoroOrElseTask <pw::async2::CoroOrElseTask>`.

.. literalinclude:: examples/count.cc
   :language: cpp
   :linenos:
   :start-after: examples-task-start
   :end-before: examples-task-end

The resulting task must either be stored somewhere that has a lifetime longer
than the async operations (such as in a static or as a member of a long-lived
class) or dynamically allocated using :doxylink:`AllocateTask
<pw::async2::AllocateTask>`.

Finally, the interface instructs the dispatcher to run the task by invoking
:doxylink:`Dispatcher::Post() <pw::async2::Dispatcher::Post>`.

See `//pw_async2/examples/count.cc`_ to view the complete example.

.. _module-pw_async2-quickstart-toolchain:

--------------------------------------
6. Build with an appropriate toolchain
--------------------------------------
If using coroutines, remember to build your project with a toolchain
that supports C++20 at minimum (the first version of C++ with coroutine
support). For example, in upstream Pigweed a ``--config=cxx20`` must be
provided when building and running the example:

.. tab-set::

   .. tab-item:: Bazel

      .. code-block:: sh

         bazelisk build //pw_async2/examples:count --config=cxx20

--------------
Other examples
--------------
.. _quickstart/bazel: https://cs.opensource.google/pigweed/quickstart/bazel
.. _//apps/blinky/: https://cs.opensource.google/pigweed/quickstart/bazel/+/main:apps/blinky/
.. _//modules/blinky/: https://cs.opensource.google/pigweed/quickstart/bazel/+/main:modules/blinky/

To see another example of ``pw_async2`` working in a minimal project,
check out the following directories of Pigweed's `quickstart/bazel`_ repo:

* `//apps/blinky/`_
* `//modules/blinky/`_
