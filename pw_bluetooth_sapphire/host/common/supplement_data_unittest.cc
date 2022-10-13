// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/supplement_data.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"

namespace bt {
namespace {

TEST(SupplementDataTest, ReaderEmptyData) {
  BufferView empty;
  SupplementDataReader reader(empty);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(SupplementDataTest, ReaderMalformedData) {
  // TLV length exceeds the size of the payload
  StaticByteBuffer bytes0(0x01);
  SupplementDataReader reader(bytes0);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  StaticByteBuffer bytes(0x05, 0x00, 0x00, 0x00, 0x00);
  reader = SupplementDataReader(bytes);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  // TLV length is 0. This is not considered malformed. Data should be valid but
  // should not return more data.
  bytes = StaticByteBuffer(0x00, 0x00, 0x00, 0x00, 0x00);
  reader = SupplementDataReader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());

  // First field is valid, second field is not.
  DataType type;
  BufferView data;
  bytes = StaticByteBuffer(0x02, 0x00, 0x00, 0x02, 0x00);
  reader = SupplementDataReader(bytes);
  EXPECT_FALSE(reader.is_valid());
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));

  // First field is valid, second field has length 0.
  bytes = StaticByteBuffer(0x02, 0x00, 0x00, 0x00, 0x00);
  reader = SupplementDataReader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_TRUE(reader.HasMoreData());
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));
}

TEST(SupplementDataTest, ReaderParseFields) {
  StaticByteBuffer bytes(0x02, 0x01, 0x00, 0x05, 0x09, 'T', 'e', 's', 't');
  SupplementDataReader reader(bytes);
  EXPECT_TRUE(reader.is_valid());
  EXPECT_TRUE(reader.HasMoreData());

  DataType type;
  BufferView data;
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_EQ(DataType::kFlags, type);
  EXPECT_EQ(1u, data.size());
  EXPECT_TRUE(ContainersEqual(StaticByteBuffer(0x00), data));

  EXPECT_TRUE(reader.HasMoreData());
  EXPECT_TRUE(reader.GetNextField(&type, &data));
  EXPECT_EQ(DataType::kCompleteLocalName, type);
  EXPECT_EQ(4u, data.size());
  EXPECT_TRUE(ContainersEqual(std::string("Test"), data));

  EXPECT_FALSE(reader.HasMoreData());
  EXPECT_FALSE(reader.GetNextField(&type, &data));
}

// Helper for computing the size of a string literal at compile time. sizeof()
// would have worked too but that counts the null character.
template <std::size_t N>
constexpr size_t StringSize(char const (&str)[N]) {
  return N - 1;
}

TEST(SupplementDataTest, WriteFieldAndVerifyContents) {
  constexpr char kValue0[] = "value zero";
  constexpr char kValue1[] = "value one";
  constexpr char kValue2[] = "value two";
  constexpr char kValue3[] = "value three";

  // Have just enough space for the first three values (+ 6 for 2 extra octets
  // for each TLV field).
  constexpr char kBufferSize = StringSize(kValue0) + StringSize(kValue1) + StringSize(kValue2) + 6;
  StaticByteBuffer<kBufferSize> buffer;

  SupplementDataWriter writer(&buffer);
  EXPECT_EQ(0u, writer.bytes_written());

  // We write malformed values here for testing purposes.
  EXPECT_TRUE(writer.WriteField(DataType::kFlags, BufferView(kValue0)));
  EXPECT_EQ(StringSize(kValue0) + 2, writer.bytes_written());

  EXPECT_TRUE(writer.WriteField(DataType::kShortenedLocalName, BufferView(kValue1)));
  EXPECT_EQ(StringSize(kValue0) + 2 + StringSize(kValue1) + 2, writer.bytes_written());

  // Trying to write kValue3 should fail because there isn't enough room left in
  // the buffer.
  EXPECT_FALSE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue3)));

  // Writing kValue2 should fill up the buffer.
  EXPECT_TRUE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue2)));
  EXPECT_FALSE(writer.WriteField(DataType::kCompleteLocalName, BufferView(kValue3)));
  EXPECT_EQ(buffer.size(), writer.bytes_written());

  // Verify the contents.
  DataType type;
  BufferView value;
  SupplementDataReader reader(buffer);
  EXPECT_TRUE(reader.is_valid());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kFlags, type);
  EXPECT_EQ(kValue0, value.AsString());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kShortenedLocalName, type);
  EXPECT_EQ(kValue1, value.AsString());

  EXPECT_TRUE(reader.GetNextField(&type, &value));
  EXPECT_EQ(DataType::kCompleteLocalName, type);
  EXPECT_EQ(kValue2, value.AsString());

  EXPECT_FALSE(reader.GetNextField(&type, &value));
}

}  // namespace
}  // namespace bt
