.. _module-pw_protobuf:

===========
pw_protobuf
===========
The protobuf module provides an expressive interface for encoding and decoding
the Protocol Buffer wire format with a lightweight code and data footprint.

.. note::

  The protobuf module is a work in progress. Wire format encoding and decoding
  is supported, though the APIs are not final. C++ code generation exists for
  encoding and decoding, but not yet optimized for in-memory decoding.

--------
Overview
--------
Unlike protobuf libraries which require protobuf messages be represented by
in-memory data structures, ``pw_protobuf`` provides a progressive flexible API
that allows the user to choose the data storage format and tradeoffs most
suitable for their product on top of the implementation.

The API is designed in three layers, which can be freely intermixed with each
other in your code, depending on point of use requirements:

 1. Message Structures,
 2. Per-Field Writers and Readers,
 3. Direct Writers and Readers.

This has a few benefits. The primary one is that it allows the library to be
incredibly small, with the encoder and decoder each having a code size of
around 1.5K and negligible RAM usage.

To demonstrate these layers, we use the following protobuf message definition
in the examples:

.. code::

  message Customer {
    enum Status {
      NEW = 1;
      ACTIVE = 2;
      INACTIVE = 3;
    }
    int32 age = 1;
    string name = 2;
    Status status = 3;
  }

And the following accompanying options file:

.. code::

  Customer.name max_size:32

Message Structures
==================
The highest level API is based around message structures created through C++
code generation, integrated with Pigweed's build system.

This results in the following generated structure:

.. code:: c++

  enum class Customer::Status {
    NEW = 1,
    ACTIVE = 2,
    INACTIVE = 3
  };

  struct Customer::Message {
    int32_t age;
    pw::Vector<char, 32> name;
    Customer::Status status;
  };

Which can be encoded with the code:

.. code:: c++

  #include "example_protos/customer.pwpb.h"

  pw::Status EncodeCustomer(Customer::StreamEncoder& encoder) {
    return encoder.Write({
      age = 33,
      name = "Joe Bloggs",
      status = Customer::Status::INACTIVE
    });
  }

And decoded into a struct with the code:

.. code:: c++

  #include "example_protos/customer.pwpb.h"

  pw::Status DecodeCustomer(Customer::StreamDecoder& decoder) {
    Customer::Message customer{};
    PW_TRY(decoder.Read(customer));
    // Read fields from customer
    return pw::OkStatus();
  }

These structures can be moved, copied, and compared with each other for
equality.

The encoder and decoder code is generic and implemented in the core C++ module.
A small overhead for each message type used in your code describes the structure
to the generic encoder and decoders.

Per-Field Writers and Readers
=============================
The middle level API is based around typed methods to write and read each
field of the message directly to the final serialized form, again created
through C++ code generation.

Encoding
--------
Given the same message structure, in addition to the ``Write()`` method that
accepts a message structure, the following additional methods are also
generated in the typed ``StreamEncoder`` class.

There are lightweight wrappers around the core implementation, calling the
underlying methods with the correct field numbers and value types, and result
in no additional binary code over correctly using the core implementation.

.. code:: c++

  class Customer::StreamEncoder : pw::protobuf::StreamEncoder {
   public:
    // Message Structure Writer.
    pw::Status Write(const Customer::Message&);

    // Per-Field Typed Writers.
    pw::Status WriteAge(int32_t);

    pw::Status WriteName(std::string_view);
    pw::Status WriteName(const char*, size_t);

    pw::Status WriteStatus(Customer::Status);
  };

So the same encoding method could be written as:

.. code:: c++

  #include "example_protos/customer.pwpb.h"

  Status EncodeCustomer(Customer::StreamEncoder& encoder) {
    PW_TRY(encoder.WriteAge(33));
    PW_TRY(encoder.WriteName("Joe Bloggs"sv));
    PW_TRY(encoder.WriteStatus(Customer::Status::INACTIVE));
  }

Pigweed's protobuf encoders encode directly to the wire format of a proto rather
than staging information to a mutable datastructure. This means any writes of a
value are final, and can't be referenced or modified as a later step in the
encode process.

Decoding
--------
For decoding, in addition to the ``Read()`` method that populates a message
structure, the following additional methods are also generated in the typed
``StreamDecoder`` class.

.. code:: c++

  class Customer::StreamDecoder : pw::protobuf::StreamDecoder {
   public:
    // Message Structure Reader.
    pw::Status Read(Customer::Message&);

    // Returns the identity of the current field.
    ::pw::Result<Fields> Field();

    // Per-Field Typed Readers.
    pw::Result<int32_t> ReadAge();

    pw::StatusWithSize ReadName(std::span<char>);
    BytesReader GetNameReader(); // Read name as a stream of bytes.

    pw::Result<Customer::Status> ReadStatus();
  };

Complete and correct decoding requires looping through the fields, so is more
complex than encoding or using the message structure.

.. code:: c++

  pw::Status DecodeCustomer(Customer::StreamDecoder& decoder) {
    uint32_t age;
    char name[32];
    Customer::Status status;

    while ((status = decoder.Next()).ok()) {
      switch (decoder.Field().value()) {
        case Customer::Fields::AGE: {
          PW_TRY_ASSIGN(age, decoder.ReadAge());
          break;
        }
        case Customer::Fields::NAME: {
          PW_TRY(decoder.ReadName(name));
          break;
        }
        case Customer::Fields::STATUS: {
          PW_TRY_ASSIGN(status, decoder.ReadStatus());
          break;
        }
      }
    }

    return status.IsOutOfRange() ? OkStatus() : status;
  }

