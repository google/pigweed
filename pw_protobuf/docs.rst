.. _module-pw_protobuf:

===========
pw_protobuf
===========
The protobuf module provides a lightweight interface for encoding and decoding
the Protocol Buffer wire format.

.. note::

  The protobuf module is a work in progress. Wire format encoding and decoding
  is supported, though the APIs are not final. C++ code generation exists for
  encoding and decoding, but does not cover all message types.

------
Design
------
Unlike other protobuf libraries, which typically provide in-memory data
structures to represent protobuf messages, ``pw_protobuf`` operates directly on
the wire format and leaves data storage to the user. This has a few benefits.
The primary one is that it allows the library to be incredibly small, with the
encoder and decoder each having a code size of around 1.5K and negligible RAM
usage. Users can choose the tradeoffs most suitable for their product on top of
this core implementation.

``pw_protobuf`` also provides zero-overhead C++ code generation which wraps its
low-level wire format operations with a user-friendly API for processing
specific protobuf messages. The code generation integrates with Pigweed's GN
build system.

-------------
Configuration
-------------
``pw_protobuf`` supports the following configuration options.

* ``PW_PROTOBUF_CFG_MAX_VARINT_SIZE``:
  When encoding nested messages, the number of bytes to reserve for the varint
  submessage length. Nested messages are limited in size to the maximum value
  that can be varint-encoded into this reserved space.

  The values that can be set, and their corresponding maximum submessage
  lengths, are outlined below.

  +-------------------+----------------------------------------+
  | MAX_VARINT_SIZE   | Maximum submessage length              |
  +===================+========================================+
  | 1 byte            | 127                                    |
  +-------------------+----------------------------------------+
  | 2 bytes           | 16,383 or < 16KiB                      |
  +-------------------+----------------------------------------+
  | 3 bytes           | 2,097,151 or < 2048KiB                 |
  +-------------------+----------------------------------------+
  | 4 bytes (default) | 268,435,455 or < 256MiB                |
  +-------------------+----------------------------------------+
  | 5 bytes           | 4,294,967,295 or < 4GiB (max uint32_t) |
  +-------------------+----------------------------------------+

--------
Encoding
--------

Usage
=====
Pigweed's protobuf encoders encode directly to the wire format of a proto rather
than staging information to a mutable datastructure. This means any writes of a
value are final, and can't be referenced or modified as a later step in the
encode process.

MemoryEncoder
=============
A MemoryEncoder directly encodes a proto to an in-memory buffer.

.. Code:: cpp

  // Writes a proto response to the provided buffer, returning the encode
  // status and number of bytes written.
  StatusWithSize WriteProtoResponse(ByteSpan response) {
    // All proto writes are directly written to the `response` buffer.
    MemoryEncoder encoder(response);
    encoder.WriteUint32(kMagicNumberField, 0x1a1a2b2b);
    encoder.WriteString(kFavoriteFood, "cookies");
    return StatusWithSize(encoder.status(), encoder.size());
  }

StreamEncoder
=============
pw_protobuf's StreamEncoder class operates on pw::stream::Writer objects to
serialized proto data. This means you can directly encode a proto to something
like pw::sys_io without needing to build the complete message in memory first.

.. Code:: cpp

  #include "pw_protobuf/encoder.h"
  #include "pw_stream/sys_io_stream.h"
  #include "pw_bytes/span.h"

  pw::stream::SysIoWriter sys_io_writer;
  pw::protobuf::StreamEncoder my_proto_encoder(sys_io_writer,
                                                  pw::ByteSpan());

  // Once this line returns, the field has been written to the Writer.
  my_proto_encoder.WriteInt64(kTimestampFieldNumber, system::GetUnixEpoch());

  // There's no intermediate buffering when writing a string directly to a
  // StreamEncoder.
  my_proto_encoder.WriteString(kWelcomeMessageFieldNumber,
                               "Welcome to Pigweed!");
  if (!my_proto_encoder.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", my_proto_encoder.status().str());
  }

Nested submessages
==================
Writing proto messages with nested submessages requires buffering due to
limitations of the proto format. Every proto submessage must know the size of
the submessage before its final serialization can begin. A streaming encoder can
be passed a scratch buffer to use when constructing nested messages. All
submessage data is buffered to this scratch buffer until the submessage is
finalized. Note that the contents of this scratch buffer is not necessarily
valid proto data, so don't try to use it directly.

