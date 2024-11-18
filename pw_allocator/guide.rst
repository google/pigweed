.. _module-pw_allocator-guides:

======
Guides
======
.. pigweed-module-subpage::
   :name: pw_allocator

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

.. _module-pw_allocator-module-configuration:

--------------------
Module configuration
--------------------
This module has configuration options that globally affect the behavior of
``pw_allocator`` via compile-time configuration of this module, see the
:ref:`module documentation <module-structure-compile-time-configuration>` for
more details.

.. doxygendefine:: PW_ALLOCATOR_STRICT_VALIDATION
.. doxygendefine:: PW_ALLOCATOR_BLOCK_POISON_INTERVAL

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

.. literalinclude:: examples/block_allocator.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-block_allocator-malloc_free]
   :end-before: [pw_allocator-examples-block_allocator-malloc_free]

.. _module-pw_allocator-use-standard-library-containers:

Use standard library containers
===============================
All of C++'s standard library containers are `AllocatorAwareContainers`_, with
the exception of ``std::array``. These types include a template parameter used
to specify an allocator type, and a constructor which takes a reference to an
object of this type.

While there are
:ref:`module-pw_allocator-design-differences-with-polymorphic-allocators`, an
:ref:`module-pw_allocator-api-allocator` can be used with these containers by
wrapping them with a PMR adapter type,
:ref:`module-pw_allocator-api-pmr_allocator`:

.. literalinclude:: examples/pmr.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-pmr]
   :end-before: [pw_allocator-examples-pmr]

.. Warning::
   Some of the standard library containers may add a significant amount of
   additional code size and/or memory overhead. In particular, implementations
   of ``std::deque`` are known to preallocate significant memory in order to
   meet its complexity requirements, e.g. O(1) insertion at the front of the
   deque.

.. Warning::
   The standard library containers expect their allocators to throw an exception
   on allocation failure, and do not check for failure themselves. If
   exceptions are disabled, :ref:`module-pw_allocator-api-pmr_allocator`
   instead **asserts** that allocation succeeded. Care must be taken in this
   case to ensure that memory is not exhausted.

--------------------------
Choose the right allocator
--------------------------
Once one or more routines are using allocators, you can consider how best to
implement memory allocation for each particular scenario.

.. TODO(b/378549332): Add a decision tree for selecting an allocator.

