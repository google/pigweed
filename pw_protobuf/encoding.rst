.. _module-pw_protobuf-encoding:

--------------------
pw_protobuf Encoding
--------------------

Usage
=====
Pigweed's protobuf encoders encode directly to the wire format of a proto rather
than staging information to a mutable datastructure. This means any writes of a
value are final, and can't be referenced or modified as a later step in the
encode process.

MemoryEncoder
-------------
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
-------------
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
------------------
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

  StreamEncoder nested_encoder =
      my_proto_encoder.GetNestedEncoder(kPetsFieldNumber);

  // There's no intermediate buffering when writing a string directly to a
  // StreamEncoder.
  nested_encoder.WriteString(kNameFieldNumber, "Spot");
  nested_encoder.WriteString(kPetTypeFieldNumber, "dog");
  // Since this message is only nested one deep, the submessage is serialized to
  // the writer as soon as we finalize the submessage.
  PW_CHECK_OK(nested_encoder.Finalize());

  {  // If a nested_encoder is destroyed it will silently Finalize().
    StreamEncoder nested_encoder_2 =
        my_proto_encoder.GetNestedEncoder(kPetsFieldNumber);
    nested_encoder_2.WriteString(kNameFieldNumber, "Slippers");
    nested_encoder_2.WriteString(kPetTypeFieldNumber, "rabbit");
  }  // When this scope ends, the nested encoder is serialized to the Writer.

  // If an encode error occurs when encoding the nested messages, it will be
  // reflected at the root encoder.
  if (!my_proto_encoder.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", my_proto_encoder.status().str());
  }

.. warning::
  When a nested submessage is created, any writes to the parent encoder that
  created the nested encoder will trigger a crash. To resume writing to
  a parent encoder, Finalize() the submessage encoder first.

Error Handling
--------------
While individual write calls on a proto encoder return pw::Status objects, the
encoder tracks all status returns and "latches" onto the first error
encountered. This status can be accessed via ``StreamEncoder::status()``.

Codegen
-------
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

  fuzzy_friends::Pet::StreamEncoder pet1 = client.GetPetsEncoder();

  pet1.WriteName("Spot");
  pet1.WritePetType("dog");
  PW_CHECK_OK(pet1.Finalize());

  {  // Since pet2 is scoped, it will automatically Finalize() on destruction.
    fuzzy_friends::Pet::StreamEncoder pet2 = client.GetPetsEncoder();
    pet2.WriteName("Slippers");
    pet2.WritePetType("rabbit");
  }

  if (!client.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", client.status().str());
  }
