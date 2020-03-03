.. default-domain:: cpp

.. highlight:: sh

---------
pw_varint
---------
The pw_varint module provides functions for encoding and decoding variable
length integers, or varints. For smaller values, varints require less memory
than a fixed-size encoding. For example, a 32-bit (4-byte) integer requires 1--5
bytes when varint-encoded.

`Protocol Buffers <https://developers.google.com/protocol-buffers/docs/encoding#varints>`_
use a variable-length encoding for integers.

Compatibility
=============
* C
* C++11 (with :doc:`../pw_polyfill/docs`)

Dependencies
============
* pw_span
