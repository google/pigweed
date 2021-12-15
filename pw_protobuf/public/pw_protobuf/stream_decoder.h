// Copyright 2021 The Pigweed Authors
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

#include <array>
#include <cstring>
#include <limits>

#include "pw_assert/assert.h"
#include "pw_bytes/endian.h"
#include "pw_protobuf/wire_format.h"
#include "pw_status/status_with_size.h"
#include "pw_stream/stream.h"

namespace pw::protobuf {

// A low-level, event-based protobuf wire format decoder that operates on a
// stream.
//
// The decoder processes an encoded message by iterating over its fields. The
// caller can extract the values of any fields it cares about.
//
// The decoder does not provide any in-memory data structures to represent a
// protobuf message's data. More sophisticated APIs can be built on top of the
// low-level decoder to provide additional functionality, if desired.
//
// **NOTE**
// This decoder is intended to be used for protobuf messages which are too large
// to fit in memory. For smaller messages, prefer the MemoryDecoder, which is
// much more efficient.
//
// Example usage:
//
//   stream::Reader& my_stream = GetProtoStream();
//   StreamDecoder decoder(my_stream);
//
//   while (decoder.Next().ok()) {
//     // FieldNumber() will always be valid if Next() returns OK.
//     switch (decoder.FieldNumber().value()) {
//       case 1:
//         Result<uint32_t> result = decoder.ReadUint32();
//         if (result.ok()) {
//           DoSomething(result.value());
//         }
//         break;
//       // ... and other fields.
//     }
//   }
//
class StreamDecoder {
 public:
  // stream::Reader for a bytes field in a streamed proto message.
  //
  // Shares the StreamDecoder's reader, limiting it to the bounds of a bytes
  // field. If the StreamDecoder's reader does not supporting seeking, this
  // will also not.
  class BytesReader : public stream::RelativeSeekableReader {
   public:
    ~BytesReader() { decoder_.CloseBytesReader(*this); }

    constexpr size_t field_size() const { return end_offset_ - start_offset_; }

   private:
    friend class StreamDecoder;

    constexpr BytesReader(StreamDecoder& decoder,
                          size_t start_offset,
                          size_t end_offset)
        : decoder_(decoder),
          start_offset_(start_offset),
          end_offset_(end_offset),
          status_(OkStatus()) {}

    constexpr BytesReader(StreamDecoder& decoder, Status status)
        : decoder_(decoder),
          start_offset_(0),
          end_offset_(0),
          status_(status) {}

    StatusWithSize DoRead(ByteSpan destination) final;
    Status DoSeek(ptrdiff_t offset, Whence origin) final;

    StreamDecoder& decoder_;
    size_t start_offset_;
    size_t end_offset_;
    Status status_;
  };

  constexpr StreamDecoder(stream::Reader& reader)
      : StreamDecoder(reader, std::numeric_limits<size_t>::max()) {}

  // Allow the maximum length of the protobuf to be specified to the decoder
  // for streaming situations. When constructed in this way, the decoder will
  // consume any remaining bytes when it goes out of scope.
  constexpr StreamDecoder(stream::Reader& reader, size_t length)
      : reader_(reader),
        stream_bounds_({0, length}),
        position_(0),
        current_field_(kInitialFieldKey),
        delimited_field_size_(0),
        delimited_field_offset_(0),
        parent_(nullptr),
        field_consumed_(true),
        nested_reader_open_(false),
        status_(OkStatus()) {}

  StreamDecoder(const StreamDecoder& other) = delete;
  StreamDecoder& operator=(const StreamDecoder& other) = delete;

  ~StreamDecoder();

  // Advances to the next field in the proto.
  //
  // If Next() returns OK, there is guaranteed to be a valid protobuf field at
  // the current position, which can then be consumed through one of the Read*
  // methods.
  //
  // Return values:
  //
  //             OK: Advanced to a valid proto field.
  //   OUT_OF_RANGE: Reached the end of the proto message.
  //      DATA_LOSS: Invalid protobuf data.
  //
  Status Next();

