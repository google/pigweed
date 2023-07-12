// Copyright 2023 The Pigweed Authors
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

#include "pw_protobuf/find.h"

#include "gtest/gtest.h"
#include "pw_stream/memory_stream.h"
#include "pw_string/string.h"

namespace pw::protobuf {
namespace {

// clang-format off
constexpr uint8_t _encoded_proto[] = {
  // type=int32, k=1, v=42
  0x08, 0x2a,
  // type=sint32, k=2, v=-13
  0x10, 0x19,
  // type=bool, k=3, v=false
  0x18, 0x00,
  // type=double, k=4, v=3.14159
  0x21, 0x6e, 0x86, 0x1b, 0xf0, 0xf9, 0x21, 0x09, 0x40,
  // type=fixed32, k=5, v=0xdeadbeef
  0x2d, 0xef, 0xbe, 0xad, 0xde,
  // type=string, k=6, v="Hello world"
  0x32, 0x0b, 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd',

  // type=message, k=7, len=2
  0x3a, 0x02,
  // (nested) type=uint32, k=1, v=3
  0x08, 0x03
};
ConstByteSpan encoded_proto(as_bytes(span(_encoded_proto)));

TEST(Find, PresentField) {
  EXPECT_EQ(FindInt32(encoded_proto, 1).value(), 42);
  EXPECT_EQ(FindSint32(encoded_proto, 2).value(), -13);
  EXPECT_EQ(FindBool(encoded_proto, 3).value(), false);
  EXPECT_EQ(FindDouble(encoded_proto, 4).value(), 3.14159);
  EXPECT_EQ(FindFixed32(encoded_proto, 5).value(), 0xdeadbeef);

  Result<std::string_view> result = FindString(encoded_proto, 6);
  ASSERT_EQ(result.status(), OkStatus());
  InlineString<32> str(*result);
  EXPECT_STREQ(str.c_str(), "Hello world");
}

TEST(Find, MissingField) {
  EXPECT_EQ(FindUint32(encoded_proto, 8).status(), Status::NotFound());
  EXPECT_EQ(FindUint32(encoded_proto, 66).status(), Status::NotFound());
  EXPECT_EQ(FindUint32(encoded_proto, 123456789).status(), Status::NotFound());
}

TEST(Find, InvalidFieldNumber) {
  EXPECT_EQ(FindUint32(encoded_proto, 0).status(), Status::InvalidArgument());
  EXPECT_EQ(FindUint32(encoded_proto, uint32_t(-1)).status(), Status::InvalidArgument());
}

TEST(Find, WrongWireType) {
  // Field 5 is a fixed32, but we request a uint32 (varint).
  EXPECT_EQ(FindUint32(encoded_proto, 5).status(), Status::FailedPrecondition());
}

TEST(Find, MultiLevel) {
  Result<ConstByteSpan> submessage = FindSubmessage(encoded_proto, 7);
  ASSERT_EQ(submessage.status(), OkStatus());
  EXPECT_EQ(submessage->size(), 2u);

  // Read a field from the submessage.
  EXPECT_EQ(FindUint32(*submessage, 1).value(), 3u);
}

TEST(FindStream, PresentField) {
  stream::MemoryReader reader(encoded_proto);

  EXPECT_EQ(FindInt32(reader, 1).value(), 42);
  EXPECT_EQ(FindSint32(reader, 2).value(), -13);
  EXPECT_EQ(FindBool(reader, 3).value(), false);
  EXPECT_EQ(FindDouble(encoded_proto, 4).value(), 3.14159);

  EXPECT_EQ(FindFixed32(reader, 5).value(), 0xdeadbeef);

  char str[32];
  StatusWithSize sws = FindString(reader, 6, str);
  ASSERT_EQ(sws.status(), OkStatus());
  ASSERT_EQ(sws.size(), 11u);
  str[sws.size()] = '\0';
  EXPECT_STREQ(str, "Hello world");
}

TEST(FindStream, MissingField) {
  stream::MemoryReader reader(encoded_proto);
  EXPECT_EQ(FindUint32(reader, 8).status(), Status::NotFound());

  reader = stream::MemoryReader(encoded_proto);
  EXPECT_EQ(FindUint32(reader, 66).status(), Status::NotFound());

  reader = stream::MemoryReader(encoded_proto);
  EXPECT_EQ(FindUint32(reader, 123456789).status(), Status::NotFound());
}

TEST(FindStream, InvalidFieldNumber) {
  stream::MemoryReader reader(encoded_proto);
  EXPECT_EQ(FindUint32(reader, 0).status(), Status::InvalidArgument());

  reader = stream::MemoryReader(encoded_proto);
  EXPECT_EQ(FindUint32(reader, uint32_t(-1)).status(), Status::InvalidArgument());
}

TEST(FindStream, WrongWireType) {
  stream::MemoryReader reader(encoded_proto);

  // Field 5 is a fixed32, but we request a uint32 (varint).
  EXPECT_EQ(FindUint32(reader, 5).status(), Status::FailedPrecondition());
}

enum class Fields : uint32_t {
  kField1 = 1,
  kField2 = 2,
  kField3 = 3,
  kField4 = 4,
  kField5 = 5,
  kField6 = 6,
  kField7 = 7,
};

TEST(FindEnum, PresentField) {
  EXPECT_EQ(FindInt32(encoded_proto, Fields::kField1).value(), 42);
  EXPECT_EQ(FindSint32(encoded_proto, Fields::kField2).value(), -13);
  EXPECT_EQ(FindBool(encoded_proto, Fields::kField3).value(), false);
  EXPECT_EQ(FindDouble(encoded_proto, Fields::kField4).value(), 3.14159);
  EXPECT_EQ(FindFixed32(encoded_proto, Fields::kField5).value(), 0xdeadbeef);

  stream::MemoryReader reader(encoded_proto);
  InlineString<32> str;
  StatusWithSize result = FindString(reader, Fields::kField6, str);
  ASSERT_EQ(result.status(), OkStatus());
  EXPECT_STREQ(str.c_str(), "Hello world");
}

}  // namespace
}  // namespace pw::protobuf
