// Copyright 2020 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <string_view>

#include "pw_protobuf/wire_format.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_varint/varint.h"

// This file defines a low-level event-based protobuf wire format decoder.
// The decoder processes an encoded message by iterating over its fields and
// notifying a handler for each field it encounters. The handler receives a
// reference to the decoder object and can extract the field's value from the
// message.
//
// The decoder does not provide any in-memory data structures to represent a
// protobuf message's data. More sophisticated APIs can be built on top of the
// low-level decoder to provide additional functionality, if desired.
//
// Example usage:
//
//   class FooProtoHandler : public DecodeHandler {
//    public:
//     Status ProcessField(Decoder* decoder, uint32_t field_number) override {
//       switch (field_number) {
//         case FooFields::kBar:
//           if (!decoder->ReadSint32(field_number, &bar).ok()) {
//             bar = 0;
//           }
//           break;
//         case FooFields::kBaz:
//           if (!decoder->ReadUint32(field_number, &baz).ok()) {
//             baz = 0;
//           }
//           break;
//       }
//
//       return Status::OK;
//     }
//
//     int bar;
//     unsigned int baz;
//   };
//
//   void DecodeFooProto(span<std::byte> raw_proto) {
//     Decoder decoder;
//     FooProtoHandler handler;
//
//     decoder.set_handler(&handler);
//     if (!decoder.Decode(raw_proto).ok()) {
//       LOG_FATAL("Invalid foo message!");
//     }
//
//     LOG_INFO("Read Foo proto message; bar: %d baz: %u",
//              handler.bar, handler.baz);
//   }
//

namespace pw::protobuf {

class DecodeHandler;

// A protobuf decoder that iterates over an encoded protobuf, calling a handler
// for each field it encounters.
class Decoder {
 public:
  constexpr Decoder() : handler_(nullptr), state_(kReady) {}

  Decoder(const Decoder& other) = delete;
  Decoder& operator=(const Decoder& other) = delete;

  void set_handler(DecodeHandler* handler) { handler_ = handler; }

  // Decodes the specified protobuf data. The registered handler's ProcessField
  // function is called on each field found in the data.
  Status Decode(span<const std::byte> proto);

  // Reads a proto int32 value from the current cursor.
  Status ReadInt32(uint32_t field_number, int32_t* out) {
    return ReadUint32(field_number, reinterpret_cast<uint32_t*>(out));
  }

  // Reads a proto uint32 value from the current cursor.
  Status ReadUint32(uint32_t field_number, uint32_t* out) {
    uint64_t value = 0;
    Status status = ReadUint64(field_number, &value);
    if (!status.ok()) {
      return status;
    }
    if (value > std::numeric_limits<uint32_t>::max()) {
      return Status::OUT_OF_RANGE;
    }
    *out = value;
    return Status::OK;
  }

  // Reads a proto int64 value from the current cursor.
  Status ReadInt64(uint32_t field_number, int64_t* out) {
    return ReadVarint(field_number, reinterpret_cast<uint64_t*>(out));
  }

  // Reads a proto uint64 value from the current cursor.
  Status ReadUint64(uint32_t field_number, uint64_t* out) {
    return ReadVarint(field_number, out);
  }

  // Reads a proto sint32 value from the current cursor.
  Status ReadSint32(uint32_t field_number, int32_t* out) {
    int64_t value = 0;
    Status status = ReadSint64(field_number, &value);
    if (!status.ok()) {
      return status;
    }
    if (value > std::numeric_limits<int32_t>::max()) {
      return Status::OUT_OF_RANGE;
    }
    *out = value;
    return Status::OK;
  }

  // Reads a proto sint64 value from the current cursor.
  Status ReadSint64(uint32_t field_number, int64_t* out) {
    uint64_t value = 0;
    Status status = ReadUint64(field_number, &value);
    if (!status.ok()) {
      return status;
    }
    *out = varint::ZigZagDecode(value);
    return Status::OK;
  }

  // Reads a proto bool value from the current cursor.
  Status ReadBool(uint32_t field_number, bool* out) {
    uint64_t value = 0;
    Status status = ReadUint64(field_number, &value);
    if (!status.ok()) {
      return status;
    }
    *out = value;
    return Status::OK;
  }