  // Returns the field number of the current field.
  //
  // Can only be called after a successful call to Next() and before any
  // Read*() operation.
  constexpr Result<uint32_t> FieldNumber() const {
    if (field_consumed_) {
      return Status::FailedPrecondition();
    }

    return status_.ok() ? current_field_.field_number()
                        : Result<uint32_t>(status_);
  }

  //
  // TODO(frolv): Add Status Read*(T& value) APIs alongside the Result<T> ones.
  //

  // Reads a proto int32 value from the current position.
  Result<int32_t> ReadInt32();

  // Reads a proto uint32 value from the current position.
  Result<uint32_t> ReadUint32();

  // Reads a proto int64 value from the current position.
  Result<int64_t> ReadInt64();

  // Reads a proto uint64 value from the current position.
  Result<uint64_t> ReadUint64() {
    uint64_t varint;
    if (Status status = ReadVarintField(&varint); !status.ok()) {
      return status;
    }
    return varint;
  }

  // Reads a proto sint32 value from the current position.
  Result<int32_t> ReadSint32();

  // Reads a proto sint64 value from the current position.
  Result<int64_t> ReadSint64();

  // Reads a proto bool value from the current position.
  Result<bool> ReadBool();

  // Reads a proto fixed32 value from the current position.
  Result<uint32_t> ReadFixed32() { return ReadFixedField<uint32_t>(); }

  // Reads a proto fixed64 value from the current position.
  Result<uint64_t> ReadFixed64() { return ReadFixedField<uint64_t>(); }

  // Reads a proto sfixed32 value from the current position.
  Result<int32_t> ReadSfixed32() {
    Result<uint32_t> fixed32 = ReadFixed32();
    if (!fixed32.ok()) {
      return fixed32.status();
    }
    return fixed32.value();
  }

  // Reads a proto sfixed64 value from the current position.
  Result<int64_t> ReadSfixed64() {
    Result<uint64_t> fixed64 = ReadFixed64();
    if (!fixed64.ok()) {
      return fixed64.status();
    }
    return fixed64.value();
  }

  // Reads a proto float value from the current position.
  Result<float> ReadFloat() {
    static_assert(sizeof(float) == sizeof(uint32_t),
                  "Float and uint32_t must be the same size for protobufs");
    float f;
    if (Status status =
            ReadFixedField(std::as_writable_bytes(std::span(&f, 1)));
        !status.ok()) {
      return status;
    }
    return f;
  }

  // Reads a proto double value from the current position.
  Result<double> ReadDouble() {
    static_assert(sizeof(double) == sizeof(uint64_t),
                  "Double and uint64_t must be the same size for protobufs");
    double d;
    if (Status status =
            ReadFixedField(std::as_writable_bytes(std::span(&d, 1)));
        !status.ok()) {
      return status;
    }
    return d;
  }

  // Reads a proto string value from the current position. The string is copied
  // into the provided buffer and the read size is returned. The copied string
  // will NOT be null terminated; this should be done manually if desired.
  //
  // If the buffer is too small to fit the string value, RESOURCE_EXHAUSTED is
  // returned and no data is read. The decoder's position remains on the string
  // field.
  StatusWithSize ReadString(std::span<char> out) {
    return ReadBytes(std::as_writable_bytes(out));
  }

  // Reads a proto bytes value from the current position. The value is copied
  // into the provided buffer and the read size is returned.
  //
  // If the buffer is too small to fit the bytes value, RESOURCE_EXHAUSTED is
  // returned and no data is read. The decoder's position remains on the bytes
  // field.
  //
  // For larger bytes values that won't fit into memory, use GetBytesReader()
  // to acquire a stream::Reader to the bytes instead.
  StatusWithSize ReadBytes(std::span<std::byte> out) {
    return ReadDelimitedField(out);
  }

