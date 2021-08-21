.. _module-pw_protobuf:

===========
pw_protobuf
===========
The protobuf module provides a lightweight interface for encoding and decoding
the Protocol Buffer wire format.

.. note::

  The protobuf module is a work in progress. Wire format encoding and decoding
  is supported, though the APIs are not final. C++ code generation exists for
  encoding, but not decoding.

Design
======
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

Configuration
=============
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

========
Encoding
========

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

Error Handling
==============
While individual write calls on a proto encoder return pw::Status objects, the
encoder tracks all status returns and "latches" onto the first error
encountered. This status can be accessed via ``StreamEncoder::status()``.

Codegen
=======
pw_protobuf encoder codegen integration is supported in GN, Bazel, and CMake.
The codegen is just a light wrapper around the ``StreamEncoder`` and
``MemoryEncoder`` objects, providing named helper functions to write proto
fields rather than requiring that field numbers are directly passed to an
encoder. Namespaced proto enums are also generated, and used as the arguments
when writing enum fields of a proto message.

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

========
Decoding
========

Size report
===========

Full size report
----------------

This report demonstrates the size of using the entire decoder with all of its
decode methods and a decode callback for a proto message containing each of the
protobuf field types.

.. include:: size_report/decoder_full


Incremental size report
-----------------------

This report is generated using the full report as a base and adding some int32
fields to the decode callback to demonstrate the incremental cost of decoding
fields in a message.

.. include:: size_report/decoder_incremental

========================================
Comparison with other protobuf libraries
========================================

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