Direct Writers and Readers
==========================
The lowest level API is provided by the core C++ implementation, and requires
the caller to provide the correct field number and value types for encoding, or
check the same when decoding.

Encoding
--------
The two fundamental classes are ``MemoryEncoder`` which directly encodes a proto
to an in-memory buffer, and ``StreamEncoder`` that operates on
``pw::stream::Writer`` objects to serialize proto data.

``StreamEncoder`` allows you encode a proto to something like ``pw::sys_io``
without needing to build the complete message in memory

To encode the same message we've used in the examples thus far, we would use
the following parts of the core API:

.. code:: c++

  class pw::protobuf::StreamEncoder {
   public:
    Status WriteInt32(uint32_t field_number, int32_t);
    Status WriteUint32(uint32_t field_number, uint32_t);

    Status WriteString(uint32_t field_number, std::string_view);
    Status WriteString(uint32_t field_number, const char*, size_t);

    // And many other methods, see pw_protobuf/encoder.h
  };

Encoding the same message requires that we specify the field numbers, which we
can hardcode, or supplement using the C++ code generated ``Fields`` enum, and
cast the enumerated type.

.. code:: c++

  #include "pw_protobuf/encoder.h"
  #include "example_protos/customer.pwpb.h"

  Status EncodeCustomer(pw::protobuf::StreamEncoder& encoder) {
    PW_TRY(encoder.WriteInt32(static_cast<uint32_t>(Customer::Fields::AGE),
                              33));
    PW_TRY(encoder.WriteString(static_cast<uint32_t>(Customer::Fields::NAME),
                               "Joe Bloggs"sv));
    PW_TRY(encoder.WriteUint32(
        static_cast<uint32_t>(Customer::Fields::STATUS),
        static_cast<uint32_t>(Customer::Status::INACTIVE)));
  }

Decoding
--------
``StreamDecoder`` reads data from a ``pw::stream::Reader`` and mirrors the API
of the encoders.

To decode the same message we would use the following parts of the core API:

.. code:: c++

  class pw::protobuf::StreamDecoder {
   public:
    // Returns the identity of the current field.
    ::pw::Result<uint32_t> FieldNumber();

    Result<int32_t> ReadInt32();
    Result<uint32_t> ReadUint32();

    StatusWithSize ReadString(std::span<char>);

    // And many other methods, see pw_protobuf/stream_decoder.h
  };

As with the typed per-field API, complete and correct decoding requires looping
through the fields and checking the field numbers, along with casting types.

.. code:: c++

  pw::Status DecodeCustomer(pw::protobuf::StreamDecoder& decoder) {
    uint32_t age;
    char name[32];
    Customer::Status status;

    while ((status = decoder.Next()).ok()) {
      switch (decoder.FieldNumber().value()) {
        case static_cast<uint32_t>(Customer::Fields::AGE): {
          PW_TRY_ASSIGN(age, decoder.ReadInt32());
          break;
        }
        case static_cast<uint32_t>(Customer::Fields::NAME): {
          PW_TRY(decoder.ReadString(name));
          break;
        }
        case static_cast<uint32_t>(Customer::Fields::STATUS): {
          uint32_t status_value;
          PW_TRY_ASSIGN(status_value, decoder.ReadUint32());
          status = static_cast<Customer::Status>(status_value);
          break;
        }
      }
    }

    return status.IsOutOfRange() ? OkStatus() : status;
  }

-------
Codegen
-------
pw_protobuf codegen integration is supported in GN, Bazel, and CMake.

This module's codegen is available through the ``*.pwpb`` sub-target of a
``pw_proto_library`` in GN, CMake, and Bazel. See :ref:`pw_protobuf_compiler's
documentation <module-pw_protobuf_compiler>` for more information on build
system integration for pw_protobuf codegen.

Example ``BUILD.gn``:

.. code::

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

Options Files
=============
Code generation can be configured using a separate ``.options`` file placed
alongside the relevant ``.proto`` file.

The format of this file is a series of fully qualified field names, or patterns,
followed by one or more options. Lines starting with ``#`` or ``//`` are
comments, and blank lines are ignored.

Example:

.. code::

  // Set an option for a specific field.
  fuzzy_friends.Client.visit_dates max_count:16

  // Set options for multiple fields by wildcard matching.
  fuzzy_friends.Pet.* max_size:32

  // Set multiple options in one go.
  fuzzy_friends.Dog.paws max_count:4 fixed_count:true

Options files should be listed as ``inputs`` when defining ``pw_proto_library``,
e.g.

.. code::

  pw_proto_library("pet_daycare_protos") {
    sources = [
      "pet_daycare_protos/client.proto",
    ]
    inputs = [
      "pet_daycare_protos/client.options",
    ]
  }

Valid options are:

* ``max_count``:
  Maximum number of entries for repeated fields. When set, repeated scalar
  fields will use the ``pw::Vector`` container type instead of a callback.

* ``fixed_count``:
  Specified with ``max_count`` to use a fixed length ``std::array`` container
  instead of ``pw::Vector``.

* ``max_size``:
  Maximum size of `bytes` and `strings` fields. When set, these field types
  will use the ``pw::Vector`` container type instead of a callback.

* ``fixed_size``:
  Specified with ``max_size`` to use a fixed length ``std::array`` container
  instead of ``pw::Vector`` for `bytes` fields.

* ``use_callback``:
  Replaces the structure member for the field with a callback function even
  where a simpler type could be used. This can be useful to ignore fields, to
  stop decoding of complex structures if certain values are not as expected, or
  to provide special handling for nested messages.

