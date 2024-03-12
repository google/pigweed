.. _module-pw_allocator-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_allocator
   :tagline: pw_allocator: Flexible, safe, and measurable memory allocation

.. _module-pw_allocator-get-started:

-----------
Get started
-----------
.. tab-set::

   .. tab-item:: Bazel

      Add ``@pigweed//pw_allocator`` to the ``deps`` list in your Bazel target:

      .. code-block::

         cc_library("...") {
           # ...
           deps = [
             # ...
             "@pigweed//pw_allocator",
             # ...
           ]
         }

      For a narrower dependency, depend on subtargets, e.g.
      ``@pigweed//pw_allocator:tracking_allocator``.

      This assumes ``@pigweed`` is the name you pulled Pigweed into your Bazel
      ``WORKSPACE`` as.

      This assumes that your Bazel ``WORKSPACE`` has a `repository`_ named
      ``@pigweed`` that points to the upstream Pigweed repository.

   .. tab-item:: GN

      Add ``dir_pw_allocator`` to the ``deps`` list in your build target:

      .. code-block::

         pw_executable("...") {
           # ...
           deps = [
             # ...
             dir_pw_allocator,
             # ...
           ]
         }

      For a narrower dependency, depend on subtargets, e.g.
      ``"$dir_pw_allocator:tracking_allocator"``.

      If the build target is a dependency of other targets and includes
      allocators in its public interface, e.g. its header file, use
      ``public_deps`` instead of ``deps``.

   .. tab-item:: CMake

      Add ``pw_allocator`` to your ``pw_add_library`` or similar CMake target:

      .. code-block::

         pw_add_library(my_library STATIC
           HEADERS
             ...
           PRIVATE_DEPS
             # ...
             pw_allocator
             # ...
         )

      For a narrower dependency, depend on subtargets, e.g.
      ``pw_allocator.tracking_allocator``, etc.

      If the build target is a dependency of other targets and includes
      allocators in its public interface, e.g. its header file, use
      ``PUBLIC_DEPS`` instead of ``PRIVATE_DEPS``.

-----------------
Inject allocators
-----------------
Routines that need to allocate memory dynamically should do so using the generic
:ref:`module-pw_allocator-api-allocator` interface. By using dependency
injection, memory users can be kept decoupled from the details of how memory is
provided. This yields the most flexibility for modifying or replacing the
specific allocators.

Use the ``Allocator`` interface
===============================
Write or refactor your memory-using routines to take
a pointer or reference to such an object and use the ``Allocate``,
``Deallocate``, ``Reallocate``, and ``Resize`` methods to manage memory. For
example:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-basic-allocate]
   :end-before: [pw_allocator-examples-basic-allocate]

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-basic-deallocate]
   :end-before: [pw_allocator-examples-basic-deallocate]

To invoke methods or objects that inject allocators now requires an allocator to
have been constructed. The exact location where allocators should be
instantiated will vary from project to project, but it's likely to be early in a
program's lifecycle and in a device-specific location of the source tree.

For initial testing on :ref:`target-host`, a simple allocator such as
:ref:`module-pw_allocator-api-libc_allocator` can be used. This allocator is
trivally constructed and simply wraps ``malloc`` and ``free``.

Use New and Delete
==================
In addition to managing raw memory, an ``Allocator`` can also be used to create
and destroy objects:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-basic-new_delete]
   :end-before: [pw_allocator-examples-basic-new_delete]

Use UniquePtr<T>
================
Where possible, using `RAII`_ is a recommended approach for making memory
management easier and less error-prone.
:ref:`module-pw_allocator-api-unique_ptr` is a smart pointer that makes
allocating and deallocating memory more transparent:

.. literalinclude:: examples/basic.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-basic-make_unique]
   :end-before: [pw_allocator-examples-basic-make_unique]

Determine an allocation's Layout
================================
Several of the :ref:`module-pw_allocator-api-allocator` methods take a parameter
of the :ref:`module-pw_allocator-api-layout` type. This type combines the size
and alignment requirements of an allocation. It can be constructed directly, or
if allocating memory for a specific type, by using a templated static method:

.. literalinclude:: examples/block_allocator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-block_allocator-layout_of]
   :end-before: [pw_allocator-examples-block_allocator-layout_of]

As stated above, you should generally try to keep allocator implementation
details abstracted behind the :ref:`module-pw_allocator-api-allocator`
interface. One exception to this guidance is when integrating allocators into
existing code that assumes ``malloc`` and ``free`` semantics. Notably, ``free``
does not take any parameters beyond a pointer describing the memory to be freed.
To implement such a method using ``Deallocate``, you may need to violate the
abstraction and only use allocators that implement the optional ``GetLayout``
method:

.. literalinclude:: examples/block_allocator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-block_allocator-malloc_free]
   :end-before: [pw_allocator-examples-block_allocator-malloc_free]

