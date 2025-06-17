.. _module-pw_bytes-design:

================
Design & roadmap
================
.. pigweed-module-subpage::
   :name: pw_bytes

``pw_bytes`` provides a suite of C++ utilities for low-level manipulation of
binary data. The design prioritizes type safety, compile-time operations,
and developer convenience for common embedded programming tasks involving
byte arrays, memory layout, and data representation.

---------------------------------
Design of byte manipulation tools
---------------------------------

Type safety with ``std::byte``
==============================
Many utilities in ``pw_bytes`` operate on or produce ``std::byte`` arrays or
spans. This encourages type safety by distinguishing byte-oriented data from
character or arithmetic data, preventing accidental misuse.

Compile-time operations
=======================
Functions like ``Concat()``, ``String()``, and ``Array()`` allow for the
construction and validation of byte arrays at compile-time. This catches errors
early and can lead to more efficient runtime code as some data structures can
be entirely defined in read-only memory.

Endianness control
==================
Embedded systems often interact with hardware registers or communication
protocols that have specific endianness requirements. ``pw_bytes/endian.h``
provides clear and explicit functions (e.g., ``ConvertOrderTo()``,
``ReadInOrder()``) to manage byte order conversions, reducing the risk of
subtle bugs related to endianness mismatches. The API encourages developers to
be explicit about the endianness of data they are working with.

Memory alignment
================
Correct memory alignment is crucial for performance and, on some architectures,
correctness. ``pw_bytes/alignment.h`` offers utilities like ``AlignUp``,
``AlignDown``, and ``IsAlignedAs`` to help manage and verify alignment,
simplifying a common source of low-level bugs.

Memory efficiency with ``pw::PackedPtr``
========================================
``PackedPtr`` allows developers to reclaim unused low-order bits in aligned
pointers to store small amounts of auxiliary data. This can be valuable in
memory-constrained environments where every bit counts, allowing for more
compact data structures without requiring separate fields for small flags or
values. This comes at the cost of some indirection and masking when accessing
the pointer or the packed data.

Convenience for byte units
==========================
``pw_bytes/units.h`` provides constants (e.g., ``kBytesInKibibyte``) and
user-defined literals (e.g., ``1_KiB``) for specifying byte sizes according to
IEC standards (KiB, MiB, etc.). This improves code readability and reduces
errors compared to using raw integer literals for byte counts.

Building byte sequences with ``pw::ByteBuilder``
================================================
``ByteBuilder`` and ``ByteBuffer`` offer a safe way to construct byte sequences
in a fixed-size buffer. They track the current size and status of operations,
preventing buffer overflows and providing error information if an append
operation fails due to insufficient space. This is particularly useful for
assembling packets or structured binary data.

Bit-level access
================
``pw_bytes/bit.h`` provides utilities for common bit-level operations like sign
extension (``pw::bytes::SignExtend``) and bitfield extraction
(``ExtractBits``), complementing the C++20 ``<bit>`` header's features like
``std::endian``.

.. _module-pw_bytes-roadmap:

-------
Roadmap
-------
- Further enhancements to compile-time validation of byte operations.
- Exploration of additional utilities for common binary data patterns and
  protocols.
- Continued focus on minimizing code size and maximizing performance for
  resource-constrained devices.