.. admonition:: Rationale

  The choice of a separate options file, over embedding options within the proto
  file, are driven by the need for proto files to be shared across multiple
  contexts.

  A typical product would require the same proto be used on a hardware
  component, running Pigweed; a server-side component, running on a cloud
  platform; and an app component, running on a Phone OS.

  While related, each of these will likely have different source projects and
  build systems.

  Were the Pigweed options embedded in the protos, it would be necessary for
  both the cloud platform and Phone OS to be able to ``"import pigweed"`` ---
  and equivalently for options relevant to their platforms in the embedded
  software project.

------------------
Message Structures
------------------
The C++ code generator creates a ``struct Message`` for each protobuf message
that can hold the set of values encoded by it, following these rules.

* Scalar fields are represented by their appropriate C++ type.

  .. code::

    message Customer {
      int32 age = 1;
      uint32 birth_year = 2;
      sint64 rating = 3;
      bool is_active = 4;
    }

  .. code:: c++

    struct Customer::Message {
      int32_t age;
      uint32_t birth_year;
      int64_t rating;
      bool is_active;
    };

* Enumerations are represented by a code generated namespaced proto enum.

  .. code::

    message Award {
      enum Service {
        BRONZE = 1;
        SILVER = 2;
        GOLD = 3;
      }
      Service service = 1;
    }

  .. code:: c++

    struct Award::Message {
      Award::Service service;
    };

* Nested messages are represented by their own ``struct Message`` provided that
  a reference cycle does not exist.

  .. code::

    message Sale {
      Customer customer = 1;
      Product product = 2;
    }

  .. code:: c++

    struct Sale::Message {
      Customer::Message customer;
      Product::Message product;
    };

* Optional scalar fields are represented by the appropriate C++ type wrapped in
  ``std::optional``. Optional fields are not encoded when the value is not
  present.

  .. code::

    message Loyalty {
      optional int32 points = 1;
    }

  .. code:: c++

    struct Loyalty::Message {
      std::optional<int32_t> points;
    };


* Repeated scalar fields are represented by ``pw::Vector`` when the
  ``max_count`` option is set for that field, or by ``std::array`` when both
  ``max_count`` and ``fixed_count:true`` are set.

  .. code::

    message Register {
      repeated int32 cash_in = 1;
      repeated int32 cash_out = 2;
    }

  .. code::

    Register.cash_in max_count:32 fixed_count:true
    Register.cash_out max_count:64

  .. code:: c++

    struct Register::Message {
      std::array<int32_t, 32> cash_in;
      pw::Vector<int32_t, 64> cash_out;
    };

* `bytes` fields are represented by ``pw::Vector`` when the ``max_size`` option
  is set for that field, or by ``std::array`` when both ``max_size`` and
  ``fixed_size:true`` are set.

  .. code::

    message Product {
      bytes sku = 1;
      bytes serial_number = 2;
    }

  .. code::

    Product.sku max_size:8 fixed_size:true
    Product.serial_number max_size:64

  .. code:: c++

    struct Product::Message {
      std::array<std::byte, 8> sku;
      pw::Vector<std::byte, 64> serial_number;
    };

* `string` fields are represented by ``pw::Vector`` when the ``max_size`` option
  is set for that field. Since the size is provided, the string is not
  automatically null-terminated. :ref:`module-pw_string` provides utility
  methods to copy string data into and out of this vector.

  .. code::

    message Employee {
      string name = 1;
    }

  .. code::

    Employee.name max_size:128

  .. code:: c++

    struct Employee::Message {
      pw::Vector<char, 128> name;
    };

* Nested messages with a dependency cycle, repeated scalar fields without a
  ``max_count`` option set, `bytes` and `strings` fields without a ``max_size``
  option set, and repeated nested messages, repeated `bytes`, and repeated
  `strings` fields, are represented by a callback.

  You set the callback to a custom function for encoding or decoding
  before passing the structure to ``Write()`` or ``Read()`` appropriately.

  .. code::

    message Store {
      Store nearest_store = 1;
      repeated int32 employee_numbers = 2;
      string driections = 3;
      repeated string address = 4;
      repeated Employee employees = 5;
    }

  .. code::

    // No options set.

  .. code:: c++

    struct Store::Message {
      pw::protobuf::Callback<Store::StreamEncoder, Store::StreamDecoder> nearest_store;
      pw::protobuf::Callback<Store::StreamEncoder, Store::StreamDecoder> employee_numbers;
      pw::protobuf::Callback<Store::StreamEncoder, Store::StreamDecoder> directions;
      pw::protobuf::Callback<Store::StreamEncoder, Store::StreamDecoder> address;
      pw::protobuf::Callback<Store::StreamEncoder, Store::StreamDecoder> employees;
    };

Message structures can be copied, but doing so will clear any assigned
callbacks. To preserve functions applied to callbacks, ensure that the message
structure is moved.

Message structures can also be compared with each other for equality. This
includes all repeated and nested fields represented by value types, but does not
compare any field represented by a callback.

Overhead
========
A single encoder and decoder is used for these structures, with a one-time code
cost. When the code generator creates the ``struct Message``, it also creates
a description of this structure that the shared encoder and decoder use.

The cost of this description is a shared table for each protobuf message
definition used, with four words per field within the protobuf message, and an
addition word to store the size of the table.

--------
Encoding
--------
The simplest way to use ``MemoryEncoder`` to encode a proto is from its code
generated ``Message`` structure into an in-memory buffer.