MemoryEncoder objects use the final destination buffer rather than relying on a
scratch buffer. Note that this means your destination buffer might need
additional space for overhead incurred by nesting submessages. The
``MaxScratchBufferSize()`` helper function can be useful in estimating how much
space to allocate to account for nested submessage encoding overhead.

.. Code:: cpp

  #include "pw_protobuf/encoder.h"
  #include "pw_stream/sys_io_stream.h"
  #include "pw_bytes/span.h"

  pw::stream::SysIoWriter sys_io_writer;
  // The scratch buffer should be at least as big as the largest nested
  // submessage. It's a good idea to be a little generous.
  std::byte submessage_scratch_buffer[64];

  // Provide the scratch buffer to the proto encoder. The buffer's lifetime must
  // match the lifetime of the encoder.
  pw::protobuf::StreamEncoder my_proto_encoder(sys_io_writer,
                                               submessage_scratch_buffer);

  {
    // Note that the parent encoder, my_proto_encoder, cannot be used until the
    // nested encoder, nested_encoder, has been destroyed.
    StreamEncoder nested_encoder =
        my_proto_encoder.GetNestedEncoder(kPetsFieldNumber);

    // There's intermediate buffering when writing to a nested encoder.
    nested_encoder.WriteString(kNameFieldNumber, "Spot");
    nested_encoder.WriteString(kPetTypeFieldNumber, "dog");

    // When this scope ends, the nested encoder is serialized to the Writer.
    // In addition, the parent encoder, my_proto_encoder, can be used again.
  }

  // If an encode error occurs when encoding the nested messages, it will be
  // reflected at the root encoder.
  if (!my_proto_encoder.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", my_proto_encoder.status().str());
  }

.. warning::
  When a nested submessage is created, any use of the parent encoder that
  created the nested encoder will trigger a crash. To resume using the parent
  encoder, destroy the submessage encoder first.

Repeated Fields
===============
Repeated fields can be encoded a value at a time by repeatedly calling
`WriteInt32` etc., or as a packed field by calling e.g. `WritePackedInt32` with
a `std::span<Type>` or `WriteRepeatedInt32` with a `pw::Vector<Type>` (see
:ref:`module-pw_containers` for details).

Error Handling
==============
While individual write calls on a proto encoder return pw::Status objects, the
encoder tracks all status returns and "latches" onto the first error
encountered. This status can be accessed via ``StreamEncoder::status()``.

Proto map encoding utils
========================

Some additional helpers for encoding more complex but common protobuf
submessages (e.g. map<string, bytes>) are provided in
``pw_protobuf/map_utils.h``.

.. Note::
  The helper API are currently in-development and may not remain stable.

--------
Decoding
--------
``pw_protobuf`` provides three decoder implementations, which are described
below.

Decoder
=======
The ``Decoder`` class operates on an protobuf message located in a buffer in
memory. It provides an iterator-style API for processing a message. Calling
``Next()`` advances the decoder to the next proto field, which can then be read
by calling the appropriate ``Read*`` function for the field number.

When reading ``bytes`` and ``string`` fields, the decoder returns a view of that
field within the buffer; no data is copied out.

.. note::

  ``pw::protobuf::Decoder`` will soon be renamed ``pw::protobuf::MemoryDecoder``
  for clarity and consistency.

.. code-block:: c++

  #include "pw_protobuf/decoder.h"
  #include "pw_status/try.h"

  pw::Status DecodeProtoFromBuffer(std::span<const std::byte> buffer) {
    pw::protobuf::Decoder decoder(buffer);
    pw::Status status;

    uint32_t uint32_field;
    std::string_view string_field;

    // Iterate over the fields in the message. A return value of OK indicates
    // that a valid field has been found and can be read. When the decoder
    // reaches the end of the message, Next() will return OUT_OF_RANGE.
    // Other return values indicate an error trying to decode the message.
    while ((status = decoder.Next()).ok()) {
      switch (decoder.FieldNumber()) {
        case 1:
          PW_TRY(decoder.ReadUint32(&uint32_field));
          break;
        case 2:
          // The passed-in string_view will point to the contents of the string
          // field within the buffer.
          PW_TRY(decoder.ReadString(&string_field));
          break;
      }
    }

    // Do something with the fields...

    return status.IsOutOfRange() ? OkStatus() : status;
  }

