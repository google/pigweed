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
  `std::pmr::polymorphic_allocator`_ type.
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

.. _module-pw_allocator-design-differences-with-polymorphic-allocators:

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

This similarity is most evident in the PMR adapter class,
:ref:`module-pw_allocator-api-pmr_allocator`. This adapter allows any
:ref:`module-pw_allocator-api-allocator` to be used as a
`std::pmr::polymorphic_allocator`_ with any standard library that
`can use an allocator`_. Refer to the guides on how to
:ref:`module-pw_allocator-use-standard-library-containers`.

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

.. _module-pw_allocator-design-blocks:

Blocks of memory
================
Several allocators make use of allocation metadata stored inline with the
allocations themselves. Often referred to as a "header", this metadata
immediately precedes the pointer to usable space returned by the allocator. This
header allows allocations to be variably sized, and converts allocation into a
`bin packing problem`_. An allocator that uses headers has a miniumum alignment
matching that of the header type itself.

For ``pw_allocator``, the most common way to store this header is as a
:ref:`module-pw_allocator-api-block`. Specific block implementations are created
by providing a concrete representation and implementing the required methods for
one or more of the block mix-ins. Each block mix-in provides a specific set of
features, allowing block implementers to include only what they need. Features
provided by these block mix-ins include:

- A :ref:`module-pw_allocator-api-basic-block` can retrieve the memory that
  makes up its usable space and its size.
- A :ref:`module-pw_allocator-api-contiguous-block` knows the blocks that are
  adjacent to it in memory. It can merge with neighboring blocks and split
  itself into smaller sub-blocks.
- An :ref:`module-pw_allocator-api-allocatable-block` knows when it is free or
  in-use. It can allocate new blocks from either the beginning or end of its
  usable space when free. When in-use, it can be freed and merged with
  neighboring blocks that are free. This ensures that free blocks are only ever
  adjacent to blocks in use, and vice versa.
- An :ref:`module-pw_allocator-api-alignable-block` can additionally allocate
  blocks from either end at specified alignment boundaries.
- A :ref:`module-pw_allocator-api-block-with-layout` can retrieve the layout
  used to allocate it, even if the block itself is larger due to alignment or
  padding.
- The :ref:`module-pw_allocator-api-forward-iterable-block` and
  :ref:`module-pw_allocator-api-reverse-iterable-block` types provide iterators
  and ranges that can be used to iterate over a sequence of blocks.
- A :ref:`module-pw_allocator-api-poisonable-block` can fill its usable space
  with a pattern when freed. This pattern can be checked on a subsequent
  allocation to detect if the memory was illegally modified while free.

In addition to poisoning, blocks validate their metadata against their neighbors
on each allocation and deallocation. A block can fail to be validated if it or
its neighbors have had their headers overwritten. In this case, it's unsafe to
continue to use this memory and the block code will assert in order make you
aware of the problem.

.. tip::
   In the case of memory corruption, the validation routines themsleves may
   crash while attempting to inspect block headers. These crashes are not
   exploitable from a security perspective, but lack the diagnostic information
   from the usual ``PW_CHECK`` macro. Examining a stack trace may be helpful in
   determining why validation failed.

.. _module-pw_allocator-design-buckets:

Buckets of blocks
=================
The most important role of a :ref:`module-pw_allocator-api-block_allocator` is
to choose the right block to satisfy an allocation request. Different block
allocators use different strategies to accomplish this, and thus need different
data structures to organize blocks in order to be able to choose them
efficiently.

For example, a block allocator that uses a "best-fit" strategy needs to be able
to efficiently search free blocks by usable size in order to find the smallest
candidate that could satisfy the request.

The :ref:`module-pw_allocator-api-basic-block` mix-in requires blocks to specify
both a ``MinInnerSize`` and ``DefaultAlignment``. Together these ensure that the
usable space of free blocks can be treated as intrusive items for containers.
The bucket classes that derive from :ref:`module-pw_allocator-api-bucket_base`
provide such containers to store and retrieve free blocks with different
performance and code size characteristics.

.. _module-pw_allocator-design-metrics:

Buckets of blocks
=================
The most important role of a :ref:`module-pw_allocator-api-block_allocator` is
to choose the right block to satisfy an allocation request. Different block
allocators use different strategies to accomplish this, and thus need different
data structures to organize blocks in order to be able to choose them
efficiently.

For example, a block allocator that uses a "best-fit" strategy needs to be able
to efficiently search free blocks by usable size in order to find the smallest
candidate that could satisfy the request.

The :ref:`module-pw_allocator-api-basic-block` mix-in requires blocks to specify
both a ``MinInnerSize`` and ``DefaultAlignment``. Together these ensure that the
usable space of free blocks can be treated as intrusive items for containers.
The :ref:`module-pw_allocator-api-bucket` provide such containers to store and
retrieve free blocks with different performance and code size characteristics.

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

.. _memory defects: https://en.wikipedia.org/wiki/Memory_corruption
.. _golden hammer: https://en.wikipedia.org/wiki/Law_of_the_instrument#Computer_programming
.. _bin packing problem: https://en.wikipedia.org/wiki/Bin_packing_problem
.. _std::pmr::memory_resource: https://en.cppreference.com/w/cpp/memory/memory_resource
.. _std::pmr::polymorphic_allocator: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator
.. _can use an allocator: https://en.cppreference.com/w/cpp/memory/uses_allocator
.. _tracker: https://pwbug.dev
.. _Discord: https://discord.gg/M9NSeTA