.. code:: c++

  #include "my_protos/my_proto.pwpb.h"
  #include "pw_bytes/span.h"
  #include "pw_protobuf/encoder.h"
  #include "pw_status/status_with_size.h"
  #include "pw_string/vector.h"

  // Writes a proto response to the provided buffer, returning the encode
  // status and number of bytes written.
  pw::StatusWithSize WriteProtoResponse(pw::ByteSpan response) {
    MyProto::Message message{}
    message.magic_number = 0x1a1a2b2b;
    pw::string::Copy("cookies", message.favorite_food);
    message.calories = 600;

    // All proto writes are directly written to the `response` buffer.
    MyProto::MemoryEncoder encoder(response);
    encoder.Write(message);

    return pw::StatusWithSize(encoder.status(), encoder.size());
  }

All fields of a message are written, including those initialized to their
default values.

Alternatively, for example if only a subset of fields are required to be
encoded, fields can be written a field at a time through the code generated
or lower-level APIs. This can be more convenient if finer grained control or
other custom handling is required.

.. code:: c++

  #include "my_protos/my_proto.pwpb.h"
  #include "pw_bytes/span.h"
  #include "pw_protobuf/encoder.h"
  #include "pw_status/status_with_size.h"

  // Writes a proto response to the provided buffer, returning the encode
  // status and number of bytes written.
  pw::StatusWithSize WriteProtoResponse(pw::ByteSpan response) {
    // All proto writes are directly written to the `response` buffer.
    MyProto::MemoryEncoder encoder(response);
    encoder.WriteMagicNumber(0x1a1a2b2b);
    encoder.WriteFavoriteFood("cookies");
    // Only conditionally write calories.
    if (on_diet) {
      encoder.WriteCalories(600);
    }
    return pw::StatusWithSize(encoder.status(), encoder.size());
  }

StreamEncoder
=============
``StreamEncoder`` is constructed with the destination stream, and a scratch
buffer used to handle nested submessages.

.. code:: c++

  #include "my_protos/my_proto.pwpb.h"
  #include "pw_bytes/span.h"
  #include "pw_protobuf/encoder.h"
  #include "pw_stream/sys_io_stream.h"

  pw::stream::SysIoWriter sys_io_writer;
  MyProto::StreamEncoder encoder(sys_io_writer, pw::ByteSpan());

  // Once this line returns, the field has been written to the Writer.
  encoder.WriteTimestamp(system::GetUnixEpoch());

  // There's no intermediate buffering when writing a string directly to a
  // StreamEncoder.
  encoder.WriteWelcomeMessage("Welcome to Pigweed!");

  if (!encoder.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", encoder.status().str());
  }

Callbacks
=========
When using the ``Write()`` method with a ``struct Message``, certain fields may
require a callback function be set to encode the values for those fields.
Otherwise the values will be treated as an empty repeated field and not encoded.

The callback is called with the cursor at the field in question, and passed
a reference to the typed encoder that can write the required values to the
stream or buffer.

Callback implementations may use any level of API. For example a callback for a
nested submessage (with a dependency cycle, or repeated) can be implemented by
calling ``Write()`` on a nested encoder.

.. code:: c++

    Store::Message store{};
    store.employees.SetEncoder([](Store::StreamEncoder& encoder) {
      Employee::Message employee{};
      // Populate `employee`.
      return encoder.GetEmployeesEncoder().Write(employee);
    ));

Nested submessages
==================
Code generated ``GetFieldEncoder`` methods are provided that return a correctly
typed ``StreamEncoder`` or ``MemoryEncoder`` for the message.

.. code::

  message Owner {
    Animal pet = 1;
  }

Note that the accessor method is named for the field, while the returned encoder
is named for the message type.

.. cpp:function:: Animal::StreamEncoder Owner::StreamEncoder::GetPetEncoder()

A lower-level API method returns an untyped encoder, which only provides the
lower-level API methods. This can be moved to a typed encoder later.

.. cpp:function:: pw::protobuf::StreamEncoder pw::protobuf::StreamEncoder::GetNestedEncoder(uint32_t field_number)

.. warning::
  When a nested submessage is created, any use of the parent encoder that
  created the nested encoder will trigger a crash. To resume using the parent
  encoder, destroy the submessage encoder first.

Buffering
---------
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

.. code:: c++

  #include "my_protos/pets.pwpb.h"
  #include "pw_bytes/span.h"
  #include "pw_protobuf/encoder.h"
  #include "pw_stream/sys_io_stream.h"

  pw::stream::SysIoWriter sys_io_writer;
  // The scratch buffer should be at least as big as the largest nested
  // submessage. It's a good idea to be a little generous.
  std::byte submessage_scratch_buffer[64];

  // Provide the scratch buffer to the proto encoder. The buffer's lifetime must
  // match the lifetime of the encoder.
  Owner::StreamEncoder owner_encoder(sys_io_writer, submessage_scratch_buffer);

  {
    // Note that the parent encoder, owner_encoder, cannot be used until the
    // nested encoder, pet_encoder, has been destroyed.
    Animal::StreamEncoder pet_encoder = owner_encoder.GetPetEncoder();

    // There's intermediate buffering when writing to a nested encoder.
    pet_encoder.WriteName("Spot");
    pet_encoder.WriteType(Pet::Type::DOG);

    // When this scope ends, the nested encoder is serialized to the Writer.
    // In addition, the parent encoder, owner_encoder, can be used again.
  }

  // If an encode error occurs when encoding the nested messages, it will be
  // reflected at the root encoder.
  if (!owner_encoder.status().ok()) {
    PW_LOG_INFO("Failed to encode proto; %s", owner_encoder.status().str());
  }

