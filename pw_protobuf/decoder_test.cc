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

#include "pw_protobuf/decoder.h"

#include "gtest/gtest.h"
#include "pw_preprocessor/util.h"

namespace pw::protobuf {
namespace {

class TestDecodeHandler : public DecodeHandler {
 public:
  Status ProcessField(CallbackDecoder& decoder,
                      uint32_t field_number) override {
    std::string_view str;

    switch (field_number) {
      case 1:
        decoder.ReadInt32(&test_int32);
        break;
      case 2:
        decoder.ReadSint32(&test_sint32);
        break;
      case 3:
        decoder.ReadBool(&test_bool);
        break;
      case 4:
        decoder.ReadDouble(&test_double);
        break;
      case 5:
        decoder.ReadFixed32(&test_fixed32);
        break;
      case 6:
        decoder.ReadString(&str);
        std::memcpy(test_string, str.data(), str.size());
        test_string[str.size()] = '\0';
        break;
    }

    called = true;
    return Status::Ok();
  }

  bool called = false;
  int32_t test_int32 = 0;
  int32_t test_sint32 = 0;
  bool test_bool = true;
  double test_double = 0;
  uint32_t test_fixed32 = 0;
  char test_string[16];
};

TEST(Decoder, Decode) {
  // clang-format off
  uint8_t encoded_proto[] = {
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
  };
  // clang-format on

  Decoder decoder(std::as_bytes(std::span(encoded_proto)));

  int32_t v1 = 0;
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 1u);
  EXPECT_EQ(decoder.ReadInt32(&v1), Status::Ok());
  EXPECT_EQ(v1, 42);

  int32_t v2 = 0;
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 2u);
  EXPECT_EQ(decoder.ReadSint32(&v2), Status::Ok());
  EXPECT_EQ(v2, -13);

  bool v3 = true;
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 3u);
  EXPECT_EQ(decoder.ReadBool(&v3), Status::Ok());
  EXPECT_FALSE(v3);

  double v4 = 0;
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 4u);
  EXPECT_EQ(decoder.ReadDouble(&v4), Status::Ok());
  EXPECT_EQ(v4, 3.14159);

  uint32_t v5 = 0;
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 5u);
  EXPECT_EQ(decoder.ReadFixed32(&v5), Status::Ok());
  EXPECT_EQ(v5, 0xdeadbeef);

  std::string_view v6;
  char buffer[16];
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 6u);
  EXPECT_EQ(decoder.ReadString(&v6), Status::Ok());
  std::memcpy(buffer, v6.data(), v6.size());
  buffer[v6.size()] = '\0';
  EXPECT_STREQ(buffer, "Hello world");

  EXPECT_EQ(decoder.Next(), Status::OutOfRange());
}

TEST(Decoder, Decode_SkipsUnusedFields) {
  // clang-format off
  uint8_t encoded_proto[] = {
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
  };
  // clang-format on

  Decoder decoder(std::as_bytes(std::span(encoded_proto)));

  // Don't process any fields except for the fourth. Next should still iterate
  // correctly despite field values not being consumed.
  EXPECT_EQ(decoder.Next(), Status::Ok());
  EXPECT_EQ(decoder.Next(), Status::Ok());
  EXPECT_EQ(decoder.Next(), Status::Ok());
  EXPECT_EQ(decoder.Next(), Status::Ok());
  ASSERT_EQ(decoder.FieldNumber(), 4u);
  EXPECT_EQ(decoder.Next(), Status::Ok());
  EXPECT_EQ(decoder.Next(), Status::Ok());
  EXPECT_EQ(decoder.Next(), Status::OutOfRange());
}

TEST(CallbackDecoder, Decode) {
  CallbackDecoder decoder;
  TestDecodeHandler handler;

  // clang-format off
  uint8_t encoded_proto[] = {
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
  };
  // clang-format on

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::as_bytes(std::span(encoded_proto))),
            Status::Ok());
  EXPECT_TRUE(handler.called);
  EXPECT_EQ(handler.test_int32, 42);
  EXPECT_EQ(handler.test_sint32, -13);
  EXPECT_FALSE(handler.test_bool);
  EXPECT_EQ(handler.test_double, 3.14159);
  EXPECT_EQ(handler.test_fixed32, 0xdeadbeef);
  EXPECT_STREQ(handler.test_string, "Hello world");
}

