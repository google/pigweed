.. _module-pw_bytes:

---------
pw_bytes
---------
pw_bytes is a collection of utilities for manipulating binary data.

Compatibility
=============
C++17

Dependencies
============
* ``pw_preprocessor``
* ``pw_status``
* ``pw_span``

Features
========

pw_bytes/array.h
----------------
Functions for working with byte arrays, primarily for building fixed-size byte
arrays at compile time.

pw_bytes/byte_builder.h
-----------------------
.. cpp:class:: ByteBuilder

  ``ByteBuilder`` is a class that facilitates building or reading arrays of
  bytes in a fixed-size buffer. ByteBuilder handles reading and writing integers
  with varying endianness.

.. cpp:class:: template <size_t max_size> ByteBuffer

  ``ByteBuilder`` with an internally allocated buffer.

Size report: using ByteBuffer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. include:: byte_builder_size_report

pw_bytes/endian.h
-----------------
Functions for converting the endianness of integral values.
