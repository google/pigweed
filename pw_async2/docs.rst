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
  easily plug into an existing ``pw_async2`` system.

:doxylink:`pw::async2::Task` is Pigweed's async primitive. ``Task`` objects
are cooperatively-scheduled "threads" which yield to the
:doxylink:`pw::async2::Dispatcher` when waiting. When the ``Task`` is able to
make progress, the ``Dispatcher`` will run it again. For example:

.. tab-set::

   .. tab-item:: Manual state machine

      .. literalinclude:: examples/basic.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-basic-manual]
         :end-before: [pw_async2-examples-basic-manual]

   .. tab-item:: Coroutine

      .. literalinclude:: examples/basic.cc
         :language: cpp
         :linenos:
         :start-after: [pw_async2-examples-basic-coro]
         :end-before: [pw_async2-examples-basic-coro]

Tasks can then be run on a :doxylink:`pw::async2::Dispatcher` using the
:doxylink:`pw::async2::Dispatcher::Post` method:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-dispatcher]
   :end-before: [pw_async2-examples-basic-dispatcher]

.. grid:: 1

   .. grid-item-card:: :octicon:`rocket` Quickstart
      :link: module-pw_async2-quickstart
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to quickly integrate ``pw_async2`` into your project and start using
      basic features.

.. grid:: 1

   .. grid-item-card:: :octicon:`beaker` Codelab
      :link: module-pw_async2-codelab
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      Learn the core concepts of ``pw_async2`` by building a simple, simulated
      vending machine.

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Guides
      :link: module-pw_async2-guides
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to use dispatchers to coordinate tasks, pass data between tasks,
      and more.

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: ../doxygen/group__pw__async2.html
      :link-type: url
      :class-item: sales-pitch-cta-secondary

      C/C++ API reference for ``Task``, ``Dispatcher``, ``CoRo``, and more.

.. grid:: 2

   .. grid-item-card:: :octicon:`pencil` Code size analysis
      :link: module-pw_async2-size-reports
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Reports on the code size cost of adding ``pw_async2`` to a system.

   .. grid-item-card:: :octicon:`code-square` Backends
      :link: module-pw_async2-backends
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      You can fulfill the ``pw_async2`` interface with a Pigweed-provided
      backend or roll your own.

.. grid:: 2

   .. grid-item-card:: :octicon:`stack` Design
      :link: module-pw_async2-design
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      Core concepts, the pendable function interface, execution
      model, memory model, interoperability, and more.

   .. grid-item-card:: :octicon:`pencil` Coroutines
      :link: module-pw_async2-coro
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      How to define tasks with coroutines, allocate memory, perform async
      operations from coroutines, and more.

.. toctree::
   :hidden:
   :maxdepth: 1

   quickstart
   codelab
   guides
   design
   backends
   code_size
   coroutines
