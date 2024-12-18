
// Copyright 2024 The Pigweed Authors
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

#include "pw_bytes/span.h"
#include "pw_protobuf_test_protos/edition.pwpb.h"
#include "pw_protobuf_test_protos/edition_file_options.pwpb.h"
#include "pw_status/status.h"
#include "pw_stream/memory_stream.h"
#include "pw_unit_test/framework.h"

namespace pw::protobuf {
namespace {

using namespace ::pw::protobuf::test::pwpb;

TEST(EditionsMessage, GeneratesCorrectTypes) {
  static_assert(std::is_same_v<decltype(EditionsTest::Message::optional_uint),
                               std::optional<uint32_t>>);
  static_assert(
      std::is_same_v<decltype(EditionsTest::Message::default_uint), uint32_t>);
  static_assert(std::is_same_v<decltype(EditionsTest::Message::packed_values),
                               pw::Vector<int32_t, 8>>);
}

TEST(EditionsFileOptionsMessage, GeneratesCorrectTypes) {
  static_assert(std::is_same_v<decltype(EditionsFileOptionsTest::Message::name),
                               InlineBasicString<char, 16>>);
  static_assert(
      std::is_same_v<decltype(EditionsFileOptionsTest::Message::value),
                     uint32_t>);
  static_assert(
      std::is_same_v<decltype(EditionsFileOptionsTest::Message::active), bool>);
  static_assert(
      std::is_same_v<decltype(EditionsFileOptionsTest::Message::count),
                     std::optional<int32_t>>);
}

TEST(EditionsMessage, Write) {
  const EditionsTest::Message message{
      .optional_uint = std::nullopt,
      .default_uint = 0,
      .packed_values = {1000, 2000, 3000, 4000},
  };

  // clang-format off
  constexpr uint8_t expected_proto[] = {
    // optional_uint omitted
    // default_uint omitted
    // packed_values[], v={1000, 2000, 3000, 4000}
    0x1a, 0x08, 0xe8, 0x07, 0xd0, 0x0f, 0xb8, 0x17, 0xa0, 0x1f,
  };
  // clang-format on

  std::byte encode_buffer[EditionsTest::kMaxEncodedSizeBytes];
  stream::MemoryWriter writer(encode_buffer);
  EditionsTest::StreamEncoder encoder(writer, ByteSpan());

  ASSERT_EQ(encoder.Write(message), OkStatus());

  ConstByteSpan result = writer.WrittenData();
  EXPECT_EQ(result.size(), sizeof(expected_proto));
  EXPECT_EQ(std::memcmp(result.data(), expected_proto, sizeof(expected_proto)),
            0);
}

}  // namespace
}  // namespace pw::protobuf
