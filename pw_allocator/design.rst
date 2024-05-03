.. _module-pw_allocator-design:

================
Design & roadmap
================
.. pigweed-module-subpage::
   :name: pw_allocator

-----------------------
Design of pw::Allocator
-----------------------
Traditionally, most embedded firmware have laid out their systems’ memory usage
statically, with every component’s buffers and resources set at compile time. As
systems grow larger and more complex, dynamic allocation provides increasing
opportunities to simplify code and improve memory usage by enabling sharing and
eliminating large reservations.

As a result, ``pw_allocator`` seeks to make dynamic allocation possible without
sacrificing too much of the control over memory usage that embedded developers
are accustomed to and need. The fundamental design goals of ``pw_allocator`` are
for allocators to be:

- Familiar: The interface and its usage should resemble that of C++17's
  `polymorphic allocators`_.
- Flexible: A diverse set of allocation strategies should be implementable
  using allocators.
- Composable: Allocators should be able to combine and use other allocators.
- Extensible: Downstream projects should be able to provide their own allocator
  implementations, and easily integrate them with Pigweed's.
- Cost-effective: Projects should be able to include only the allocator
  behaviors they desire.
- Observable: Allocators should provide tools and data to reveal how memory is
  being used.
- Correcting: Allocators should include features to help uncover
  `memory defects`_ including heap corruption, leaks, use-after-frees, etc.

Differences with C++ polymorphic allocators
===========================================
C++17 introduced the ``<memory_resource>`` header with support for polymorphic
memory resources (PMR), i.e. allocators. This library defines many allocator
interfaces similar to those in ``pw_allocator``.

Pigweed has decided to keep ``pw_allocator`` distinct from PMR primarily because
the latter's interface requires the use of C++ language features prohibited by
Pigweed. In PMR, allocators are expected to throw an exception in the case of
failure, and equality comparisons require runtime type identification (RTTI).

Even so, ``pw_allocator`` has taken inspiration from the design of PMR,
incorporating many of its ideas. :ref:`module-pw_allocator-api-allocator` in
particular is similar to `std::pmr::memory_resource`_.

.. TODO: b/328076428 - Furthermore, ``pw::allocator::MemoryResource`` acts as a
   PMR adapter, allowing Pigweed allocators to be used with the C++ STL, albeit
   at the cost of an extra layer of virtual indirection.

.. _module-pw_allocator-design-forwarding:

Forwarding allocator concept
============================
In addition to concrete allocator implementations, the design of
``pw_allocator`` also encourages the use of "forwarding" allocators. These are
implementations of the :ref:`module-pw_allocator-api-allocator` interface that
don't allocate memory directly and instead rely on other allocators while
providing some additional behavior.

For example, the :ref:`module-pw_allocator-api-allocator` records various
metrics such as the peak number of bytes allocated and the number of failed
allocation requests. It wraps another allocator which is used to actually
perform dynamic allocation. It implements the allocator API, and so it can be
passed into any routines that use dependency injection by taking a generic
:ref:`module-pw_allocator-api-allocator` parameter.

These "forwarding" allocators are not completely free. At a miniumum, they
represent an extra virtual indirection, and an extra function call, albeit one
that can often be inlined. Additional behavior-specific code or state adds to
their cost in terms of code size and performance. Even so, these "forwarding"
allocators can provide savings relative to a "`golden hammer`_"-style allocator
that combined all of their features and more. By decomposing allocators into
orthogonal behaviors, implementers can choose to pay for only those that they
want.

-----------------------------
Design of allocator utilities
-----------------------------
In addtion to providing allocator implementations themselves, ``pw_allocator``
includes some foundational classes that can be used to implement allocators.

.. _module-pw_allocator-design-block:

pw::allocator::Block
====================
Several allocators make use of allocation metadata stored inline with the
allocations themselves. Often referred to as a "header", this metadata
immediately precedes the pointer to usable space returned by the allocator. This
header allows allocations to be variably sized, and converts allocation into a
`bin packing problem`_. An allocator that uses headers has a miniumum alignment
matching that of the header type itself.

