.. _module-pw_numeric:

==========
pw_numeric
==========
.. pigweed-module::
   :name: pw_numeric

``pw_numeric`` is a collection of mathematical utilities optimized for embedded
systems.

**What belongs in this module?**

Any standalone mathematical, numerical, or statisitical utilities that are
optimized for embedded belong in ``pw_numeric``. This encompasses a broad area,
but since the C++ standard library provides many mathematical utilities already,
this module is not anticipated to be too large. If the module does grow
substantially, some features may be moved to other modules (e.g.
``pw_statistics``).

**What does NOT belong in this module?**

- Features available in the C++ standard library (``<cmath>``, ``<numeric>``,
  ``<bit>``, etc.) should not be duplicated.
- Pseudo-random number generation belongs in :ref:`module-pw_random`.
- Data integrity checking belongs in :ref:`module-pw_checksum`.
- Bit / byte manipulation belong in :ref:`module-pw_bytes`.

-----------------
C++ API reference
-----------------
Moved: :cc:`pw_numeric`
