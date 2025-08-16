.. _module-pw_varint:

=========
pw_varint
=========
.. pigweed-module::
   :name: pw_varint

The ``pw_varint`` module provides functions for encoding and decoding variable
length integers or varints. For smaller values, varints require less memory
than a fixed-size encoding. For example, a 32-bit (4-byte) integer requires
1–5 bytes when varint-encoded.

``pw_varint`` supports custom variable-length encodings with different
terminator bit values and positions (:doxylink:`pw::varint::Format`).
The basic encoding for unsigned integers is Little Endian Base 128 (LEB128).
ZigZag encoding is also supported, which maps negative integers to positive
integers to improve encoding density for LEB128.

.. _Protocol Buffers: https://developers.google.com/protocol-buffers/docs/encoding#varints

`Protocol Buffers`_ and :ref:`module-pw_hdlc` use variable-length
integer encodings for integers.

-------------
Compatibility
-------------
* C
* C++
* `Rust </rustdoc/pw_varint>`_

-------------
API Reference
-------------

C/C++
=====
Moved: :doxylink:`pw_varint`

Rust
====
``pw_varint``'s Rust API is documented in our
`rustdoc API docs </rustdoc/pw_varint>`_.

------
Zephyr
------
To enable ``pw_varint`` for Zephyr add ``CONFIG_PIGWEED_VARINT=y`` to the
project's configuration.
