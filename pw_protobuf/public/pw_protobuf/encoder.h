// Copyright 2019 The Pigweed Authors
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

#include <cstddef>
#include <cstring>
#include <span>

#include "pw_bytes/span.h"
#include "pw_protobuf/wire_format.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_varint/varint.h"

namespace pw::protobuf {

// A streaming protobuf encoder which encodes to a user-specified buffer.
class Encoder {
 public:
  // TODO(frolv): Right now, only one intermediate size is supported. However,
  // this can be wasteful, as it requires 4 or 8 bytes of space per nested
  // message. This can be templated to minimize the overhead.
  using SizeType = size_t;

  constexpr Encoder(ByteSpan buffer,
                    std::span<SizeType*> locations,
                    std::span<SizeType*> stack)
      : buffer_(buffer),
        cursor_(buffer.data()),
        blob_locations_(locations),
        blob_count_(0),
        blob_stack_(stack),
        depth_(0),
        encode_status_(Status::Ok()) {}

  // Disallow copy/assign to avoid confusion about who owns the buffer.
  Encoder(const Encoder& other) = delete;
  Encoder& operator=(const Encoder& other) = delete;

  // Per the protobuf specification, valid field numbers range between 1 and
  // 2**29 - 1, inclusive. The numbers 19000-19999 are reserved for internal
  // use.
  constexpr static uint32_t kMaxFieldNumber = (1u << 29) - 1;
  constexpr static uint32_t kFirstReservedNumber = 19000;
  constexpr static uint32_t kLastReservedNumber = 19999;

  // Writes a proto uint32 key-value pair.
  Status WriteUint32(uint32_t field_number, uint32_t value) {
    return WriteUint64(field_number, value);
  }

  // Writes a repeated uint32 using packed encoding.
  Status WritePackedUint32(uint32_t field_number,
                           std::span<const uint32_t> values) {
    return WritePackedVarints(field_number, values, /*zigzag=*/false);
  }

  // Writes a proto uint64 key-value pair.
  Status WriteUint64(uint32_t field_number, uint64_t value);

  // Writes a repeated uint64 using packed encoding.
  Status WritePackedUint64(uint64_t field_number,
                           std::span<const uint64_t> values) {
    return WritePackedVarints(field_number, values, /*zigzag=*/false);
  }

  // Writes a proto int32 key-value pair.
  Status WriteInt32(uint32_t field_number, int32_t value) {
    return WriteUint64(field_number, value);
  }

  // Writes a repeated int32 using packed encoding.
  Status WritePackedInt32(uint32_t field_number,
                          std::span<const int32_t> values) {
    return WritePackedVarints(
        field_number,
        std::span(reinterpret_cast<const uint32_t*>(values.data()),
                  values.size()),
        /*zigzag=*/false);
  }

  // Writes a proto int64 key-value pair.
  Status WriteInt64(uint32_t field_number, int64_t value) {
    return WriteUint64(field_number, value);
  }

  // Writes a repeated int64 using packed encoding.
  Status WritePackedInt64(uint32_t field_number,
                          std::span<const int64_t> values) {
    return WritePackedVarints(
        field_number,
        std::span(reinterpret_cast<const uint64_t*>(values.data()),
                  values.size()),
        /*zigzag=*/false);
  }

  // Writes a proto sint32 key-value pair.
  Status WriteSint32(uint32_t field_number, int32_t value) {
    return WriteUint64(field_number, varint::ZigZagEncode(value));
  }

  // Writes a repeated sint32 using packed encoding.
  Status WritePackedSint32(uint32_t field_number,
                           std::span<const int32_t> values) {
    return WritePackedVarints(
        field_number,
        std::span(reinterpret_cast<const uint32_t*>(values.data()),
                  values.size()),
        /*zigzag=*/true);
  }

  // Writes a proto sint64 key-value pair.
  Status WriteSint64(uint32_t field_number, int64_t value) {
    return WriteUint64(field_number, varint::ZigZagEncode(value));
  }

  // Writes a repeated sint64 using packed encoding.
  Status WritePackedSint64(uint32_t field_number,
                           std::span<const int64_t> values) {
    return WritePackedVarints(
        field_number,
        std::span(reinterpret_cast<const uint64_t*>(values.data()),
                  values.size()),
        /*zigzag=*/true);
  }

