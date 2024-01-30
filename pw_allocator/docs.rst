.. _module-pw_allocator:

------------
pw_allocator
------------

This module provides various building blocks
for a dynamic allocator. This is composed of the following parts:

- ``block``: An implementation of a doubly-linked list of memory blocks,
  supporting splitting and merging of blocks.
- ``freelist``: A freelist, suitable for fast lookups of available memory chunks
  (i.e. ``block`` s).
- ``allocator``: An interface for memory allocators. Several concrete
  implementations are also provided.

Block
=====
.. doxygenclass:: pw::allocator::Block
   :members:

FreeList
========
.. doxygenclass:: pw::allocator::FreeList
   :members:

Allocator
=========
.. doxygenclass:: pw::allocator::Allocator
   :members:

Example
-------
As an example, the following implements a simple allocator that tracks memory
using ``Block``.

.. literalinclude:: public/pw_allocator/simple_allocator.h
   :language: cpp
   :linenos:
   :start-after: [pw_allocator_examples_simple_allocator]
   :end-before: [pw_allocator_examples_simple_allocator]

Other Implemetations
--------------------
Provided implementations of the ``Allocator`` interface include:

- ``AllocatorMetricProxy``: Wraps another allocator and records its usage.
- ``AllocatorSyncProxy``: Synchronizes access to another allocator, allowing it
  to be used by multiple threads.
- ``FallbackAllocator``: Dispatches first to a primary allocator, and, if that
  fails, to a secondary alloator.
- ``LibCAllocator``: Uses ``malloc``, ``realloc``, and ``free``. This should
  only be used if the ``libc`` in use provides those functions.
- ``MultiplexAllocator``: Abstract class that applications can use to dispatch
  between allocators based on an application-specific request type identifier.
- ``NullAllocator``: Always fails. This may be useful if allocations should be
  disallowed under specific circumstances.
- ``SplitFreeListAllocator``: Tracks memory using ``Block``, and splits large
  and small allocations between the front and back, respectively, of it memory
  region in order to reduce fragmentation.

UniquePtr
=========
.. doxygenclass:: pw::allocator::UniquePtr
   :members:

.. _module-pw_allocator-test-support:

Test Support
============
This module also provides several utilities designed to make it easier to write
tests for custom ``Allocator`` implementations:

AllocatorForTest
----------------
.. doxygenclass:: pw::allocator::test::AllocatorForTest
   :members:

AllocatorTestHarness
--------------------
.. doxygenclass:: pw::allocator::test::AllocatorTestHarness
   :members:

Heap Poisoning
==============

By default, this module disables heap poisoning since it requires extra space.
User can enable heap poisoning by enabling the ``pw_allocator_POISON_HEAP``
build arg.

.. tab-set::

   .. tab-item:: GN
      :sync: gn

      .. code-block:: sh

         $ gn args out
         # Modify and save the args file to use heap poison.
         pw_allocator_POISON_HEAP = true

   .. tab-item:: CMake
      :sync: cmake

      .. code-block:: sh

         # Modify the top-level CMakeLists.txt and add:
         set(pw_allocator_POISON_HEAP, ON)

When heap poisoning is enabled, ``pw_allocator`` will add ``sizeof(void*)``
bytes before and after the usable space of each ``Block``, and paint the space
with a hard-coded randomized pattern. During each check, ``pw_allocator``
will check if the painted space still remains the pattern, and return ``false``
if the pattern is damaged.

Heap Visualizer
===============

Functionality
-------------

``pw_allocator`` supplies a pw command ``pw heap-viewer`` to help visualize
the state of the heap at the end of a dump file. The heap is represented by
ASCII characters, where each character represents 4 bytes in the heap.

.. image:: doc_resources/pw_allocator_heap_visualizer_demo.png

Usage
-----

The heap visualizer can be launched from a shell using the Pigweed environment.

.. code-block:: sh

  $ pw heap-viewer --dump-file <directory of dump file> --heap-low-address
  <hex address of heap lower address> --heap-high-address <hex address of heap
  lower address> [options]

The required arguments are:

- ``--dump-file`` is the path of a file that contains ``malloc/free``
  information. Each line in the dump file represents a ``malloc/free`` call.
  ``malloc`` is represented as ``m <size> <memory address>`` and ``free`` is
  represented as ``f <memory address>``. For example, a dump file should look
  like:

  .. code-block:: sh

    m 20 0x20004450  # malloc 20 bytes, the pointer is 0x20004450
    m 8 0x2000447c   # malloc 8 bytes, the pointer is 0x2000447c
    f 0x2000447c     # free the pointer at 0x2000447c
    ...

  Any line not formatted as the above will be ignored.

- ``--heap-low-address`` is the start of the heap. For example:

  .. code-block:: sh

    --heap-low-address 0x20004440

- ``--heap-high-address`` is the end of the heap. For example:

  .. code-block:: sh

    --heap-high-address 0x20006040

Options include the following:

- ``--poison-enable``: If heap poisoning is enabled during the
  allocation or not. The value is ``False`` if the option is not specified and
  ``True`` otherwise.

- ``--pointer-size <integer of pointer size>``: The size of a pointer on the
  machine where ``malloc/free`` is called. The default value is ``4``.

.. _module-pw_allocator-size:

Size report
===========
``pw_allocator`` provides some of its own implementations of the ``Allocator``
interface, whos costs are shown below.

.. include:: allocator_size_report

.. _module-pw_allocator-metric-collection:

Metric collection
=================
Consumers can use an ``AllocatorMetricProxy`` to wrap an allocator and collect
usage statistics. These statistics are implemented using
:ref:`module-pw_metric`.

.. code-block:: cpp

  MyAllocator allocator;
  AllocatorMetricProxy proxy(PW_TOKENIZE_STRING_EXPR("my allocator"));
  proxy.Init(allocator);
  // ...Perform various allocations and deallocations...
  proxy.Dump();

Metric collection is controlled using the ``pw_allocator_COLLECT_METRICS`` build
argument. If this is unset or ``false``, metric collection is skipped.

To force metric collection regardless of the build argument, tests may include
the ``"pw_allocator/allocator_metric_proxy_for_tests.h"`` header file and depend
on the ``//pw_allocator:allocator_metric_proxy_for_tests`` target.