StreamDecoder
=============
Sometimes, a serialized protobuf message may be too large to fit into an
in-memory buffer. To faciliate working with that type of data, ``pw_protobuf``
provides a ``StreamDecoder`` which reads data from a ``pw::stream::Reader``.

.. admonition:: When to use a stream decoder

  The ``StreamDecoder`` should only be used in cases where the protobuf data
  cannot be read directly from a buffer. It is unadvisable to use a
  ``StreamDecoder`` with a ``MemoryStream`` --- the decoding operations will be
  far less efficient than the ``Decoder``, which is optimized for in-memory
  messages.

The general usage of a ``StreamDecoder`` is similar to the basic ``Decoder``,
with the exception of ``bytes`` and ``string`` fields, which must be copied out
of the stream into a provided buffer.

.. code-block:: c++

  #include "pw_protobuf/decoder.h"
  #include "pw_status/try.h"

  pw::Status DecodeProtoFromStream(pw::stream::Reader& reader) {
    pw::protobuf::StreamDecoder decoder(reader);
    pw::Status status;

    uint32_t uint32_field;
    char string_field[16];

    // Iterate over the fields in the message. A return value of OK indicates
    // that a valid field has been found and can be read. When the decoder
    // reaches the end of the message, Next() will return OUT_OF_RANGE.
    // Other return values indicate an error trying to decode the message.
    while ((status = decoder.Next()).ok()) {
      // FieldNumber() returns a Result<uint32_t> as it may fail sometimes.
      // However, FieldNumber() is guaranteed to be valid after a call to Next()
      // that returns OK, so the value can be used directly here.
      switch (decoder.FieldNumber().value()) {
        case 1: {
          Result<uint32_t> result = decoder.ReadUint32();
          if (result.ok()) {
            uint32_field = result.value();
          }
          break;
        }

        case 2:
          // The string field is copied into the provided buffer. If the buffer
          // is too small to fit the string, RESOURCE_EXHAUSTED is returned and
          // the decoder is not advanced, allowing the field to be re-read.
          PW_TRY(decoder.ReadString(string_field));
          break;
      }
    }

    // Do something with the fields...

    return status.IsOutOfRange() ? OkStatus() : status;
  }

Where the length of the protobuf message is known in advance, the decoder can
be prevented from reading from the stream beyond the known bounds by specifying
the known length to the decoder:

.. code-block:: c++

  pw::protobuf::StreamDecoder decoder(reader, message_length);

When a decoder constructed in this way goes out of scope, it will consume any
remaining bytes in `message_length` allowing the next `Read` to be data after
the protobuf, even when it was not fully parsed.

The ``StreamDecoder`` can also return a ``StreamDecoder::BytesReader`` for
reading bytes fields, avoiding the need to copy data out directly.

.. code-block:: c++

  if (decoder.FieldNumber() == 3) {
    // bytes my_bytes_field = 3;
    pw::protobuf::StreamDecoder::BytesReader bytes_reader =
        decoder.GetBytesReader();

    // Read data incrementally through the bytes_reader. While the reader is
    // active, any attempts to use the decoder will result in a crash. When the
    // reader goes out of scope, it will close itself and reactive the decoder.
  }

This reader supports seeking only if the ``StreamDecoder``'s reader supports
seeking.

If the current field is a nested protobuf message, the ``StreamDecoder`` can
provide a decoder for the nested message. While the nested decoder is active,
its parent decoder cannot be used.

.. code-block:: c++

  if (decoder.FieldNumber() == 4) {
    pw::protobuf::StreamDecoder nested_decoder = decoder.GetNestedDecoder();

    while (nested_decoder.Next().ok()) {
      // Process the nested message.
    }

    // Once the nested decoder goes out of scope, it closes itself, and the
    // parent decoder can be used again.
  }

Repeated Fields
---------------
The ``StreamDecoder`` supports two encoded forms of repeated fields: value at a
time, by repeatedly calling `ReadInt32` etc., and packed fields by calling
e.g. `ReadPackedInt32`.

Since protobuf encoders are permitted to choose either format, including
splitting repeated fields up into multiple packed fields, ``StreamDecoder``
also provides method `ReadRepeatedInt32` etc. methods that accept a
``pw::Vector`` (see :ref:`module-pw_containers` for details). These methods
correctly extend the vector for either encoding.

Message
=======