  // Writes a proto bool key-value pair.
  Status WriteBool(uint32_t field_number, bool value) {
    return WriteUint32(field_number, static_cast<uint32_t>(value));
  }

  // Writes a proto fixed32 key-value pair.
  Status WriteFixed32(uint32_t field_number, uint32_t value) {
    std::byte* original_cursor = cursor_;
    WriteFieldKey(field_number, WireType::kFixed32);
    Status status = WriteRawBytes(value);
    IncreaseParentSize(cursor_ - original_cursor);
    return status;
  }

  // Writes a repeated fixed32 field using packed encoding.
  Status WritePackedFixed32(uint32_t field_number,
                            std::span<const uint32_t> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto fixed64 key-value pair.
  Status WriteFixed64(uint32_t field_number, uint64_t value) {
    std::byte* original_cursor = cursor_;
    WriteFieldKey(field_number, WireType::kFixed64);
    Status status = WriteRawBytes(value);
    IncreaseParentSize(cursor_ - original_cursor);
    return status;
  }

  // Writes a repeated fixed64 field using packed encoding.
  Status WritePackedFixed64(uint32_t field_number,
                            std::span<const uint64_t> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto sfixed32 key-value pair.
  Status WriteSfixed32(uint32_t field_number, int32_t value) {
    return WriteFixed32(field_number, static_cast<uint32_t>(value));
  }

  // Writes a repeated sfixed32 field using packed encoding.
  Status WritePackedSfixed32(uint32_t field_number,
                             std::span<const int32_t> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto sfixed64 key-value pair.
  Status WriteSfixed64(uint32_t field_number, int64_t value) {
    return WriteFixed64(field_number, static_cast<uint64_t>(value));
  }

  // Writes a repeated sfixed64 field using packed encoding.
  Status WritePackedSfixed64(uint32_t field_number,
                             std::span<const int64_t> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto float key-value pair.
  Status WriteFloat(uint32_t field_number, float value) {
    static_assert(sizeof(float) == sizeof(uint32_t),
                  "Float and uint32_t are not the same size");
    std::byte* original_cursor = cursor_;
    WriteFieldKey(field_number, WireType::kFixed32);
    Status status = WriteRawBytes(value);
    IncreaseParentSize(cursor_ - original_cursor);
    return status;
  }

  // Writes a repeated float field using packed encoding.
  Status WritePackedFloat(uint32_t field_number,
                          std::span<const float> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto double key-value pair.
  Status WriteDouble(uint32_t field_number, double value) {
    static_assert(sizeof(double) == sizeof(uint64_t),
                  "Double and uint64_t are not the same size");
    std::byte* original_cursor = cursor_;
    WriteFieldKey(field_number, WireType::kFixed64);
    Status status = WriteRawBytes(value);
    IncreaseParentSize(cursor_ - original_cursor);
    return status;
  }

  // Writes a repeated double field using packed encoding.
  Status WritePackedDouble(uint32_t field_number,
                           std::span<const double> values) {
    return WriteBytes(field_number, std::as_bytes(values));
  }

  // Writes a proto bytes key-value pair.
  Status WriteBytes(uint32_t field_number, ConstByteSpan value) {
    std::byte* original_cursor = cursor_;
    WriteFieldKey(field_number, WireType::kDelimited);
    WriteVarint(value.size_bytes());
    Status status = WriteRawBytes(value.data(), value.size_bytes());
    IncreaseParentSize(cursor_ - original_cursor);
    return status;
  }

  // Writes a proto string key-value pair.
  Status WriteString(uint32_t field_number, const char* value, size_t size) {
    return WriteBytes(field_number, std::as_bytes(std::span(value, size)));
  }

  Status WriteString(uint32_t field_number, const char* value) {
    return WriteString(field_number, value, strlen(value));
  }

  // Begins writing a sub-message with a specified field number.
  Status Push(uint32_t field_number);

  // Finishes writing a sub-message.
  Status Pop();

  // Returns the total encoded size of the proto message.
  size_t EncodedSize() const { return cursor_ - buffer_.data(); }

  // Returns the number of bytes remaining in the buffer.
  size_t RemainingSize() const { return buffer_.size() - EncodedSize(); }

  // Resets write index to the start of the buffer. This invalidates any spans
  // obtained from Encode().
  void Clear() {
    cursor_ = buffer_.data();
    encode_status_ = Status::Ok();
    blob_count_ = 0;
    depth_ = 0;
  }

  // Runs a final encoding pass over the intermediary data and returns the
  // encoded protobuf message.
  Result<ConstByteSpan> Encode();

  // DEPRECATED. Use Encode() instead.
  // TODO(frolv): Remove this after all references to it are updated.
  Status Encode(ConstByteSpan* out) {
    Result result = Encode();
    if (!result.ok()) {
      return result.status();
    }
    *out = result.value();
    return Status::Ok();
  }

 private:
  constexpr bool ValidFieldNumber(uint32_t field_number) const {
    return field_number != 0 && field_number <= kMaxFieldNumber &&
           !(field_number >= kFirstReservedNumber &&
             field_number <= kLastReservedNumber);
  }

  // Encodes the key for a proto field consisting of its number and wire type.
  Status WriteFieldKey(uint32_t field_number, WireType wire_type) {
    if (!ValidFieldNumber(field_number)) {
      encode_status_ = Status::InvalidArgument();
      return encode_status_;
    }

    return WriteVarint(MakeKey(field_number, wire_type));
  }

  Status WriteVarint(uint64_t value);

  Status WriteZigzagVarint(int64_t value) {
    return WriteVarint(varint::ZigZagEncode(value));
  }

  template <typename T>
  Status WriteRawBytes(const T& value) {
    return WriteRawBytes(reinterpret_cast<const std::byte*>(&value),
                         sizeof(value));
  }

  Status WriteRawBytes(const std::byte* ptr, size_t size);

  // Writes a list of varints to the buffer in length-delimited packed encoding.
  // If zigzag is true, zig-zag encodes each of the varints.
  template <typename T>
  Status WritePackedVarints(uint32_t field_number,
                            std::span<T> values,
                            bool zigzag) {
    if (Status status = Push(field_number); !status.ok()) {
      return status;
    }

    std::byte* original_cursor = cursor_;
    for (T value : values) {
      if (zigzag) {
        WriteZigzagVarint(static_cast<std::make_signed_t<T>>(value));
      } else {
        WriteVarint(value);
      }
    }
    IncreaseParentSize(cursor_ - original_cursor);

    return Pop();
  }

  // Adds to the parent proto's size field in the buffer.
  void IncreaseParentSize(size_t bytes) {
    if (depth_ > 0) {
      *blob_stack_[depth_ - 1] += bytes;
    }
  }

  // Returns the size of `n` encoded as a varint.
  size_t VarintSizeBytes(uint64_t n) {
    size_t size_bytes = 1;
    while (n > 127) {
      ++size_bytes;
      n >>= 7;
    }
    return size_bytes;
  }

  // Do the actual (potentially partial) encoding. Also used in Pop().
  Result<ConstByteSpan> EncodeFrom(size_t blob);

  // The buffer into which the proto is encoded.
  ByteSpan buffer_;
  std::byte* cursor_;

  // List of pointers to sub-messages' delimiting size fields.
  std::span<SizeType*> blob_locations_;
  size_t blob_count_;

  // Stack of current nested message size locations. Push() operations add a new
  // entry to this stack and Pop() operations remove one.
  std::span<SizeType*> blob_stack_;
  size_t depth_;

  Status encode_status_;
};

// A proto encoder with its own blob stack.
template <size_t kMaxNestedDepth = 1, size_t kMaxBlobs = kMaxNestedDepth>
class NestedEncoder : public Encoder {
 public:
  NestedEncoder(ByteSpan buffer) : Encoder(buffer, blobs_, stack_) {}

  // Disallow copy/assign to avoid confusion about who owns the buffer.
  NestedEncoder(const NestedEncoder& other) = delete;
  NestedEncoder& operator=(const NestedEncoder& other) = delete;

 private:
  std::array<size_t*, kMaxBlobs> blobs_;
  std::array<size_t*, kMaxNestedDepth> stack_;
};

// Explicit template argument deduction to hide warnings.
NestedEncoder()->NestedEncoder<>;

}  // namespace pw::protobuf