For ``pw_allocator``, the most common way to store this header is as a
:ref:`module-pw_allocator-api-block`. This class is used to construct a
doubly-linked list of subsequences of the allocator's memory region. It was
designed with the following features:

- **Templated offset types**: Rather than use pointers to the next and previous
  blocks, ``Block`` uses offsets of a templated unsigned integral type. This
  saves a few bits that can be used for other purposes, since the blocks are
  always aligned to the block header. It also gives callers the ability to
  reduce the size of the headers if the allocator's memory region is
  sufficently small, e.g. a type of ``uint16_t`` could be used if the region
  could hold no more than 65536 block headers.
- **Splitting and merging**: This class centralizes the logic for splitting
  memory regions into smaller pieces. Usable sub-blocks can either be split from
  the beginning or end of a block. Additionally, blocks from  either end can be
  split at specified alignment boundaries. This class also provides the logic for
  merging blocks back together. Together, these methods provide the invariant
  that a free block is only ever adjacent to blocks in use.
- **Validation and poisoning**: On every deallocation, blocks validate their
  metadata against their neighbors. A block can fail to be validated if it or
  its neighbors have had their headers overwritten. In this case, it's unsafe to
  continue to use this memory and the block code will assert in order make you
  aware of the problem. Additionally, blocks can "paint" their memory with a
  known poison pattern that's checked whenever the memory is next allocated. If
  the check fails, then some code has written to unallocated memory. Again, the
  block code will assert to alert you of a "use-after-free" condition.

.. tip::
   In the case of memory corruption, the validation routines themsleves may
   crash while attempting to inspect block headers. These crashes are not
   exploitable from a security perspective, but lack the diagnostic information
   from the usual ``PW_CHECK`` macro. Examining a stack trace may be helpful in
   determining why validation failed.

.. _module-pw_allocator-design-metrics:

Allocator metrics
=================
A common desire for a project using dynamic memory is to clearly understand how
much memory is being allocated. However, each tracked metric adds code size,
memory overhead, and a per-call performance cost. As a result, ``pw_allocator``
is design to allow allocator implementers to select just the metrics they're
interested in.

In particular, the :ref:`module-pw_allocator-api-metrics_adapter` uses
per-metric type traits generated by ``PW_ALLOCATOR_METRICS_DECLARE`` to
conditionally include the code to update the metrics that are included in its
``MetricsType`` template parameter type. A suitable ``MetricType`` struct can be
created using the ``PW_ALLOCATOR_METRICS_ENABLE`` macro, which will only create
fields for the enabled metrics.

Using these macros prevents unwanted metrics from increasing either the code
size or object size of the metrics adapter, and by extension,
:ref:`module-pw_allocator-api-tracking_allocator`.

-------
Roadmap
-------
While the :ref:`module-pw_allocator-api-allocator` interface is almost stable,
there are some outstanding features the Pigweed team would like to add to
``pw_allocator``:

- **Asynchronous allocators**: Determine whether these should be provided, and
  if so, add them.
- **Additional smart pointers**: Determine if pointers like ``std::shared_ptr``,
  etc., are needed, and if so, add them.
- **Dynamic containers**: Provide the concept of allocator equality without
  using RTTI or ``typeid``. This would allow dynamic containers with their own
  allocators.
- **Default allocators**: Integrate ``pw_allocator`` into the monolithic
  ``pw_system`` as a starting point for projects.

Found a bug? Got a feature request? Please create a new issue in our `tracker`_!

Want to discuss allocators in real-time with the Pigweed team? Head over to our
`Discord`_!

.. _polymorphic allocators: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator
.. _memory defects: https://en.wikipedia.org/wiki/Memory_corruption
.. _golden hammer: https://en.wikipedia.org/wiki/Law_of_the_instrument#Computer_programming
.. _bin packing problem: https://en.wikipedia.org/wiki/Bin_packing_problem
.. _std::pmr::memory_resource: https://en.cppreference.com/w/cpp/memory/memory_resource
.. _tracker: https://pwbug.dev
.. _Discord: https://discord.gg/M9NSeTA
