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

#include <cstdint>

#include "pw_protobuf/wire_format.h"
#include "pw_varint/varint.h"

namespace pw::protobuf {

// Field types that directly map to fixed wire types:
inline constexpr size_t kMaxSizeBytesFixed32 = 4;
inline constexpr size_t kMaxSizeBytesFixed64 = 8;
inline constexpr size_t kMaxSizeBytesSfixed32 = 4;
inline constexpr size_t kMaxSizeBytesSfixed64 = 8;
inline constexpr size_t kMaxSizeBytesFloat = kMaxSizeBytesFixed32;
inline constexpr size_t kMaxSizeBytesDouble = kMaxSizeBytesFixed64;

// Field types that map to varint:
inline constexpr size_t kMaxSizeBytesUint32 = varint::kMaxVarint32SizeBytes;
inline constexpr size_t kMaxSizeBytesUint64 = varint::kMaxVarint64SizeBytes;
inline constexpr size_t kMaxSizeBytesSint32 = varint::kMaxVarint32SizeBytes;
inline constexpr size_t kMaxSizeBytesSint64 = varint::kMaxVarint64SizeBytes;
// The int32 field type does not use zigzag encoding, ergo negative values
// can result in the worst case varint size.
inline constexpr size_t kMaxSizeBytesInt32 = varint::kMaxVarint64SizeBytes;
inline constexpr size_t kMaxSizeBytesInt64 = varint::kMaxVarint64SizeBytes;
// The bool field type is backed by a varint, but has a limited value range.
inline constexpr size_t kMaxSizeBytesBool = 1;

inline constexpr size_t kMaxSizeOfFieldKey = varint::kMaxVarint32SizeBytes;

inline constexpr size_t kMaxSizeOfLength = varint::kMaxVarint32SizeBytes;

// Calculate the size of a proto field key in wire format, including the key
// field number + wire type.
// type).
//
// Args:
//   field_number: The field number for the field.
//
// Returns:
//   The size of the field key.
//
// Precondition: The field_number must be a ValidFieldNumber.
// Precondition: The field_number must be a ValidFieldNumber.
constexpr size_t SizeOfFieldKey(uint32_t field_number) {
  // The wiretype does not impact the serialized size, so use kVarint (0), which
  // will be optimized out by the compiler.
  return varint::EncodedSize(FieldKey(field_number, WireType::kVarint));
}

// Calculate the size of a proto field in wire format. This is the size of a
// final serialized protobuf entry, including the key (field number + wire
// type), encoded payload size (for length-delimited types), and data.
//
// Args:
//   field_number: The field number for the field.
//   type: The wire type of the field
//   data_size: The size of the payload.
//
// Returns:
//   The size of the field.
//
// Precondition: The field_number must be a ValidFieldNumber.
// Precondition: `data_size_bytes` must be smaller than
//   std::numeric_limits<uint32_t>::max()
constexpr size_t SizeOfField(uint32_t field_number,
                             WireType type,
                             size_t data_size_bytes) {
  size_t size = SizeOfFieldKey(field_number);
  if (type == WireType::kDelimited) {
    size += varint::EncodedSize(data_size_bytes);
  }
  size += data_size_bytes;
  return size;
}

}  // namespace pw::protobuf