Scalar Fields
=============
As shown, scalar fields are written using code generated ``WriteFoo``
methods that accept the appropriate type and automatically writes the correct
field number.

.. cpp:function:: Status MyProto::StreamEncoder::WriteFoo(T)

These can be freely intermixed with the lower-level API that provides a method
per field type, requiring that the field number be passed in. The code
generation includes a ``Fields`` enum to provide the field number values.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteUint64(uint32_t field_number, uint64_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteSint64(uint32_t field_number, int64_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteInt64(uint32_t field_number, int64_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteUint32(uint32_t field_number, uint32_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteSint32(uint32_t field_number, int32_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteInt32(uint32_t field_number, int32_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteFixed64(uint32_t field_number, uint64_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteFixed32(uint32_t field_number, uint64_t)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteDouble(uint32_t field_number, double)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteFloat(uint32_t field_number, float)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteBool(uint32_t field_number, bool)

The following two method calls are equivalent, where the first is using the
code generated API, and the second implemented by hand.

.. code:: c++

  my_proto_encoder.WriteAge(42);
  my_proto_encoder.WriteInt32(static_cast<uint32_t>(MyProto::Fields::AGE), 42);

Repeated Fields
---------------
For repeated scalar fields, multiple code generated ``WriteFoos`` methods
are provided.

.. cpp:function:: Status MyProto::StreamEncoder::WriteFoos(T)

  This writes a single unpacked value.

.. cpp:function:: Status MyProto::StreamEncoder::WriteFoos(std::span<const T>)
.. cpp:function:: Status MyProto::StreamEncoder::WriteFoos(const pw::Vector<T>&)

  These write a packed field containing all of the values in the provided span
  or vector.

These too can be freely intermixed with the lower-level API methods, both to
write a single value, or to write packed values from either a ``std::span`` or
``pw::Vector`` source.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedUint64(uint32_t field_number, std::span<const uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedUint64(uint32_t field_number, const pw::Vector<uint64_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedSint64(uint32_t field_number, std::span<const int64_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedSint64(uint32_t field_number, const pw::Vector<int64_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedInt64(uint32_t field_number, std::span<const int64_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedInt64(uint32_t field_number, const pw::Vector<int64_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedUint32(uint32_t field_number, std::span<const uint32_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedUint32(uint32_t field_number, const pw::Vector<uint32_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedSint32(uint32_t field_number, std::span<const int32_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedSint32(uint32_t field_number, const pw::Vector<int32_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedInt32(uint32_t field_number, std::span<const int32_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedInt32(uint32_t field_number, const pw::Vector<int32_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedFixed64(uint32_t field_number, std::span<const uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedFixed64(uint32_t field_number, const pw::Vector<uint64_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedFixed32(uint32_t field_number, std::span<const uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedFixed32(uint32_t field_number, const pw::Vector<uint64_t>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedDouble(uint32_t field_number, std::span<const double>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedDouble(uint32_t field_number, const pw::Vector<double>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedFloat(uint32_t field_number, std::span<const float>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedFloat(uint32_t field_number, const pw::Vector<float>&)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WritePackedBool(uint32_t field_number, std::span<const bool>)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteRepeatedBool(uint32_t field_number, const pw::Vector<bool>&)

The following two method calls are equivalent, where the first is using the
code generated API, and the second implemented by hand.

.. code:: c++

  constexpr std::array<int32_t, 5> numbers = { 4, 8, 15, 16, 23, 42 };

  my_proto_encoder.WriteNumbers(numbers);
  my_proto_encoder.WritePackedInt32(
      static_cast<uint32_t>(MyProto::Fields::NUMBERS),
      numbers);

Enumerations
============
Enumerations are written using code generated ``WriteEnum`` methods that
accept the code generated enumeration as the appropriate type and automatically
writes both the correct field number and corresponding value.

.. cpp:function:: Status MyProto::StreamEncoder::WriteEnum(MyProto::Enum)

To write enumerations with the lower-level API, you would need to cast both
the field number and value to the ``uint32_t`` type.

The following two methods are equivalent, where the first is code generated,
and the second implemented by hand.

.. code:: c++

  my_proto_encoder.WriteAward(MyProto::Award::SILVER);
  my_proto_encoder.WriteUint32(
      static_cast<uint32_t>(MyProto::Fields::AWARD),
      static_cast<uint32_t>(MyProto::Award::SILVER));

Repeated Fields
---------------
For repeated enum fields, multiple code generated ``WriteEnums`` methods
are provided.

.. cpp:function:: Status MyProto::StreamEncoder::WriteEnums(MyProto::Enums)

  This writes a single unpacked value.

.. cpp:function:: Status MyProto::StreamEncoder::WriteEnums(std::span<const MyProto::Enums>)
.. cpp:function:: Status MyProto::StreamEncoder::WriteEnums(const pw::Vector<MyProto::Enums>&)

  These write a packed field containing all of the values in the provided span
  or vector.

Their use is as scalar fields.

Strings
=======
Strings fields have multiple code generated methods provided.

.. cpp:function:: Status MyProto::StreamEncoder::WriteName(std::string_view)
.. cpp:function:: Status MyProto::StreamEncoder::WriteName(const char*, size_t)

These can be freely intermixed with the lower-level API methods.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteString(uint32_t field_number, std::string_view)
.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteString(uint32_t field_number, const char*, size_t)

A lower level API method is provided that can write a string from another
stream.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteStringFromStream(uint32_t field_number, stream::Reader& bytes_reader, size_t num_bytes, ByteSpan stream_pipe_buffer)

  The payload for the value is provided through the stream::Reader
  ``bytes_reader``. The method reads a chunk of the data from the reader using
  the ``stream_pipe_buffer`` and writes it to the encoder.

Bytes
=====
Bytes fields provide the ``WriteData`` code generated method.

.. cpp:function:: Status MyProto::StreamEncoder::WriteData(ConstByteSpan)

This can be freely intermixed with the lower-level API method.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteBytes(uint32_t field_number, ConstByteSpan)

And with the API method that can write bytes from another stream.

.. cpp:function:: Status pw::protobuf::StreamEncoder::WriteBytesFromStream(uint32_t field_number, stream::Reader& bytes_reader, size_t num_bytes, ByteSpan stream_pipe_buffer)

  The payload for the value is provided through the stream::Reader
  ``bytes_reader``. The method reads a chunk of the data from the reader using
  the ``stream_pipe_buffer`` and writes it to the encoder.

Error Handling
==============
While individual write calls on a proto encoder return ``pw::Status`` objects,
the encoder tracks all status returns and "latches" onto the first error
encountered. This status can be accessed via ``StreamEncoder::status()``.

Proto map encoding utils
========================
Some additional helpers for encoding more complex but common protobuf
submessages (e.g. ``map<string, bytes>``) are provided in
``pw_protobuf/map_utils.h``.

.. Note::
  The helper API are currently in-development and may not remain stable.

--------
Decoding
--------
The simplest way to use ``StreamDecoder`` is to decode a proto from the stream
into its code generated ``Message`` structure.

.. code:: c++

  #include "my_protos/my_proto.pwpb.h"
  #include "pw_protobuf/stream_decoder.h"
  #include "pw_status/status.h"
  #include "pw_stream/stream.h"

  pw::Status DecodeProtoFromStream(pw::stream::Reader& reader) {
    MyProto::Message message{};
    MyProto::StreamDecoder decoder(reader);
    decoder.Read(message);
    return decoder.status();
  }

In the case of errors, the decoding will stop and return with the cursor on the
field that caused the error. It is valid in some cases to inspect the error and
continue decoding by calling ``Read()`` again on the same structure, or fall
back to using the lower-level APIs.

Unknown fields in the wire encoding are skipped.

If finer-grained control is required, the ``StreamDecoder`` class provides an
iterator-style API for processing a message a field at a time where calling
``Next()`` advances the decoder to the next proto field.

.. cpp:function:: Status pw::protobuf::StreamDecoder::Next()

In the code generated classes the ``Field()`` method returns the current field
as a typed ``Fields`` enumeration member, while the lower-level API provides a
``FieldNumber()`` method that returns the number of the field.

.. cpp:function:: Result<MyProto::Fields> MyProto::StreamDecoder::Field()
.. cpp:function:: Result<uint32_t> pw::protobuf::StreamDecoder::FieldNumber()

.. code:: c++

  #include "my_protos/my_proto.pwpb.h"
  #include "pw_protobuf/strema_decoder.h"
  #include "pw_status/status.h"
  #include "pw_status/try.h"
  #include "pw_stream/stream.h"

  pw::Status DecodeProtoFromStream(pw::stream::Reader& reader) {
    MyProto::StreamDecoder decoder(reader);
    pw::Status status;

    uint32_t age;
    char name[16];

    // Iterate over the fields in the message. A return value of OK indicates
    // that a valid field has been found and can be read. When the decoder
    // reaches the end of the message, Next() will return OUT_OF_RANGE.
    // Other return values indicate an error trying to decode the message.
    while ((status = decoder.Next()).ok()) {
      // Field() returns a Result<Fields> as it may fail sometimes.
      // However, Field() is guaranteed to be valid after a call to Next()
      // that returns OK, so the value can be used directly here.
      switch (decoder.Field().value()) {
        case MyProto::Fields::AGE: {
          PW_TRY_ASSIGN(age, decoder.ReadAge());
          break;
        }
        case MyProto::Fields::NAME:
          // The string field is copied into the provided buffer. If the buffer
          // is too small to fit the string, RESOURCE_EXHAUSTED is returned and
          // the decoder is not advanced, allowing the field to be re-read.
          PW_TRY(decoder.ReadName(name));
          break;
      }
    }

    // Do something with the fields...

    return status.IsOutOfRange() ? OkStatus() : status;
  }

Callbacks
=========
When using the ``Read()`` method with a ``struct Message``, certain fields may
require a callback function be set, otherwise a ``DataLoss`` error will be
returned should that field be encountered in the wire encoding.

The callback is called with the cursor at the field in question, and passed
a reference to the typed decoder that can examine the field and be used to
decode it.

Callback implementations may use any level of API. For example a callback for a
nested submessage (with a dependency cycle, or repeated) can be implemented by
calling ``Read()`` on a nested decoder.

.. code:: c++

    Store::Message store{};
    store.employees.SetDecoder([](Store::StreamDecoder& decoder) {
      PW_ASSERT(decoder.Field().value() == Store::Fields::EMPLOYEES);

      Employee::Message employee{};
      // Set any callbacks on `employee`.
      PW_TRY(decoder.GetEmployeesDecoder().Read(employee));
      // Do things with `employee`.
      return OkStatus();
    ));

Nested submessages
==================
Code generated ``GetFieldDecoder`` methods are provided that return a correctly
typed ``StreamDecoder`` for the message.

.. code::

  message Owner {
    Animal pet = 1;
  }

As with encoding, note that the accessor method is named for the field, while
the returned decoder is named for the message type.

.. cpp:function:: Animal::StreamDecoder Owner::StreamDecoder::GetPetDecoder()

A lower-level API method returns an untyped decoder, which only provides the
lower-level API methods. This can be moved to a typed decoder later.

.. cpp:function:: pw::protobuf::StreamDecoder pw::protobuf::StreamDecoder::GetNestedDecoder()

.. warning::
  When a nested submessage is being decoded, any use of the parent decoder that
  created the nested decoder will trigger a crash. To resume using the parent
  decoder, destroy the submessage decoder first.


.. code:: c++

  case Owner::Fields::PET: {
    // Note that the parent decoder, owner_decoder, cannot be used until the
    // nested decoder, pet_decoder, has been destroyed.
    Animal::StreamDecoder pet_decoder = owner_decoder.GetPetDecoder();

    while ((status = pet_decoder.Next()).ok()) {
      switch (pet_decoder.Field().value()) {
        // Decode pet fields...
      }
    }

    // When this scope ends, the nested decoder is destroyed and the
    // parent decoder, owner_decoder, can be used again.
    break;
  }

Scalar Fields
=============
Scalar fields are read using code generated ``ReadFoo`` methods that return the
appropriate type and assert that the correct field number ie being read.

.. cpp:function:: Result<T> MyProto::StreamDecoder::ReadFoo()

These can be freely intermixed with the lower-level API that provides a method
per field type, requiring that the caller first check the field number.

.. cpp:function:: Result<uint64_t> pw::protobuf::StreamDecoder::ReadUint64()
.. cpp:function:: Result<int64_t> pw::protobuf::StreamDecoder::ReadSint64()
.. cpp:function:: Result<int64_t> pw::protobuf::StreamDecoder::ReadInt64()
.. cpp:function:: Result<uint32_t> pw::protobuf::StreamDecoder::ReadUint32()
.. cpp:function:: Result<int32_t> pw::protobuf::StreamDecoder::ReadSint32()
.. cpp:function:: Result<int32_t> pw::protobuf::StreamDecoder::ReadInt32()
.. cpp:function:: Result<uint64_t> pw::protobuf::StreamDecoder::ReadFixed64()
.. cpp:function:: Result<uint64_t> pw::protobuf::StreamDecoder::ReadFixed32()
.. cpp:function:: Result<double> pw::protobuf::StreamDecoder::ReadDouble()
.. cpp:function:: Result<float> pw::protobuf::StreamDecoder::ReadFloat()
.. cpp:function:: Result<bool> pw::protobuf::StreamDecoder::ReadBool()

The following two code snippets are equivalent, where the first uses the code
generated API, and the second implemented by hand.

.. code:: c++

  pw::Result<int32_t> age = my_proto_decoder.ReadAge();

.. code:: c++

  PW_ASSERT(my_proto_decoder.FieldNumber().value() ==
      static_cast<uint32_t>(MyProto::Fields::AGE));
  pw::Result<int32_t> my_proto_decoder.ReadInt32();

Repeated Fields
---------------
For repeated scalar fields, multiple code generated ``ReadFoos`` methods
are provided.

.. cpp:function:: Result<T> MyProto::StreamDecoder::ReadFoos()

  This reads a single unpacked value.

.. cpp:function:: StatusWithSize MyProto::StreamDecoder::ReadFoos(std::span<T>)

  This reads a packed field containing all of the values into the provided span.

.. cpp:function:: Status MyProto::StreamDecoder::ReadFoos(pw::Vector<T>&)

  Protobuf encoders are permitted to choose either repeating single unpacked
  values, or a packed field, including splitting repeated fields up into
  multiple packed fields.

  This method supports either format, appending values to the provided
  ``pw::Vector``.

These too can be freely intermixed with the lower-level API methods, to read a
single value, a field of packed values into a ``std::span``, or support both
formats appending to a ``pw::Vector`` source.

.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedUint64(std::span<uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedUint64(pw::Vector<uint64_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedSint64(std::span<int64_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedSint64(pw::Vector<int64_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedInt64(std::span<int64_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedInt64(pw::Vector<int64_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedUint32(std::span<uint32_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedUint32(pw::Vector<uint32_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedSint32(std::span<int32_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedSint32(pw::Vector<int32_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedInt32(std::span<int32_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedInt32(pw::Vector<int32_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedFixed64(std::span<uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedFixed64(pw::Vector<uint64_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedFixed32(std::span<uint64_t>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedFixed32(pw::Vector<uint64_t>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedDouble(std::span<double>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedDouble(pw::Vector<double>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedFloat(std::span<float>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedFloat(pw::Vector<float>&)
.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadPackedBool(std::span<bool>)
.. cpp:function:: Status pw::protobuf::StreamDecoder::ReadRepeatedBool(pw::Vector<bool>&)

The following two code blocks are equivalent, where the first uses the code
generated API, and the second is implemented by hand.

.. code:: c++

  pw::Vector<int32_t, 8> numbers;

  my_proto_decoder.ReadNumbers(numbers);

.. code:: c++

  pw::Vector<int32_t, 8> numbers;

  PW_ASSERT(my_proto_decoder.FieldNumber().value() ==
      static_cast<uint32_t>(MyProto::Fields::NUMBERS));
  my_proto_decoder.ReadRepeatedInt32(numbers);

Enumerations
============
Enumerations are read using code generated ``ReadEnum`` methods that return the
code generated enumeration as the appropriate type.

.. cpp:function:: Result<MyProto::Enum> MyProto::StreamDecoder::ReadEnum()

To validate the value encoded in the wire format against the known set of
enumerates, a function is generated that you can use:

.. cpp:function:: bool MyProto::IsValidEnum(MyProto::Enum)

To read enumerations with the lower-level API, you would need to cast the
retured value from the ``uint32_t``.

The following two code blocks are equivalent, where the first is using the code
generated API, and the second implemented by hand.

.. code:: c++

  pw::Result<MyProto::Award> award = my_proto_decoder.ReadAward();
  if (!MyProto::IsValidAward(award)) {
    PW_LOG_DBG("Unknown award");
  }

.. code:: c++

  PW_ASSERT(my_proto_decoder.FieldNumber().value() ==
      static_cast<uint32_t>(MyProto::Fields::AWARD));
  pw::Result<uint32_t> award_value = my_proto_decoder.ReadUint32();
  if (award_value.ok()) {
    MyProto::Award award = static_cast<MyProto::Award>(award_value);
  }

Repeated Fields
---------------
For repeated enum fields, multiple code generated ``ReadEnums`` methods
are provided.

.. cpp:function:: Result<MyProto::Enums> MyProto::StreamDecoder::ReadEnums()

  This reads a single unpacked value.

.. cpp:function:: StatusWithSize MyProto::StreamDecoder::ReadEnums(std::span<MyProto::Enums>)

  This reads a packed field containing all of the checked values into the
  provided span.

.. cpp:function:: Status MyProto::StreamDecoder::ReadEnums(pw::Vector<MyProto::Enums>&)

  This method supports either repeated unpacked or packed formats, appending
  checked values to the provided ``pw::Vector``.

Their use is as scalar fields.

Strings
=======
Strings fields provide a code generated method to read the string into the
provided span. Since the span is updated with the size of the string, the string
is not automatically null-terminated. :ref:`module-pw_string` provides utility
methods to copy string data from spans into other targets.

.. cpp:function:: StatusWithSize MyProto::StreamDecoder::ReadName(std::span<char>)

An additional code generated method is provided to return a nested
``BytesReader`` to access the data as a stream. As with nested submessage
decoders, any use of the parent decoder that created the bytes reader will
trigger a crash. To resume using the parent decoder, destroy the bytes reader
first.

.. cpp:function:: pw::protobuf::StreamDecoder::BytesReader MyProto::StreamDecoder::GetNameReader()

These can be freely intermixed with the lower-level API method:

.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadString(std::span<char>)

The lower-level ``GetBytesReader()`` method can also be used to read string data
as bytes.

Bytes
=====
Bytes fields provide the ``WriteData`` code generated method to read the bytes
into the provided span.

.. cpp:function:: StatusWithSize MyProto::StreamDecoder::ReadData(ByteSpan)

An additional code generated method is provided to return a nested
``BytesReader`` to access the data as a stream. As with nested submessage
decoders, any use of the parent decoder that created the bytes reader will
trigger a crash. To resume using the parent decoder, destroy the bytes reader
first.

.. cpp:function:: pw::protobuf::StreamDecoder::BytesReader MyProto::StreamDecoder::GetDataReader()

These can be freely intermixed with the lower-level API methods.

.. cpp:function:: StatusWithSize pw::protobuf::StreamDecoder::ReadBytes(ByteSpan)
.. cpp:function:: pw::protobuf::StreamDecoder::BytesReader pw::protobuf::StreamDecoder::GetBytesReader()

The ``BytesReader`` supports seeking only if the ``StreamDecoder``'s reader
supports seeking.

Error Handling
==============
While individual read calls on a proto decoder return ``pw::Result``,
``pw::StatusWithSize``, or ``pw::Status`` objects, the decoder tracks all status
returns and "latches" onto the first error encountered. This status can be
accessed via ``StreamDecoder::status()``.

Length Limited Decoding
=======================
Where the length of the protobuf message is known in advance, the decoder can
be prevented from reading from the stream beyond the known bounds by specifying
the known length to the decoder:

.. code:: c++

  pw::protobuf::StreamDecoder decoder(reader, message_length);

When a decoder constructed in this way goes out of scope, it will consume any
remaining bytes in ``message_length`` allowing the next ``Read()`` on the stream
to be data after the protobuf, even when it was not fully parsed.

-----------------
In-memory Decoder
-----------------
The separate ``Decoder`` class operates on an protobuf message located in a
buffer in memory. It is more efficient than the ``StreamDecoder`` in cases
where the complete protobuf data can be stored in memory. The tradeoff of this
efficiency is that no code generation is provided, so all decoding must be
performed by hand.

As ``StreamDecoder``, it provides an iterator-style API for processing a
message. Calling ``Next()`` advances the decoder to the next proto field, which
can then be read by calling the appropriate ``Read*`` function for the field
number.

When reading ``bytes`` and ``string`` fields, the decoder returns a view of that
field within the buffer; no data is copied out.

.. code:: c++

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

---------------
Message Decoder
---------------

.. note::

  ``pw::protobuf::Message`` is unrelated to the codegen ``struct Message``
  used with ``StreamDecoder``.

The module implements a message parsing helper class ``Message``, in
``pw_protobuf/message.h``, to faciliate proto message parsing and field access.
The class provides interfaces for searching fields in a proto message and
creating helper classes for it according to its interpreted field type, i.e.
uint32, bytes, string, map<>, repeated etc. The class works on top of
``StreamDecoder`` and thus requires a ``pw::stream::SeekableReader`` for proto
message access. The following gives examples for using the class to process
different fields in a proto message:

.. code:: c++

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


.. code:: c++

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

  .. code:: c++

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