--------------------------
Choose the right allocator
--------------------------
Once one or more routines are using allocators, you can consider how best to
implement memory allocation for each particular scenario.

Concrete allocator implementations
==================================
This module provides several allocator implementations. The following is an
overview. Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-libc_allocator`: Uses ``malloc``, ``realloc``,
  and ``free``. This should only be used if the ``libc`` in use provides those
  functions.
- :ref:`module-pw_allocator-api-null_allocator`: Always fails. This may be
  useful if allocations should be disallowed under specific circumstances.
- :ref:`module-pw_allocator-api-block_allocator`: Tracks memory using
  :ref:`module-pw_allocator-api-block`. Derived types use specific strategies
  for how to choose a block to use to satisfy a request. See also
  :ref:`module-pw_allocator-design-block`. Derived types include:

  - :ref:`module-pw_allocator-api-first_fit_block_allocator`: Chooses the first
    block that's large enough to satisfy a request. This strategy is very fast,
    but may increase fragmentation.
  - :ref:`module-pw_allocator-api-last_fit_block_allocator`: Chooses the last
    block that's large enough to satisfy a request. This strategy is fast, and
    may fragment memory less than
    :ref:`module-pw_allocator-api-first_fit_block_allocator` when satisfying
    aligned memory requests.
  - :ref:`module-pw_allocator-api-best_fit_block_allocator`: Chooses the
    smallest block that's large enough to satisfy a request. This strategy
    maximizes the avilable space for large allocations, but may increase
    fragmentation and is slower.
  - :ref:`module-pw_allocator-api-worst_fit_block_allocator`: Chooses the
    largest block if it's large enough to satisfy a request. This strategy
    minimizes the amount of memory in unusably small blocks, but is slower.
  - :ref:`module-pw_allocator-api-dual_first_fit_block_allocator`: Acts like
    :ref:`module-pw_allocator-api-first_fit_block_allocator` or
    :ref:`module-pw_allocator-api-last_fit_block_allocator` depending on
    whether a request is larger or smaller, respectively, than a given
    threshold value. This strategy preserves the speed of the two other
    strategies, while fragmenting memory less by co-locating allocations of
    similar sizes.

.. TODO: b/328076428 - Add MonotonicAllocator.

.. TODO: b/328076428 - Add BuddyAllocator.

.. TODO: b/328076428 - Add SlabAllocator.

Forwarding allocator implementations
====================================
This module provides several "forwarding" allocators, as described in
:ref:`module-pw_allocator-design-forwarding`. The following is an overview.
Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-fallback_allocator`: Dispatches first to a
  primary allocator, and, if that fails, to a secondary allocator.
- :ref:`module-pw_allocator-api-synchronized_allocator`: Synchronizes access to
  another allocator, allowing it to be used by multiple threads.
- :ref:`module-pw_allocator-api-tracking_allocator`: Wraps another allocator and
  records its usage.

.. TODO: b/328076428 - Add MemoryResource.

.. _module-pw_allocator-guide-custom_allocator:

Custom allocator implementations
================================
If none of the allocator implementations provided by this module meet your
needs, you can implement your allocator and pass it into any routine that uses
the generic interface.

:ref:`module-pw_allocator-api-allocator` uses an `NVI`_ pattern. A custom
allocator implementation must at a miniumum implement the ``DoAllocate`` and
``DoDeallocate`` methods.

For example, the following is a forwarding allocator that simply writes to the
log whenever a threshold is exceeded:

.. literalinclude:: examples/custom_allocator.h
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator]

.. literalinclude:: examples/custom_allocator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator]

There are also several optional methods you can provide:

- If an implementation of ``DoResize`` isn't provided, then ``Resize`` will
  always return false.
- If an implementation of ``DoReallocate`` isn't provided, then ``Reallocate``
  will try to ``Resize``, and, if unsuccessful, try to ``Allocate``, copy, and
  ``Deallocate``.
- If an implementation of ``DoGetLayout`` isn't provided, then ``GetLayout``
  will always return ``pw::Status::Unimplmented``.
- If an implementation of ``DoQuery`` isn't provided, then ``Query`` will
  always return ``pw::Status::Unimplmented``.

.. TODO: b/328076428 - Make Deallocate optional once traits supporting
   MonotonicAllocator are added.

--------------------
Measure memory usage
--------------------
You can observe how much memory is being used for a particular use case using a
:ref:`module-pw_allocator-api-tracking_allocator`.

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-all_metrics]
   :end-before: [pw_allocator-examples-metrics-all_metrics]

Metric data can be retrieved according to the steps described in
:ref:`module-pw_metric-exporting`, or by using the ``Dump`` method of
:ref:`module-pw_metric-group`:

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-dump]
   :end-before: [pw_allocator-examples-metrics-dump]