  // Returns a stream::Reader to a bytes (or string) field at the current
  // position in the protobuf.
  //
  // The BytesReader shares the same stream as the decoder, using RAII to manage
  // ownership of the stream. The decoder cannot be used while the BytesStream
  // is alive.
  //
  //   StreamDecoder decoder(my_stream);
  //
  //   while (decoder.Next().ok()) {
  //     switch (decoder.FieldNumber()) {
  //
  //       // Bytes field.
  //       case 1: {
  //         // The BytesReader is created within a new C++ scope. While it is
  //         // alive, the decoder cannot be used.
  //         StreamDecoder::BytesReader reader = decoder.GetBytesReader();
  //
  //         // Do stuff with the reader.
  //         reader.Read(&some_buffer);
  //
  //         // At the end of the scope, the reader is destructed and the
  //         // decoder becomes usable again.
  //         break;
  //       }
  //     }
  //   }
  //
  // The returned decoder is seekable if the stream's decoder is seekable.
  BytesReader GetBytesReader();

  // Returns a decoder to a nested protobuf message located at the current
  // position.
  //
  // The nested decoder shares the same stream as its parent, using RAII to
  // manage ownership of the stream. The parent decoder cannot be used while the
  // nested one is alive.
  //
  // See the example in GetBytesReader() above for RAII semantics and usage.
  StreamDecoder GetNestedDecoder();

  struct Bounds {
    size_t low;
    size_t high;
  };

  // Get the interval of the payload part of a length-delimited field. That is,
  // the interval exluding the field key and the length prefix. The bounds are
  // relative to the given reader.
  Result<Bounds> GetLengthDelimitedPayloadBounds();

 private:
  friend class BytesReader;

  // The FieldKey class can't store an invalid key, so pick a random large key
  // to set as the initial value. This will be overwritten the first time Next()
  // is called, and FieldKey() fails if Next() is not called first -- ensuring
  // that users will never see this value.
  static constexpr FieldKey kInitialFieldKey =
      FieldKey(20000, WireType::kVarint);

  constexpr StreamDecoder(stream::Reader& reader,
                          StreamDecoder* parent,
                          size_t low,
                          size_t high)
      : reader_(reader),
        stream_bounds_({low, high}),
        position_(parent->position_),
        current_field_(kInitialFieldKey),
        delimited_field_size_(0),
        delimited_field_offset_(0),
        parent_(parent),
        field_consumed_(true),
        nested_reader_open_(false),
        status_(OkStatus()) {}

  // Creates an unusable decoder in an error state. This is required as
  // GetNestedEncoder does not have a way to report an error in its API.
  constexpr StreamDecoder(stream::Reader& reader,
                          StreamDecoder* parent,
                          Status status)
      : reader_(reader),
        stream_bounds_({0, std::numeric_limits<size_t>::max()}),
        position_(0),
        current_field_(kInitialFieldKey),
        delimited_field_size_(0),
        delimited_field_offset_(0),
        parent_(parent),
        field_consumed_(true),
        nested_reader_open_(false),
        status_(status) {
    PW_ASSERT(!status.ok());
  }

  Status Advance(size_t end_position);

  void CloseBytesReader(BytesReader& reader);
  void CloseNestedDecoder(StreamDecoder& nested);

  Status ReadFieldKey();
  Status SkipField();

  Status ReadVarintField(uint64_t* out);

  Status ReadFixedField(std::span<std::byte> out);

  template <typename T>
  Result<T> ReadFixedField() {
    static_assert(std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>,
                  "Protobuf fixed-size fields must be 32- or 64-bit");

    std::array<std::byte, sizeof(T)> buffer;
    if (Status status = ReadFixedField(std::span(buffer)); !status.ok()) {
      return status;
    }

    return bytes::ReadInOrder<T>(std::endian::little, buffer);
  }

  StatusWithSize ReadDelimitedField(std::span<std::byte> out);

  Status CheckOkToRead(WireType type);

  stream::Reader& reader_;
  Bounds stream_bounds_;
  size_t position_;

  FieldKey current_field_;
  size_t delimited_field_size_;
  size_t delimited_field_offset_;

  StreamDecoder* parent_;

  bool field_consumed_;
  bool nested_reader_open_;

  Status status_;

  friend class Message;
};

}  // namespace pw::protobuf
