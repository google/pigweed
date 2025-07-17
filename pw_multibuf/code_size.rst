.. _module-pw_multibuf-size-reports:

==================
Code size analysis
==================
.. pigweed-module-subpage::
   :name: pw_multibuf

The following table shows the code size cost of using ``pw_multibuf``.
These size reports compare a fake ``MultiBuf`` implementation against the v1
and v2 ``pw_multibuf`` implementations, and also compares the two ``pw_multibuf``
implementations against each other.

The scenarios being measured involve sending and receiving data using a fake
protocol stack. At the lowest layer, "DemoLink" frames carry "DemoNetwork"
packets, which in turn carry segments of a "DemoTransport" message. This layered
protocol demonstrates how different ``MultiBuf`` implementations can handle data
encapsulation and fragmentation.

The v0 implementation uses simple, contiguous spans of memory, requiring data
to be copied at each protocol layer. In contrast, ``pw_multibuf`` is designed
to avoid these copies. It allows for reserving space for headers and footers,
and can represent data as a collection of discontiguous memory chunks. This
reduces memory usage and improves performance by minimizing data movement.

The key difference between ``pw_multibuf`` v1 and v2 lies in their approach to
memory management. Version 1 integrates memory allocation directly, with a
``MultiBufAllocator`` that handles the creation of ``MultiBuf`` objects.
Version 2 separates memory allocation from the buffer logic, allowing users to
provide their own allocation schemes. Additionally, v2 introduces the concept
of "layers" to more explicitly manage different views of the data, such as
protocol layers.

.. include:: size_report/multibuf_size_report
