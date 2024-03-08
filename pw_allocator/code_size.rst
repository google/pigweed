.. _module-pw_allocator-size-reports:

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_allocator
   :tagline: "pw_allocator: Flexible, safe, and measurable memory allocation"

This module provides several implementations of the
:ref:`module-pw_allocator-api-allocator` interface. The tables below shows the
relative code size for each of these allocators' independent implementation of
the complete allocator interface. Your code size may be smaller if you use a
subset of the interface or if you use more than one allocator.

----------------------------------------------------
Size comparision: concrete allocator implementations
----------------------------------------------------
The following are code size for the provided allocator implementation that
directly manage the memory they use to fulfill requests:

.. include:: concrete_allocators_size_report

-------------------------------------------------------
Size comparision: forwarding allocators implementations
-------------------------------------------------------
The following are code size for the provided "forwarding" allocators as
described by :ref:`module-pw_allocator-design-forwarding`:

.. include:: forwarding_allocators_size_report