TEST(CallbackDecoder, Decode_OverridesDuplicateFields) {
  CallbackDecoder decoder;
  TestDecodeHandler handler;

  // clang-format off
  uint8_t encoded_proto[] = {
    // type=int32, k=1, v=42
    0x08, 0x2a,
    // type=int32, k=1, v=43
    0x08, 0x2b,
    // type=int32, k=1, v=44
    0x08, 0x2c,
  };
  // clang-format on

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::as_bytes(std::span(encoded_proto))),
            Status::Ok());
  EXPECT_TRUE(handler.called);
  EXPECT_EQ(handler.test_int32, 44);
}

TEST(CallbackDecoder, Decode_Empty) {
  CallbackDecoder decoder;
  TestDecodeHandler handler;

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::span<std::byte>()), Status::Ok());
  EXPECT_FALSE(handler.called);
  EXPECT_EQ(handler.test_int32, 0);
  EXPECT_EQ(handler.test_sint32, 0);
}

TEST(CallbackDecoder, Decode_BadData) {
  CallbackDecoder decoder;
  TestDecodeHandler handler;

  // Field key without a value.
  uint8_t encoded_proto[] = {0x08};

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::as_bytes(std::span(encoded_proto))),
            Status::DataLoss());
}

// Only processes fields numbered 1 or 3.
class OneThreeDecodeHandler : public DecodeHandler {
 public:
  Status ProcessField(CallbackDecoder& decoder,
                      uint32_t field_number) override {
    switch (field_number) {
      case 1:
        EXPECT_EQ(decoder.ReadInt32(&field_one), Status::Ok());
        break;
      case 3:
        EXPECT_EQ(decoder.ReadInt32(&field_three), Status::Ok());
        break;
      default:
        // Do nothing.
        break;
    }

    called = true;
    return Status::Ok();
  }

  bool called = false;
  int32_t field_one = 0;
  int32_t field_three = 0;
};

TEST(CallbackDecoder, Decode_SkipsUnprocessedFields) {
  CallbackDecoder decoder;
  OneThreeDecodeHandler handler;

  // clang-format off
  uint8_t encoded_proto[] = {
    // type=int32, k=1, v=42
    // Should be read.
    0x08, 0x2a,
    // type=sint32, k=2, v=-13
    // Should be ignored.
    0x10, 0x19,
    // type=int32, k=2, v=3
    // Should be ignored.
    0x10, 0x03,
    // type=int32, k=3, v=99
    // Should be read.
    0x18, 0x63,
    // type=int32, k=4, v=16
    // Should be ignored.
    0x20, 0x10,
  };
  // clang-format on

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::as_bytes(std::span(encoded_proto))),
            Status::Ok());
  EXPECT_TRUE(handler.called);
  EXPECT_EQ(handler.field_one, 42);
  EXPECT_EQ(handler.field_three, 99);
}

// Only processes fields numbered 1 or 3, and stops the decode after hitting 1.
class ExitOnOneDecoder : public DecodeHandler {
 public:
  Status ProcessField(CallbackDecoder& decoder,
                      uint32_t field_number) override {
    switch (field_number) {
      case 1:
        EXPECT_EQ(decoder.ReadInt32(&field_one), Status::Ok());
        return Status::Cancelled();
      case 3:
        EXPECT_EQ(decoder.ReadInt32(&field_three), Status::Ok());
        break;
      default:
        // Do nothing.
        break;
    }

    return Status::Ok();
  }

  int32_t field_one = 0;
  int32_t field_three = 1111;
};

TEST(CallbackDecoder, Decode_StopsOnNonOkStatus) {
  CallbackDecoder decoder;
  ExitOnOneDecoder handler;

  // clang-format off
  uint8_t encoded_proto[] = {
    // type=int32, k=1, v=42
    // Should be read.
    0x08, 0x2a,
    // type=int32, k=3, v=99
    // Should be skipped.
    0x18, 0x63,
    // type=int32, k=2, v=16
    // Should be skipped.
    0x08, 0x10,
  };
  // clang-format on

  decoder.set_handler(&handler);
  EXPECT_EQ(decoder.Decode(std::as_bytes(std::span(encoded_proto))),
            Status::Cancelled());
  EXPECT_EQ(handler.field_one, 42);
  EXPECT_EQ(handler.field_three, 1111);
}

}  // namespace
}  // namespace pw::protobuf