Concrete allocator implementations
==================================
This module provides several allocator implementations. The following is an
overview. Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-libc_allocator`: Uses ``malloc``, ``realloc``,
  and ``free``. This should only be used if the ``libc`` in use provides those
  functions. This allocator is a stateless singleton that may be referenced
  using ``GetLibCAllocator()``.
- :ref:`module-pw_allocator-api-null_allocator`: Always fails. This may be
  useful if allocations should be disallowed under specific circumstances.
  This allocator is a stateless singleton that may be referenced using
  ``GetNullAllocator()``.
- :ref:`module-pw_allocator-api-bump_allocator`: Allocates objects out of a
  region of memory and only frees them all at once when the allocator is
  destroyed.
- :ref:`module-pw_allocator-api-buddy_allocator`: Allocates objects out of a
  blocks with sizes that are powers of two. Blocks are split evenly for smaller
  allocations and merged on free.
- :ref:`module-pw_allocator-api-block_allocator`: Tracks memory using
  :ref:`module-pw_allocator-api-block`. Derived types use specific strategies
  for how to choose a block to use to satisfy a request. See also
  :ref:`module-pw_allocator-design-block`. Derived types include:

  - :ref:`module-pw_allocator-api-first_fit_allocator`: Chooses the first
    block that's large enough to satisfy a request. This strategy is very fast,
    but may increase fragmentation.
  - :ref:`module-pw_allocator-api-best_fit_allocator`: Chooses the
    smallest block that's large enough to satisfy a request. This strategy
    maximizes the avilable space for large allocations, but may increase
    fragmentation and is slower.
  - :ref:`module-pw_allocator-api-worst_fit_allocator`: Chooses the
    largest block if it's large enough to satisfy a request. This strategy
    minimizes the amount of memory in unusably small blocks, but is slower.
  - :ref:`module-pw_allocator-api-bucket_block_allocator`: Sorts and stores
    each free blocks in a :ref:`module-pw_allocator-api-bucket` with a given
    maximum block inner size.

- :ref:`module-pw_allocator-api-typed_pool`: Efficiently creates and
  destroys objects of a single given type.

Forwarding allocator implementations
====================================
This module provides several "forwarding" allocators, as described in
:ref:`module-pw_allocator-design-forwarding`. The following is an overview.
Consult the :ref:`module-pw_allocator-api` for additional details.

- :ref:`module-pw_allocator-api-fallback_allocator`: Dispatches first to a
  primary allocator, and, if that fails, to a secondary allocator.
- :ref:`module-pw_allocator-api-pmr_allocator`: Adapts an allocator to be a
  ``std::pmr::polymorphic_allocator``, which can be used with standard library
  containers that `use allocators`_, such as ``std::pmr::vector<T>``.
- :ref:`module-pw_allocator-api-synchronized_allocator`: Synchronizes access to
  another allocator, allowing it to be used by multiple threads.
- :ref:`module-pw_allocator-api-tracking_allocator`: Wraps another allocator and
  records its usage.

.. _module-pw_allocator-guide-custom_allocator:

Custom allocator implementations
================================
If none of the allocator implementations provided by this module meet your
needs, you can implement your allocator and pass it into any routine that uses
the generic interface.

:ref:`module-pw_allocator-api-allocator` uses an `NVI`_ pattern. To add a custom
allocator implementation, you must at a miniumum implement the ``DoAllocate``
and ``DoDeallocate`` methods.

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
- If an implementation of ``DoGetInfo`` isn't provided, then ``GetInfo``
  will always return ``pw::Status::Unimplmented``.

Custom allocators can indicate which optional methods they implement and what
optional behaviors they want from the base class by specifying
:ref:`module-pw_allocator-api-capabilities` when invoking the base class
constructor.

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
   :start-after: [pw_allocator-examples-metrics-custom_metrics1]
   :end-before: [pw_allocator-examples-metrics-custom_metrics1]

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-custom_metrics2]
   :end-before: [pw_allocator-examples-metrics-custom_metrics2]

Metric data can be retrieved according to the steps described in
:ref:`module-pw_metric-exporting`, or by using the ``Dump`` method of
:ref:`module-pw_metric-group`:

.. literalinclude:: examples/metrics.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-metrics-dump]
   :end-before: [pw_allocator-examples-metrics-dump]


The ``CustomMetrics`` type used in the example above is a struct provided by the
developer. You can create your own metrics structs that enable zero or more of
the following metrics:

- **requested_bytes**: The number of bytes currently requested from this
  allocator.
- **peak_requested_bytes**: The most bytes requested from this allocator at any
  one time.
- **cumulative_requested_bytes**: The total number of bytes that have been
  requested from this allocator across its lifetime.
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

Measure fragmentation
=====================

If you are using a :ref:`module-pw_allocator-api-block_allocator`, you can use
the ``MeasureFragmentation`` method to examine how fragmented the heap is. This
method returns a :ref:`module-pw_allocator-api-fragmentation` struct, which
includes the "sum of squares" and the sum of the inner sizes of the current free
blocks. On a platform or host with floating point support, you can divide the
square root of the sum of squares by the sum to obtain a number that ranges from
0 to 1 to indicate maximal and minimal fragmenation, respectively. Subtracting
this number from 1 can give a more intuitive "fragmenation score".

For example, consider a heap consisting of the following blocks:

- 100 bytes in use.
- 200 bytes free.
- 50 bytes in use.
- 10 bytes free.
- 200 bytes in use.
- 300 bytes free.

For such a heap, ``MeasureFragmentation`` will return 130100 and 510. The above
calculation gives a fragmentation score of ``1 - sqrt(130100) / 510``, which is
approximately ``0.29``.

.. TODO: b/328648868 - Add guide for heap-viewer and link to cli.rst.

------------------------
Detect memory corruption
------------------------
The :ref:`module-pw_allocator-design-block` class provides a few different
mechanisms to help detect memory corruptions when they happen. First, on every
deallocation it will check the integrity of the block header and assert if it
has been modified.

Additionally, you can enable poisoning to detect additional memory corruptions
such as use-after-frees. The :ref:`module-pw_allocator-module-configuration` for
``pw_allocator`` includes the ``PW_ALLOCATOR_BLOCK_POISON_INTERVAL`` option,
which will "poison" every N-th ``Block``. Allocators "poison" blocks on
deallocation by writing a set pattern to the usable memory, and later check on
allocation that the pattern is intact. If it's not, some routine has modified
unallocated memory.

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

You can also extend the :ref:`module-pw_allocator-api-test_harness` to perform
pseudorandom sequences of allocations and deallocations, e.g. as part of a
performance test:

.. literalinclude:: examples/custom_allocator_test_harness.h
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-test_harness]

.. literalinclude:: examples/custom_allocator_perf_test.cc
   :language: cpp
   :linenos:
   :start-after: [pw_allocator-examples-custom_allocator-perf_test]

Even better, you can easily add fuzz tests for your allocator. This module
uses the :ref:`module-pw_allocator-api-test_harness` to integrate with
:ref:`module-pw_fuzzer` and provide
:ref:`module-pw_allocator-api-fuzzing_support`.

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
pw_allocator/size_report/first_fit.cc to identify just the code added in this
case by ``CustomAllocator``.

For example, the GN build rule to generate a size report might look liek:

.. code-block::

   pw_size_diff("size_report") {
     title = "Example size report"
     binaries = [
       {
         target = ":size_report"
         base = "$dir_pw_allocator/size_report:first_fit"
         label = "CustomAllocator"
       },
     ]
   }

The size report produced by this rule would render as:

.. include:: examples/custom_allocator_size_report

.. _AllocatorAwareContainers: https://en.cppreference.com/w/cpp/named_req/AllocatorAwareContainer
.. _NVI: https://en.wikipedia.org/wiki/Non-virtual_interface_pattern
.. _RAII: https://en.cppreference.com/w/cpp/language/raii
.. _repository: https://bazel.build/concepts/build-ref#repositories
.. _use allocators: https://en.cppreference.com/w/cpp/memory/uses_allocator
