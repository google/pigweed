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

constexpr size_t SizeOfFieldKey(uint32_t field_number) {
  // The wiretype is ignored as this does not impact the serialized size.
  return varint::EncodedSize(field_number << kFieldNumberShift);
}

}  // namespace pw::protobuf