The ``AllMetrics`` type used in the example above enables the following metrics:

- **allocated_bytes**: The number of bytes currently allocated by this
  allocator.
- **peak_allocated_bytes**: The most bytes allocated by this allocator at any
  one time.
- **cumulative_allocated_bytes**: The total number of bytes that have been
  allocated by this allocator across its lifetime.
- **num_allocations**: The number of allocation requests this allocator has
  successfully completed.
- **num_deallocations**: The number of deallocation requests this allocator has
  successfully completed.
- **num_resizes**: The number of resize requests this allocator has successfully
  completed.
- **num_reallocations**: The number of reallocation requests this allocator has
  successfully completed.
- **num_failures**: The number of requests this allocator has failed to
  complete.

If you only want a subset of these metrics, you can implement your own metrics
struct. For example:

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-custom_metrics1]
   :end-before: [pw_allocator-examples-metrics-custom_metrics1]

If you have multiple routines that share an underlying allocator that you want
to track separately, you can create multiple tracking allocators that forward to
the same underlying allocator:

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-multiple_trackers]
   :end-before: [pw_allocator-examples-metrics-multiple_trackers]

.. TODO: b/328648868 - Add guide for heap-viewer and link to cli.rst.

------------------------
Detect memory corruption
------------------------
The :ref:`module-pw_allocator-design-block` class provides a few different
mechanisms to help detect memory corruptions when they happen. First, on every
deallocation it will check the integrity of the block header and assert if it
has been modified.

Additionally, you can enable poisoning to detect additional memory corruptions
such as use-after-frees. The ``Block`` type has a template parameter that
enables the ``Poison`` and ``CheckPoison`` methods. Allocators can use these
methods to "poison" blocks on deallocation with a set pattern, and later check
on allocation that the pattern is intact. If it's not, some routine has
modified unallocated memory.

The :ref:`module-pw_allocator-api-block_allocator` class has a
``kPoisonInterval`` template parameter to control how frequently blocks are
poisoned on deallocation. This allows projects to stochiastically sample
allocations for memory corruptions while mitigating the performance impact.

.. literalinclude:: examples/block_allocator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-block_allocator-poison]
   :end-before: [pw_allocator-examples-block_allocator-poison]

----------------------
Test custom allocators
----------------------
If you create your own allocator implementation, it's strongly recommended that
you test it as well. If you're creating a forwarding allocator, you can use
:ref:`module-pw_allocator-api-allocator_for_test`. This simple allocator
provides its own backing storage and automatically frees any outstanding
allocations when it goes out of scope. It also tracks the most recent values
provided as parameters to the interface methods.

For example, the following tests the custom allocator from
:ref:`module-pw_allocator-guide-custom_allocator`:

.. literalinclude:: examples/custom_allocator_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-unit_test]
   :end-before: [pw_allocator-examples-custom_allocator-unit_test]

You can also extend the :ref:`module-pw_allocator-api-allocator_test_harness` to
perform pseudorandom sequences of allocations and deallocations, e.g. as part of
a performance test:

.. literalinclude:: examples/custom_allocator_test_harness.h
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-test_harness]

.. literalinclude:: examples/custom_allocator_perf_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-perf_test]

Even better, you can easily add fuzz tests for your allocator. This module
uses the ``AllocatorTestHarness`` to integrate with :ref:`module-pw_fuzzer` and
provide :ref:`module-pw_allocator-api-fuzzing_support`.

.. literalinclude:: examples/custom_allocator_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-fuzz_test]
   :end-before: [pw_allocator-examples-custom_allocator-fuzz_test]

-----------------------------
Measure custom allocator size
-----------------------------
If you create your own allocator implementation, you may wish to measure its
code size, similar to measurements in the module's own
:ref:`module-pw_allocator-size-reports`. You can use ``pw_bloat`` and
:ref:`module-pw_allocator-api-size_reporter` to create size reports as described
in :ref:`bloat-howto`.

For example, the C++ code for a size report binary might look like:

.. literalinclude:: examples/size_report.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-size_report]

The resulting binary could be compared with the binary produced from
pw_allocator/size_report/first_fit_block_allocator.cc to identify just the code
added in this case by ``CustomAllocator``.

For example, the GN build rule to generate a size report might look liek:

.. code-block::

   pw_size_diff("size_report") {
     title = "Example size report"
     binaries = [
       {
         target = ":size_report"
         base = "$dir_pw_allocator/size_report:first_fit_block_allocator"
         label = "CustomAllocator"
       },
     ]
   }

The size report produced by this rule would render as:

.. include:: examples/custom_allocator_size_report

.. _NVI: https://en.wikipedia.org/wiki/Non-virtual_interface_pattern
.. _RAII: https://en.cppreference.com/w/cpp/language/raii
.. _repository: https://bazel.build/concepts/build-ref#repositories
