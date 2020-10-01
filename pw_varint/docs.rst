.. _module-pw_varint:

---------
pw_varint
---------
The ``pw_varint`` module provides functions for encoding and decoding variable
length integers, or varints. For smaller values, varints require less memory
than a fixed-size encoding. For example, a 32-bit (4-byte) integer requires 1--5
bytes when varint-encoded.

`Protocol Buffers <https://developers.google.com/protocol-buffers/docs/encoding#varints>`_
use a variable-length encoding for integers.

Compatibility
=============
* C
* C++11 (with :doc:`../pw_polyfill/docs`)

API
===

.. cpp:function:: size_t EncodedSize(uint64_t integer)

Returns the size of an integer when encoded as a varint. Works on both signed
and unsigned integers.

.. cpp:function:: size_t ZigZagEncodedSize(int64_t integer)

Returns the size of a signed integer when ZigZag encoded as a varint.

Dependencies
============
* ``pw_span``
