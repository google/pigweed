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

#include "pw_bytes/array.h"
#include "pw_protobuf/encoder.h"
#include "pw_protobuf_test_protos/full_test.pwpb.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::protobuf {
namespace {

TEST(Encoder, SizeTypeIsConfigured) {
  static_assert(config::kMaxVarintSize == sizeof(uint8_t));
}

TEST(Encoder, NestedWriteSmallerThanVarintSize) {
  std::array<std::byte, 256> buffer;

  MemoryEncoder encoder(buffer);

  {
    // Overhead of 1 byte key + 1 byte size for the nested message itself.
    StreamEncoder nested = encoder.GetNestedEncoder(1);
    EXPECT_EQ(nested.ConservativeWriteLimit(), 125u);
    // 1 byte key + 1 byte size + 123 byte value = 125 byte nested length.
    EXPECT_EQ(nested.WriteBytes(2, bytes::Initialized<123>(0xaa)), OkStatus());
  }

  EXPECT_EQ(encoder.status(), OkStatus());
}

TEST(Encoder, NestedWriteLargerThanVarintSizeReturnsResourceExhausted) {
  std::array<std::byte, 256> buffer;

  MemoryEncoder encoder(buffer);

  {
    // Try to write a larger nested message than the max nested varint value.
    // Overhead of 1 byte key + 1 byte size for the nested message itself.
    StreamEncoder nested = encoder.GetNestedEncoder(1);
    EXPECT_EQ(nested.ConservativeWriteLimit(), 125u);
    // 1 byte key + 1 byte size + 124 byte value = 126 byte nested length.
    EXPECT_EQ(nested.WriteBytes(2, bytes::Initialized<124>(0xaa)),
              Status::ResourceExhausted());
    EXPECT_EQ(nested.WriteUint32(3, 42), Status::ResourceExhausted());
  }

  EXPECT_EQ(encoder.status(), Status::ResourceExhausted());
}

TEST(Encoder, NestedMessageLargerThanVarintSizeReturnsResourceExhausted) {
  std::array<std::byte, 256> buffer;

  MemoryEncoder encoder(buffer);

  {
    // Try to write a larger nested message than the max nested varint value as
    // multiple smaller writes.
    StreamEncoder nested = encoder.GetNestedEncoder(1);
    EXPECT_EQ(nested.WriteBytes(2, bytes::Initialized<60>(0xaa)), OkStatus());
    EXPECT_EQ(nested.WriteBytes(3, bytes::Initialized<60>(0xaa)), OkStatus());
    EXPECT_EQ(nested.WriteBytes(4, bytes::Initialized<60>(0xaa)),
              Status::ResourceExhausted());
  }

  EXPECT_EQ(encoder.status(), Status::ResourceExhausted());
}

TEST(Encoder, MessageWrite_NestedMessageLargerThanVarintSize) {
  std::array<std::byte, 256> buffer;
  test::pwpb::LargeNestedTest::MemoryEncoder encoder(buffer);

  test::pwpb::LargeNestedTest::Message message = {};

  message.large_nested.data.SetEncoder(
      [](test::pwpb::LargeNestedTest::LargeNested::StreamEncoder&
             large_nested_encoder) {
        // 1 byte key + 1 byte size + 126 byte value = 128 byte nested length.
        Status status =
            large_nested_encoder.WriteData(bytes::Initialized<126>(0xaa));
        EXPECT_EQ(status, Status::ResourceExhausted());
        return status;
      });

  EXPECT_EQ(encoder.Write(message), Status::ResourceExhausted());
}

}  // namespace
}  // namespace pw::protobuf
