.. _module-pw_allocator-size-reports:

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_allocator
   :tagline: "pw_allocator: Flexible, safe, and measurable memory allocation"

----------------------------------------------------
Size comparision: concrete allocator implementations
----------------------------------------------------
``pw_allocator`` provides some implementations of the
``pw::allocator::Allocator`` interface. The relative code size costs for these
implementations are shown below:

.. include:: allocator_size_report

.. TODO: b/328076428 - change to "allocator_implementations_size_report"

-------------------------------------------------------
Size comparision: forwarding allocators implementations
-------------------------------------------------------
``pw_allocator`` also provides some "forwarding" allocators that wrap other
allocators to provides additional capabilities. The relative code size costs for
these wrappers are shown below:

.. TODO: b/328076428 - include "forwarding_allocators_size_report"
