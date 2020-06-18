.. _chapter-pw-bytes:

.. default-domain:: cpp

.. highlight:: sh

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

pw::ByteBuilder
-----------------
ByteBuilder is a utility class which facilitates the creation and
building of formatted bytes in a fixed-size buffer.

Utilities for building byte arrays at run time
------------------------------------------------
-``PutInt8``, ``PutUInt8``: Inserts 8-bit integers.
-``PutInt16``, ``PutInt16``: Inserts 16-bit integers in little/big endian.
-``PutInt32``, ``PutUInt32``: Inserts 32-bit integers in little/big endian.
-``PutInt64``, ``PutInt64``: Inserts 64-bit integers in little/big endian.

Future work
^^^^^^^^^^^
- Utilities for building byte arrays at compile time.