The module implements a message parsing class ``Message``, in
``pw_protobuf/message.h``, to faciliate proto message parsing and field access.
The class provides interfaces for searching fields in a proto message and
creating helper classes for it according to its interpreted field type, i.e.
uint32, bytes, string, map<>, repeated etc. The class works on top of
``StreamDecoder`` and thus requires a ``pw::stream::SeekableReader`` for proto
message access. The following gives examples for using the class to process
different fields in a proto message:

.. code-block:: c++

  // Consider the proto messages defined as follows:
  //
  // message Nested {
  //   string nested_str = 1;
  //   bytes nested_bytes = 2;
  // }
  //
  // message {
  //   uint32 integer = 1;
  //   string str = 2;
  //   bytes bytes = 3;
  //   Nested nested = 4;
  //   repeated string rep_str = 5;
  //   repeated Nested rep_nested  = 6;
  //   map<string, bytes> str_to_bytes = 7;
  //   map<string, Nested> str_to_nested = 8;
  // }

  // Given a seekable `reader` that reads the top-level proto message, and
  // a <proto_size> that gives the size of the proto message:
  Message message(reader, proto_size);

  // Parse a proto integer field
  Uint32 integer = messasge_parser.AsUint32(1);
  if (!integer.ok()) {
    // handle parsing error. i.e. return integer.status().
  }
  uint32_t integer_value = integer.value(); // obtained the value

  // Parse a string field
  String str = message.AsString(2);
  if (!str.ok()) {
    // handle parsing error. i.e. return str.status();
  }

  // check string equal
  Result<bool> str_check = str.Equal("foo");

  // Parse a bytes field
  Bytes bytes = message.AsBytes(3);
  if (!bytes.ok()) {
    // handle parsing error. i.e. return bytes.status();
  }

  // Get a reader to the bytes.
  stream::IntervalReader bytes_reader = bytes.GetBytesReader();

  // Parse nested message `Nested nested = 4;`
  Message nested = message.AsMessage(4).
  // Get the fields in the nested message.
  String nested_str = nested.AsString(1);
  Bytes nested_bytes = nested.AsBytes(2);

  // Parse repeated field `repeated string rep_str = 5;`
  RepeatedStrings rep_str = message.AsRepeatedString(5);
  // Iterate through the entries. If proto is malformed when
  // iterating, the next element (`str` in this case) will be invalid
  // and loop will end in the iteration after.
  for (String element : rep_str) {
    // Check status
    if (!str.ok()) {
      // In the case of error, loop will end in the next iteration if
      // continues. This is the chance for code to catch the error.
    }
    // Process str
  }

  // Parse repeated field `repeated Nested rep_nested = 6;`
  RepeatedStrings rep_str = message.AsRepeatedString(6);
  // Iterate through the entries. For iteration
  for (Message element : rep_rep_nestedstr) {
    // Check status
    if (!element.ok()) {
      // In the case of error, loop will end in the next iteration if
      // continues. This is the chance for code to catch the error.
    }
    // Process element
  }

  // Parse map field `map<string, bytes> str_to_bytes = 7;`
  StringToBytesMap str_to_bytes = message.AsStringToBytesMap(7);
  // Access the entry by a given key value
  Bytes bytes_for_key = str_to_bytes["key"];
  // Or iterate through map entries
  for (StringToBytesMapEntry entry : str_to_bytes) {
    // Check status
    if (!entry.ok()) {
      // In the case of error, loop will end in the next iteration if
      // continues. This is the chance for code to catch the error.
    }
    String key = entry.Key();
    Bytes value = entry.Value();
    // process entry
  }

  // Parse map field `map<string, Nested> str_to_nested = 8;`
  StringToMessageMap str_to_nested = message.AsStringToBytesMap(8);
  // Access the entry by a given key value
  Message nested_for_key = str_to_nested["key"];
  // Or iterate through map entries
  for (StringToMessageMapEntry entry : str_to_nested) {
    // Check status
    if (!entry.ok()) {
      // In the case of error, loop will end in the next iteration if
      // continues. This is the chance for code to catch the error.
      // However it is still recommended that the user breaks here.
      break;
    }
    String key = entry.Key();
    Message value = entry.Value();
    // process entry
  }

The methods in ``Message`` for parsing a single field, i.e. everty `AsXXX()`
method except AsRepeatedXXX() and AsStringMapXXX(), internally performs a
linear scan of the entire proto message to find the field with the given
field number. This can be expensive if performed multiple times, especially
on slow reader. The same applies to the ``operator[]`` of StringToXXXXMap
helper class. Therefore, for performance consideration, whenever possible, it
is recommended to use the following for-range style to iterate and process
single fields directly.


