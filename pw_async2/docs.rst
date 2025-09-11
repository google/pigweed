.. _module-pw_async2:

=========
pw_async2
=========
.. pigweed-module::
   :name: pw_async2

``pw_async2`` is a cooperatively scheduled asynchronous framework for C++,
optimized for use in resource-constrained systems. It helps you write complex,
concurrent applications without the overhead of traditional preemptive
multithreading. The design prioritizes efficiency, minimal resource usage
(especially memory), and testability.

--------
Benefits
--------
- **Simple Ownership**: Say goodbye to that jumble of callbacks and shared
  state! Complex tasks with many concurrent elements can be expressed by
  simply combining smaller tasks.
- **Efficient**: No dynamic memory allocation required.
- **Pluggable**: Your existing event loop, work queue, or task scheduler
  can run the ``Dispatcher`` without any extra threads.

-------
Example
-------
:ref:`module-pw_async2-informed-poll` is the core design philosophy behind
``pw_async2``. :doxylink:`Task <pw::async2::Task>` is the main async primitive.
It's a cooperatively scheduled "thread" which yields to the
:doxylink:`Dispatcher <pw::async2::Dispatcher>` when waiting. When a ``Task``
is able to make progress, the ``Dispatcher`` runs it again:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-manual]
   :end-before: [pw_async2-examples-basic-manual]

Tasks can then be run on a :doxylink:`Dispatcher <pw::async2::Dispatcher>`
using the :doxylink:`Post() <pw::async2::Dispatcher::Post>` method:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-dispatcher]
   :end-before: [pw_async2-examples-basic-dispatcher]

----------
Learn more
----------
.. grid:: 2

   .. grid-item-card:: :octicon:`light-bulb` Informed poll
      :link: module-pw_async2-informed-poll
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      The core design philosophy behind ``pw_async2``. We strongly encourage
      all ``pw_async2`` users to internalize this concept before attempting to
      use ``pw_async2``!

   .. grid-item-card:: :octicon:`beaker` Codelab
      :link: module-pw_async2-codelab
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Get hands-on experience with the core concepts of ``pw_async2`` by
      building a simple, simulated vending machine.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: module-pw_async2-quickstart
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to quickly integrate ``pw_async2`` into your project and start using
      basic features.

   .. grid-item-card:: :octicon:`rocket` Guides
      :link: module-pw_async2-guides
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to use dispatchers to coordinate tasks, pass data between tasks,
      and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: ../doxygen/group__pw__async2.html
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      C/C++ API reference for ``Task``, ``Dispatcher``, ``CoRo``, and more.

   .. grid-item-card:: :octicon:`pencil` Code size analysis
      :link: module-pw_async2-size-reports
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reports on the code size cost of adding ``pw_async2`` to a system.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Backends
      :link: module-pw_async2-backends
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      You can fulfill the ``pw_async2`` interface with a Pigweed-provided
      backend or roll your own.

   .. grid-item-card:: :octicon:`pencil` Coroutines
      :link: module-pw_async2-coro
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to define tasks with coroutines, allocate memory, perform async
      operations from coroutines, and more.

.. toctree::
   :hidden:
   :maxdepth: 1

   Informed poll <informed_poll>
   codelab
   quickstart
   guides
   backends
   code_size
   coroutines
