.. _module-pw_varint:

=========
pw_varint
=========
.. doxygenfile:: pw_varint/varint.h
   :sections: detaileddescription

-------------
Compatibility
-------------
* C
* C++
* `Rust </rustdoc/pw_varint>`_

-------------
API Reference
-------------

.. _module-pw_varint-api-c:

C
=
.. doxygendefine:: PW_VARINT_MAX_INT32_SIZE_BYTES
.. doxygendefine:: PW_VARINT_MAX_INT64_SIZE_BYTES
.. doxygenfunction:: pw_varint_Encode32
.. doxygenfunction:: pw_varint_Encode64
.. doxygenfunction:: pw_varint_Decode32
.. doxygenfunction:: pw_varint_Decode64
.. doxygenfunction:: pw_varint_ZigZagEncode32
.. doxygenfunction:: pw_varint_ZigZagEncode64
.. doxygenfunction:: pw_varint_ZigZagDecode32
.. doxygenfunction:: pw_varint_ZigZagDecode64
.. doxygendefine:: PW_VARINT_ENCODED_SIZE_BYTES
.. doxygenfunction:: pw_varint_EncodedSizeBytes
.. doxygenenum:: pw_varint_Format
.. doxygenfunction:: pw_varint_EncodeCustom
.. doxygenfunction:: pw_varint_DecodeCustom

C++
===
.. doxygenvariable:: pw::varint::kMaxVarint32SizeBytes
.. doxygenvariable:: pw::varint::kMaxVarint64SizeBytes
.. doxygenfunction:: pw::varint::ZigZagEncode
.. doxygenfunction:: pw::varint::ZigZagDecode
.. doxygenfunction:: pw::varint::EncodedSize
.. doxygenfunction:: pw::varint::EncodeLittleEndianBase128
.. doxygenfunction:: pw::varint::Encode(T integer, const span<std::byte> &output)
.. doxygenfunction:: pw::varint::Decode(const span<const std::byte>& input, int64_t* output)
.. doxygenfunction:: pw::varint::Decode(const span<const std::byte>& input, uint64_t* output)
.. doxygenfunction:: pw::varint::MaxValueInBytes(size_t bytes)
.. doxygenenum:: pw::varint::Format
.. doxygenfunction:: pw::varint::Encode(uint64_t value, span<std::byte> output, Format format)
.. doxygenfunction:: pw::varint::Decode(span<const std::byte> input, uint64_t* value, Format format)

Stream API
----------
.. doxygenfunction:: pw::varint::Read(stream::Reader& reader, uint64_t* output, size_t max_size)
.. doxygenfunction:: pw::varint::Read(stream::Reader& reader, int64_t* output, size_t max_size)

Rust
====
``pw_varint``'s Rust API is documented in our
`rustdoc API docs </rustdoc/pw_varint>`_.

------
Zephyr
------
To enable ``pw_varint`` for Zephyr add ``CONFIG_PIGWEED_VARINT=y`` to the
project's configuration.
