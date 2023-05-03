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
* C++14 (with :doc:`../pw_polyfill/docs`)

API
===

.. doxygenfunction:: pw::varint::EncodedSize(uint64_t integer)

.. doxygenfunction:: pw::varint::ZigZagEncodedSize(int64_t integer)

.. cpp:function:: uint64_t MaxValueInBytes(size_t bytes)

Returns the maximum integer value that can be encoded as a varint into the
specified number of bytes.


Stream API
----------

.. doxygenfunction:: pw::varint::Read(stream::Reader& reader, uint64_t* output, size_t max_size)

.. doxygenfunction:: pw::varint::Read(stream::Reader& reader, int64_t* output, size_t max_size)

Dependencies
============
* ``pw_span``

Zephyr
======
To enable ``pw_varint`` for Zephyr add ``CONFIG_PIGWEED_VARINT=y`` to the
project's configuration.

Rust
====
The Rust implementation is documented in
`//pw_varint/rust/pw_varint.rs <https://pigweed.googlesource.com/pigweed/pigweed/+/refs/heads/main/pw_varint/rust/pw_varint.rs>`_

..
  TODO(b/280102965): Update above to point to rustdoc API docs