.. code-block:: c++

  for (Message::Field field : message) {
    // Check status
    if (!field.ok()) {
      // In the case of error, loop will end in the next iteration if
      // continues. This is the chance for code to catch the error.
    }
    if (field.field_number() == 1) {
      Uint32 integer = field.As<Uint32>();
      ...
    } else if (field.field_number() == 2) {
      String str = field.As<String>();
      ...
    } else if (field.field_number() == 3) {
      Bytes bytes = field.As<Bytes>();
      ...
    } else if (field.field_number() == 4) {
      Message nested = field.As<Message>();
      ...
    }
  }


.. Note::
  The helper API are currently in-development and may not remain stable.

-------
Codegen
-------

pw_protobuf codegen integration is supported in GN, Bazel, and CMake.
The codegen is a light wrapper around the ``StreamEncoder``, ``MemoryEncoder``,
and ``StreamDecoder`` objects, providing named helper functions to write and
read proto fields rather than requiring that field numbers are directly passed
to an encoder.

All generated messages provide a ``Fields`` enum that can be used directly for
out-of-band encoding, or with the ``pw::protobuf::Decoder``.

This module's codegen is available through the ``*.pwpb`` sub-target of a
``pw_proto_library`` in GN, CMake, and Bazel. See :ref:`pw_protobuf_compiler's
documentation <module-pw_protobuf_compiler>` for more information on build
system integration for pw_protobuf codegen.

Example ``BUILD.gn``:

.. Code:: none

  import("//build_overrides/pigweed.gni")

  import("$dir_pw_build/target_types.gni")
  import("$dir_pw_protobuf_compiler/proto.gni")

  # This target controls where the *.pwpb.h headers end up on the include path.
  # In this example, it's at "pet_daycare_protos/client.pwpb.h".
  pw_proto_library("pet_daycare_protos") {
    sources = [
      "pet_daycare_protos/client.proto",
    ]
  }

  pw_source_set("example_client") {
    sources = [ "example_client.cc" ]
    deps = [
      ":pet_daycare_protos.pwpb",
      dir_pw_bytes,
      dir_pw_stream,
    ]
  }

  pw_source_set("example_server") {
    sources = [ "example_server.cc" ]
    deps = [
      ":pet_daycare_protos.pwpb",
      dir_pw_bytes,
      dir_pw_stream,
    ]
  }

Example ``pet_daycare_protos/client.proto``:

.. Code:: none

  syntax = "proto3";
  // The proto package controls the namespacing of the codegen. If this package
  // were fuzzy.friends, the namespace for codegen would be fuzzy::friends::*.
  package fuzzy_friends;

  message Pet {
    string name = 1;
    string pet_type = 2;
  }

  message Client {
    repeated Pet pets = 1;
  }

Example ``example_client.cc``:

.. Code:: cpp

  #include "pet_daycare_protos/client.pwpb.h"
  #include "pw_protobuf/encoder.h"
  #include "pw_stream/sys_io_stream.h"
  #include "pw_bytes/span.h"

  pw::stream::SysIoWriter sys_io_writer;
  std::byte submessage_scratch_buffer[64];
  // The constructor is the same as a pw::protobuf::StreamEncoder.
  fuzzy_friends::Client::StreamEncoder client(sys_io_writer,
                                              submessage_scratch_buffer);
  {
    fuzzy_friends::Pet::StreamEncoder pet1 = client.GetPetsEncoder();
    pet1.WriteName("Spot");
    pet1.WritePetType("dog");
  }

  {
    fuzzy_friends::Pet::StreamEncoder pet2 = client.GetPetsEncoder();
    pet2.WriteName("Slippers");
    pet2.WritePetType("rabbit");
  }

  if (!client.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", client.status().str());
  }

Example ``example_server.cc``:

