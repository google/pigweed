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

:cpp:class:`pw::async2::Task` is Pigweed's async primitive. ``Task`` objects
are cooperatively-scheduled "threads" which yield to the
:cpp:class:`pw::async2::Dispatcher` when waiting. When the ``Task`` is able to make
progress, the ``Dispatcher`` will run it again. For example:

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

Tasks can then be run on a :cpp:class:`pw::async2::Dispatcher` using the
:cpp:func:`pw::async2::Dispatcher::Post` method:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_async2-examples-basic-dispatcher]
   :end-before: [pw_async2-examples-basic-dispatcher]

.. grid:: 2

   .. grid-item-card:: :octicon:`rocket` Quickstart & guides
      :link: module-pw_async2-quickstart-guides
      :link-type: ref
      :class-item: sales-pitch-cta-primary

      How to:

      * Use dispatchers to coordinate tasks
      * Pass data between tasks
      * Use coroutines

      And more.

   .. grid-item-card:: :octicon:`code-square` Reference
      :link: module-pw_async2-reference
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      API reference for:

      * ``Task``
      * ``Dispatcher``
      * ``CoRo``

      And more.

.. grid:: 2

   .. grid-item-card:: :octicon:`code-square` Backends
      :link: module-pw_async2-backends
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      You can fulfill the ``pw_async2`` interface with a Pigweed-provided
      backend or roll your own.

   .. grid-item-card:: :octicon:`pencil` Pigweed blog: C++20 coroutines
      :link: docs-blog-05-coroutines
      :link-type: ref
      :class-item: sales-pitch-cta-secondary

      A blog post on how Pigweed implements coroutines without heap
      allocation, and challenges encountered along the way.

.. toctree::
   :hidden:
   :maxdepth: 1

   guides
   reference
   backends