  // Reads a proto fixed32 value from the current cursor.
  Status ReadFixed32(uint32_t field_number, uint32_t* out) {
    return ReadFixed(field_number, out);
  }

  // Reads a proto fixed64 value from the current cursor.
  Status ReadFixed64(uint32_t field_number, uint64_t* out) {
    return ReadFixed(field_number, out);
  }

  // Reads a proto sfixed32 value from the current cursor.
  Status ReadSfixed32(uint32_t field_number, int32_t* out) {
    return ReadFixed32(field_number, reinterpret_cast<uint32_t*>(out));
  }

  // Reads a proto sfixed64 value from the current cursor.
  Status ReadSfixed64(uint32_t field_number, int64_t* out) {
    return ReadFixed64(field_number, reinterpret_cast<uint64_t*>(out));
  }

  // Reads a proto float value from the current cursor.
  Status ReadFloat(uint32_t field_number, float* out) {
    static_assert(sizeof(float) == sizeof(uint32_t),
                  "Float and uint32_t must be the same size for protobufs");
    return ReadFixed(field_number, out);
  }

  // Reads a proto double value from the current cursor.
  Status ReadDouble(uint32_t field_number, double* out) {
    static_assert(sizeof(double) == sizeof(uint64_t),
                  "Double and uint64_t must be the same size for protobufs");
    return ReadFixed(field_number, out);
  }

  // Reads a proto string value from the current cursor and returns a view of it
  // in `out`. The raw protobuf data must outlive `out`. If the string field is
  // invalid, `out` is not modified.
  Status ReadString(uint32_t field_number, std::string_view* out) {
    span<const std::byte> bytes;
    Status status = ReadDelimited(field_number, &bytes);
    if (!status.ok()) {
      return status;
    }
    *out = std::string_view(reinterpret_cast<const char*>(bytes.data()),
                            bytes.size());
    return Status::OK;
  }

  // Reads a proto bytes value from the current cursor and returns a view of it
  // in `out`. The raw protobuf data must outlive the `out` span. If the bytes
  // field is invalid, `out` is not modified.
  Status ReadBytes(uint32_t field_number, span<const std::byte>* out) {
    return ReadDelimited(field_number, out);
  }

 private:
  enum State {
    kReady,
    kDecodeInProgress,
    kDecodeFailed,
  };

  // Reads a varint key-value pair from the current cursor position.
  Status ReadVarint(uint32_t field_number, uint64_t* out);

  // Reads a fixed-size key-value pair from the current cursor position.
  Status ReadFixed(uint32_t field_number, std::byte* out, size_t size);

  template <typename T>
  Status ReadFixed(uint32_t field_number, T* out) {
    static_assert(
        sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t),
        "Protobuf fixed-size fields must be 32- or 64-bit");
    union {
      T value;
      std::byte bytes[sizeof(T)];
    };
    Status status = ReadFixed(field_number, bytes, sizeof(bytes));
    if (!status.ok()) {
      return status;
    }
    *out = value;
    return Status::OK;
  }

  Status ReadDelimited(uint32_t field_number, span<const std::byte>* out);

  Status ConsumeKey(uint32_t field_number, WireType expected_type);

  // Advances the cursor to the next field in the proto.
  void SkipField();

  DecodeHandler* handler_;

  State state_;
  span<const std::byte> proto_;
};

// The event-handling interface implemented for a proto decoding operation.
class DecodeHandler {
 public:
  virtual ~DecodeHandler() = default;

  // Callback called for each field encountered in the decoded proto message.
  // Receives a pointer to the decoder object, allowing the handler to call
  // the appropriate method to extract the field's data.
  //
  // If the status returned is not Status::OK, the decode operation is exited
  // with the provided status. Returning Status::CANCELLED allows a convenient
  // way of stopping a decode early (for example, if a desired field is found).
  virtual Status ProcessField(Decoder* decoder, uint32_t field_number) = 0;
};

}  // namespace pw::protobuf