.. Code:: cpp

  #include "pet_daycare_protos/client.pwpb.h"
  #include "pw_protobuf/stream_decoder.h"
  #include "pw_stream/sys_io_stream.h"
  #include "pw_bytes/span.h"

  pw::stream::SysIoReader sys_io_reader;
  // The constructor is the same as a pw::protobuf::StreamDecoder.
  fuzzy_friends::Client::StreamDecoder client(sys_io_reader);
  while (client.Next().ok()) {
    switch (client.Field().value) {
      case fuzzy_friends::Client::Fields::PET: {
        std::array<char, 32> name{};
        std::array<char, 32> pet_type{};

        fuzzy_friends::Pet::StreamDecoder pet = client.GetPetsDecoder();
        while (pet.Next().ok()) {
          switch (pet.Field().value) {
            case fuzzy_friends::Pet::NAME:
              pet.ReadName(name);
              break;
            case fuzzy_friends::Pet::TYPE:
              pet.ReadPetType(pet_type);
              break;
          }
        }

        break;
      }
    }
  }

  if (!client.status().ok()) {
    PW_LOG_INFO("Failed to decode proto; %s", client.status().str());
  }

Enums
=====
Namespaced proto enums are generated, and used as the arguments when writing
enum fields of a proto message. When reading enum fields of a proto message,
the enum value is validated and returned as the correct type, or
``Status::DataLoss()`` if the decoded enum value was not given in the proto.

Repeated Fields
===============
For encoding, the wrappers provide a `WriteFieldName` method with three
signatures. One that encodes a single value at a time, one that encodes a packed
field from a `std::span<Type>`, and one that encodes a packed field from a
`pw::Vector<Type>`. All three return `Status`.

For decoding, the wrappers provide a `ReadFieldName` method with three
signatures. One that reads a single value at a time, returning a `Result<Type>`,
one that reads a packed field into a `std::span<Type>` and returning a
`StatusWithSize`, and one that supports all formats reading into a
`pw::Vector<Type>` and returning `Status`.

-----------
Size report
-----------

Full size report
================

This report demonstrates the size of using the entire decoder with all of its
decode methods and a decode callback for a proto message containing each of the
protobuf field types.

.. include:: size_report/decoder_full


Incremental size report
=======================

This report is generated using the full report as a base and adding some int32
fields to the decode callback to demonstrate the incremental cost of decoding
fields in a message.

.. include:: size_report/decoder_incremental

---------------------------
Serialized size calculation
---------------------------
``pw_protobuf/serialized_size.h`` provides a set of functions for calculating
how much memory serialized protocol buffer fields require. The
``kMaxSizeBytes*`` variables provide the maximum encoded sizes of each field
type. The ``SizeOfField*()`` functions calculate the encoded size of a field of
the specified type, given a particular key and, for variable length fields
(varint or delimited), a value. The ``SizeOf*Field`` functions calculate the
encoded size of fields with a particular wire format (delimited, varint).

--------------------------
Available protobuf modules
--------------------------
There are a handful of messages ready to be used in Pigweed projects. These are
located in ``pw_protobuf/pw_protobuf_protos``.

common.proto
============
Contains Empty message proto used in many RPC calls.


status.proto
============
Contains the enum for pw::Status.

.. Note::
 ``pw::protobuf::StatusCode`` values should not be used outside of a .proto
 file. Instead, the StatusCodes should be converted to the Status type in the
 language. In C++, this would be:

  .. code-block:: c++

    // Reading from a proto
    pw::Status status = static_cast<pw::Status::Code>(proto.status_field));
    // Writing to a proto
    proto.status_field = static_cast<pw::protobuf::StatusCode>(status.code()));

----------------------------------------
Comparison with other protobuf libraries
----------------------------------------

protobuf-lite
=============
protobuf-lite is the official reduced-size C++ implementation of protobuf. It
uses a restricted subset of the protobuf library's features to minimize code
size. However, is is still around 150K in size and requires dynamic memory
allocation, making it unsuitable for many embedded systems.

nanopb
======
`nanopb <https://github.com/nanopb/nanopb>`_ is a commonly used embedded
protobuf library with very small code size and full code generation. It provides
both encoding/decoding functionality and in-memory C structs representing
protobuf messages.

nanopb works well for many embedded products; however, using its generated code
can run into RAM usage issues when processing nontrivial protobuf messages due
to the necessity of defining a struct capable of storing all configurations of
the message, which can grow incredibly large. In one project, Pigweed developers
encountered an 11K struct statically allocated for a single message---over twice
the size of the final encoded output! (This was what prompted the development of
``pw_protobuf``.)

To avoid this issue, it is possible to use nanopb's low-level encode/decode
functions to process individual message fields directly, but this loses all of
the useful semantics of code generation. ``pw_protobuf`` is designed to optimize
for this use case; it allows for efficient operations on the wire format with an
intuitive user interface.

Depending on the requirements of a project, either of these libraries could be
suitable.
