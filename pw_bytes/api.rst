.. _module-pw_bytes-api:

=============
API reference
=============
.. pigweed-module-subpage::
   :name: pw_bytes

--------
Overview
--------
This module provides utilities for manipulating binary data, including:

-  **Packed Pointers**: Store data in unused pointer bits.
-  **Alignment**: Functions for memory alignment.
-  **Byte arrays**: Compile-time construction of byte arrays.
-  **Byte builders**: Dynamically build byte sequences in fixed-size buffers.
-  **Bit manipulation**: C++20 ``<bit>`` features and extensions.
-  **Endian conversion**: Utilities for handling byte order.
-  **Byte suffixes**: Convenient literal for ``std::byte``.
-  **Byte units**: Constants and literals for byte sizes, e.g. KiB, MiB, etc.
-  **Byte spans**: Aliases for ``pw::span`` of bytes.

---------------------
pw_bytes/packed_ptr.h
---------------------
.. doxygenfile:: pw_bytes/packed_ptr.h
   :sections: briefdescription
.. doxygenclass:: pw::PackedPtr
   :members:

--------------------
pw_bytes/alignment.h
--------------------
.. doxygenfile:: pw_bytes/alignment.h
   :sections: briefdescription

.. doxygenfunction:: pw::IsAlignedAs(const void* ptr, size_t alignment)

.. doxygenfunction:: pw::IsAlignedAs(const void* ptr)

.. doxygenfunction:: pw::AlignDown(uintptr_t value, size_t alignment)

.. doxygenfunction:: pw::AlignDown(T* value, size_t alignment)

.. doxygenfunction:: pw::AlignUp(uintptr_t value, size_t alignment)

.. doxygenfunction:: pw::AlignUp(T* value, size_t alignment)

.. doxygenfunction:: pw::Padding

.. doxygenfunction:: pw::GetAlignedSubspan

----------------
pw_bytes/array.h
----------------
.. doxygenfile:: pw_bytes/array.h
   :sections: briefdescription

.. doxygenfunction:: pw::bytes::Concat

.. doxygenfunction:: pw::bytes::String(const char (&str)[kSize])

.. doxygenfunction:: pw::bytes::String(const char (&)[1])

.. doxygenfunction:: pw::bytes::MakeArray

.. _//pw_bytes/public/pw_bytes/array.h: https://cs.opensource.google/pigweed/pigweed/+/main:pw_bytes/public/pw_bytes/array.h

.. note::

   This reference is incomplete. See `//pw_bytes/public/pw_bytes/array.h`_ to
   view the full API.

-----------------------
pw_bytes/byte_builder.h
-----------------------
.. doxygenfile:: pw_bytes/byte_builder.h
   :sections: briefdescription

.. doxygenclass:: pw::ByteBuilder
   :members:

.. doxygenclass:: pw::ByteBuffer
   :members:

--------------
pw_bytes/bit.h
--------------
.. doxygenfile:: pw_bytes/bit.h
   :sections: briefdescription

.. doxygenfunction:: pw::bytes::SignExtend

.. doxygenfunction:: pw::bytes::ExtractBits

-----------------
pw_bytes/endian.h
-----------------
.. doxygenfile:: pw_bytes/endian.h
   :sections: briefdescription

.. doxygengroup:: pw_bytes_endian
   :content-only:
   :members:

-----------------
pw_bytes/suffix.h
-----------------
.. doxygenfile:: pw_bytes/suffix.h
   :sections: briefdescription

.. cpp:function:: constexpr std::byte pw::operator""_b(unsigned long long value)

----------------
pw_bytes/units.h
----------------
.. doxygenfile:: pw_bytes/units.h
   :sections: briefdescription

Constants like ``pw::bytes::kBytesInKibibyte`` are documented within the header.

Functions for specifying units:

.. doxygenfunction:: pw::bytes::B

.. doxygenfunction:: pw::bytes::KiB

.. doxygenfunction:: pw::bytes::MiB

.. doxygenfunction:: pw::bytes::GiB

.. doxygenfunction:: pw::bytes::TiB

.. doxygenfunction:: pw::bytes::PiB

.. doxygenfunction:: pw::bytes::EiB


User-defined literals (require ``using namespace pw::bytes::unit_literals;``):

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_B(unsigned long long int bytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_KiB(unsigned long long int kibibytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_MiB(unsigned long long int mebibytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_GiB(unsigned long long int gibibytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_TiB(unsigned long long int tebibytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_PiB(unsigned long long int pebibytes)

.. cpp:function:: constexpr unsigned long long int pw::bytes::unit_literals::operator""_EiB(unsigned long long int exbibytes)

-----------------
pw_bytes/span.h
-----------------
.. doxygenfile:: pw_bytes/span.h
   :sections: briefdescription

.. doxygentypedef:: pw::ByteSpan

.. doxygentypedef:: pw::ConstByteSpan

.. doxygenfunction:: pw::ObjectAsBytes

.. doxygenfunction:: pw::ObjectAsWritableBytes
