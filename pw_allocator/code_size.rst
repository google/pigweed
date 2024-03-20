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

----------------------------------
Concrete allocator implementations
----------------------------------
The following are code sizes for each of the provided allocator implementations
that directly manage the memory they use to fulfill requests. These are measured
relative to the empty implementation measured above.

.. include:: concrete_allocators_size_report

-------------------------------------
Forwarding allocators implementations
-------------------------------------
The following are code sizes for each of the provided "forwarding" allocators as
described by :ref:`module-pw_allocator-design-forwarding`. These are measured
by having the forwarding allocator wrap a
:ref:`module-pw_allocator-api-first_fit_block_allocator`, and are measured
relative to that implementation.

.. include:: forwarding_allocators_size_report

