.. _module-pw_allocator-size-reports:

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_allocator

This module provides the :ref:`module-pw_allocator-api-allocator` interface, as
well as several implementations of it. The tables below shows the
relative code size for the interface and each of these implementations. The
measurement includes a call to each method.

-------------------
Allocator interface
-------------------
The following shows the code size incurred by the
:ref:`module-pw_allocator-api-allocator` interface itself. A call to each method
of the interface is measured using an empty implementation,
:ref:`module-pw_allocator-api-null_allocator`.

.. include:: allocator_api_size_report

---------------------
Block implementations
---------------------
Most of the concrete allocator implementations are block allocators that use
:ref:`module-pw_allocator-design-blocks` to manage allocations. Code size and
memory overhead for blocks varies depending on what features are included.

The following are code sizes for the block implementations provided by this
module.

.. include:: blocks_size_report

Impact of different hardening levels
====================================
This module includes :ref:`module-pw_allocator-config-hardening` which sets
which validation checks are included. Additional checks can detect more errors
at the cost of additional code size, as illustrated in the size report below:

.. include:: hardening_size_report

----------------------
Bucket implementations
----------------------
Most of the concrete allocator implementations are block allocators that use
:ref:`module-pw_allocator-design-buckets` to organize blocks that are not in use
and are available for allocation.

The following are code sizes for the block implementations provided by this
module. These are measured relative to the container they use, as reusing
container types may save code size. See :ref:`module-pw_containers` for code
size information on each container type.

.. include:: buckets_size_report

-------------------------------
Block allocator implementations
-------------------------------
Most of the concrete allocator implementations provided by this module that
are derived from :ref:`module-pw_allocator-api-block_allocator`. The following
are code sizes for each of the block allocator implementations, and are measured
relative to the blocks they use.

.. include:: block_allocators_size_report

----------------------------------------
Other concrete allocator implementations
----------------------------------------
The following are code sizes for the other allocator implementations that
directly manage the memory they use to fulfill requests, but that do not derive
from :ref:`module-pw_allocator-api-block_allocator`. These are measured relative
to the empty implementation measured above.

.. include:: concrete_allocators_size_report

-------------------------------------
Forwarding allocators implementations
-------------------------------------
The following are code sizes for each of the provided "forwarding" allocators as
described by :ref:`module-pw_allocator-design-forwarding`. These are measured
by having the forwarding allocator wrap a
:ref:`module-pw_allocator-api-first_fit_allocator`, and are measured relative to
that implementation.

.. include:: forwarding_allocators_size_report
