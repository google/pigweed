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

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_protobuf/encoder.h"

namespace pw::protobuf {
namespace {

TEST(Encoder, SizeTypeIsConfigured) {
  static_assert(sizeof(Encoder::SizeType) == sizeof(uint8_t));
}

TEST(Encoder, NestedWriteSmallerThanVarintSize) {
  std::array<std::byte, 256> buffer;
  NestedEncoder<2, 2> encoder(buffer);

  encoder.Push(1);
  // 1 byte key + 1 byte size + 125 byte value = 127 byte nested length.
  EXPECT_EQ(encoder.WriteBytes(2, bytes::Initialized<125>(0xaa)), OkStatus());
  encoder.Pop();

  auto result = encoder.Encode();
  EXPECT_EQ(result.status(), OkStatus());
}

TEST(Encoder, NestedWriteLargerThanVarintSizeReturnsOutOfRange) {
  std::array<std::byte, 256> buffer;
  NestedEncoder<2, 2> encoder(buffer);

  // Try to write a larger nested message than the max nested varint value.
  encoder.Push(1);
  // 1 byte key + 1 byte size + 126 byte value = 128 byte nested length.
  EXPECT_EQ(encoder.WriteBytes(2, bytes::Initialized<126>(0xaa)),
            Status::OutOfRange());
  EXPECT_EQ(encoder.WriteUint32(3, 42), Status::OutOfRange());
  encoder.Pop();

  auto result = encoder.Encode();
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(Encoder, NestedMessageLargerThanVarintSizeReturnsOutOfRange) {
  std::array<std::byte, 256> buffer;
  NestedEncoder<2, 2> encoder(buffer);

  // Try to write a larger nested message than the max nested varint value as
  // multiple smaller writes.
  encoder.Push(1);
  EXPECT_EQ(encoder.WriteBytes(2, bytes::Initialized<60>(0xaa)), OkStatus());
  EXPECT_EQ(encoder.WriteBytes(3, bytes::Initialized<60>(0xaa)), OkStatus());
  EXPECT_EQ(encoder.WriteBytes(4, bytes::Initialized<60>(0xaa)),
            Status::OutOfRange());
  encoder.Pop();

  auto result = encoder.Encode();
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

}  // namespace
}  // namespace pw::protobuf
