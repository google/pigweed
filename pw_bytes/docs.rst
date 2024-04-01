.. _module-pw_bytes:

=========
pw_bytes
=========
pw_bytes is a collection of utilities for manipulating binary data.

-------------
Compatibility
-------------
C++17

------------
Dependencies
------------
* ``pw_preprocessor``
* ``pw_status``
* ``pw_span``

--------
Features
--------

pw_bytes/alignment.h
====================
Functions for aligning sizes and addresses to memory alignment boundaries.

pw_bytes/array.h
================
Functions for working with byte arrays, primarily for building fixed-size byte
arrays at compile time.

pw_bytes/byte_builder.h
=======================
.. doxygenclass:: pw::ByteBuilder
   :members:

.. doxygenclass:: pw::ByteBuffer
   :members:

Size report: using ByteBuffer
-----------------------------
.. include:: byte_builder_size_report

pw_bytes/bit.h
================
Implementation of features provided by C++20's ``<bit>`` header. Supported
features:

* ``pw::endian`` -- Implementation of the ``std::endian`` enum. If
  ``std::endian`` is available, ``pw::endian`` is an alias of it.

* Additional functions for bit-level operations.

  .. doxygenfunction:: pw::bytes::SignExtend
  .. doxygenfunction:: pw::bytes::ExtractBits

pw_bytes/endian.h
=================
Functions for converting the endianness of integral values.

pw_bytes/suffix.h
=================
This module exports a single ``_b`` literal, making it easier to create
``std::byte`` values for tests.

.. cpp:function:: constexpr std::byte operator"" _b(unsigned long long value)

.. note::
   This should not be used in header files, as it requires a ``using``
   declaration that will be publicly exported at whatever level it is
   used.

Example:

.. code-block:: cpp

   #include "pw_bytes/units.h"

   using ::pw::operator""_b;

   constexpr std::byte kValue = 5_b;

pw_bytes/units.h
================
Constants, functions and user-defined literals for specifying a number of bytes
in powers of two, as defined by IEC 60027-2 A.2 and ISO/IEC 80000:13-2008.

The supported suffixes include:

* ``_B``   for bytes     (1024\ :sup:`0`)
* ``_KiB`` for kibibytes (1024\ :sup:`1`)
* ``_MiB`` for mebibytes (1024\ :sup:`2`)
* ``_GiB`` for gibibytes (1024\ :sup:`3`)
* ``_TiB`` for tebibytes (1024\ :sup:`4`)
* ``_PiB`` for pebibytes (1024\ :sup:`5`)
* ``_EiB`` for exbibytes (1024\ :sup:`6`)

In order to use these you must use a using namespace directive, for example:

.. code-block:: cpp

   #include "pw_bytes/units.h"

   using namespace pw::bytes::unit_literals;

   constexpr size_t kRandomBufferSizeBytes = 1_MiB + 42_KiB;

In some cases, the use of user-defined literals is not permitted because of the
required using namespace directive. One example of this is in header files,
where it is undesirable to pollute the namespace. For this situation, there are
also similar functions:

.. code-block:: cpp

   #include "pw_bytes/units.h"

   constexpr size_t kBufferSizeBytes = pw::bytes::MiB(1) + pw::bytes::KiB(42);

------
Zephyr
------
To enable ``pw_bytes`` for Zephyr add ``CONFIG_PIGWEED_BYTES=y`` to the
project's configuration.

--------
Rust API
--------
``pw_bytes``'s Rust API is documented in our
`rustdoc API docs </rustdoc/pw_bytes>`_.
