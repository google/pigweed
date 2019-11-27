// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

#include "pw_varint/varint.h"

#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <limits>

#include "gtest/gtest.h"

namespace pw::varint {
namespace {

class Varint : public ::testing::Test {
 protected:
  Varint() : buffer_{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'} {}
  uint8_t buffer_[10];
};

TEST_F(Varint, EncodeSizeUnsigned32_SmallSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(0), buffer_));
  EXPECT_EQ(0u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(1), buffer_));
  EXPECT_EQ(1u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(2), buffer_));
  EXPECT_EQ(2u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeUnsigned32_LargeSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(63), buffer_));
  EXPECT_EQ(63u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(64), buffer_));
  EXPECT_EQ(64u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(126), buffer_));
  EXPECT_EQ(126u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT32_C(127), buffer_));
  EXPECT_EQ(127u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeUnsigned32_MultiByte) {
  ASSERT_EQ(2u, EncodeVarint(UINT32_C(128), buffer_));
  EXPECT_EQ(std::memcmp("\x80\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(UINT32_C(129), buffer_));
  EXPECT_EQ(std::memcmp("\x81\x01", buffer_, 2), 0);

  ASSERT_EQ(5u,
            EncodeVarint(std::numeric_limits<uint32_t>::max() - 1, buffer_));
  EXPECT_EQ(std::memcmp("\xfe\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(5u, EncodeVarint(std::numeric_limits<uint32_t>::max(), buffer_));
  EXPECT_EQ(std::memcmp("\xff\xff\xff\xff\x0f", buffer_, 5), 0);
}

TEST_F(Varint, EncodeSizeSigned32_SmallSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(INT32_C(0), buffer_));
  EXPECT_EQ(0u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(-1), buffer_));
  EXPECT_EQ(1u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(1), buffer_));
  EXPECT_EQ(2u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(-2), buffer_));
  EXPECT_EQ(3u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(2), buffer_));
  EXPECT_EQ(4u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeSigned32_LargeSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(INT32_C(-63), buffer_));
  EXPECT_EQ(125u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(63), buffer_));
  EXPECT_EQ(126u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT32_C(-64), buffer_));
  EXPECT_EQ(127u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeSigned32_MultiByte) {
  ASSERT_EQ(2u, EncodeVarint(INT32_C(64), buffer_));
  EXPECT_EQ(std::memcmp("\x80\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(INT32_C(-65), buffer_));
  EXPECT_EQ(std::memcmp("\x81\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(INT32_C(65), buffer_));
  EXPECT_EQ(std::memcmp("\x82\x01", buffer_, 2), 0);

  ASSERT_EQ(5u, EncodeVarint(std::numeric_limits<int32_t>::min(), buffer_));
  EXPECT_EQ(std::memcmp("\xff\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(5u, EncodeVarint(std::numeric_limits<int32_t>::max(), buffer_));
  EXPECT_EQ(std::memcmp("\xfe\xff\xff\xff\x0f", buffer_, 5), 0);
}

TEST_F(Varint, EncodeSizeUnsigned64_SmallSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(0), buffer_));
  EXPECT_EQ(0u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(1), buffer_));
  EXPECT_EQ(1u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(2), buffer_));
  EXPECT_EQ(2u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeUnsigned64_LargeSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(63), buffer_));
  EXPECT_EQ(63u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(64), buffer_));
  EXPECT_EQ(64u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(126), buffer_));
  EXPECT_EQ(126u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(UINT64_C(127), buffer_));
  EXPECT_EQ(127u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeUnsigned64_MultiByte) {
  ASSERT_EQ(2u, EncodeVarint(UINT64_C(128), buffer_));
  EXPECT_EQ(std::memcmp("\x80\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(UINT64_C(129), buffer_));
  EXPECT_EQ(std::memcmp("\x81\x01", buffer_, 2), 0);

  ASSERT_EQ(5u,
            EncodeVarint(std::numeric_limits<uint32_t>::max() - 1, buffer_));
  EXPECT_EQ(std::memcmp("\xfe\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(5u, EncodeVarint(std::numeric_limits<uint32_t>::max(), buffer_));
  EXPECT_EQ(std::memcmp("\xff\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(10u,
            EncodeVarint(std::numeric_limits<uint64_t>::max() - 1, buffer_));
  EXPECT_EQ(
      std::memcmp("\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", buffer_, 10), 0);

  ASSERT_EQ(10u, EncodeVarint(std::numeric_limits<uint64_t>::max(), buffer_));
  EXPECT_EQ(
      std::memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", buffer_, 10), 0);
}

TEST_F(Varint, EncodeSizeSigned64_SmallSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(INT64_C(0), buffer_));
  EXPECT_EQ(0u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(-1), buffer_));
  EXPECT_EQ(1u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(1), buffer_));
  EXPECT_EQ(2u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(-2), buffer_));
  EXPECT_EQ(3u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(2), buffer_));
  EXPECT_EQ(4u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeSigned64_LargeSingleByte) {
  ASSERT_EQ(1u, EncodeVarint(INT64_C(-63), buffer_));
  EXPECT_EQ(125u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(63), buffer_));
  EXPECT_EQ(126u, buffer_[0]);
  ASSERT_EQ(1u, EncodeVarint(INT64_C(-64), buffer_));
  EXPECT_EQ(127u, buffer_[0]);
}

TEST_F(Varint, EncodeSizeSigned64_MultiByte) {
  ASSERT_EQ(2u, EncodeVarint(INT64_C(64), buffer_));
  EXPECT_EQ(std::memcmp("\x80\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(INT64_C(-65), buffer_));
  EXPECT_EQ(std::memcmp("\x81\x01", buffer_, 2), 0);
  ASSERT_EQ(2u, EncodeVarint(INT64_C(65), buffer_));
  EXPECT_EQ(std::memcmp("\x82\x01", buffer_, 2), 0);

  ASSERT_EQ(
      5u,
      EncodeVarint(static_cast<int64_t>(std::numeric_limits<int32_t>::min()),
                   buffer_));
  EXPECT_EQ(std::memcmp("\xff\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(
      5u,
      EncodeVarint(static_cast<int64_t>(std::numeric_limits<int32_t>::max()),
                   buffer_));
  EXPECT_EQ(std::memcmp("\xfe\xff\xff\xff\x0f", buffer_, 5), 0);

  ASSERT_EQ(10u, EncodeVarint(std::numeric_limits<int64_t>::min(), buffer_));
  EXPECT_EQ(
      std::memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01", buffer_, 10), 0);

  ASSERT_EQ(10u, EncodeVarint(std::numeric_limits<int64_t>::max(), buffer_));
  EXPECT_EQ(
      std::memcmp("\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", buffer_, 10), 0);
}

TEST_F(Varint, EncodeDecodeSigned32) {
  // Set the increment to 1 to test every number (this is slow)
  static constexpr int kIncrement = 1'000'009;

  int32_t i = std::numeric_limits<int32_t>::min();
  while (true) {
    size_t encoded = EncodeVarint(i, buffer_);

    int64_t result;
    size_t decoded = DecodeVarint(buffer_, &result);

    EXPECT_EQ(encoded, decoded);
    ASSERT_EQ(i, result);

    if (i > std::numeric_limits<int32_t>::max() - kIncrement) {
      break;
    }

    i += kIncrement;
  }
}

TEST_F(Varint, EncodeDecodeUnsigned32) {
  // Set the increment to 1 to test every number (this is slow)
  static constexpr int kIncrement = 1'000'009;

  uint32_t i = 0;
  while (true) {
    size_t encoded = EncodeVarint(i, buffer_);

    uint64_t result;
    size_t decoded = DecodeVarint(buffer_, &result);

    EXPECT_EQ(encoded, decoded);
    ASSERT_EQ(i, result);

    if (i > std::numeric_limits<uint32_t>::max() - kIncrement) {
      break;
    }

    i += kIncrement;
  }
}

template <size_t kStringSize>
auto MakeBuffer(const char (&data)[kStringSize]) {
  constexpr size_t kSizeBytes = kStringSize - 1;
  static_assert(kSizeBytes <= 10, "Varint arrays never need be larger than 10");

  std::array<uint8_t, kSizeBytes> array;
  std::memcpy(array.data(), data, kSizeBytes);
  return array;
}

TEST(VarintDecode, DecodeSigned64_SingleByte) {
  int64_t value = -1234;

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x00"), &value), 1u);
  EXPECT_EQ(value, 0);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x01"), &value), 1u);
  EXPECT_EQ(value, -1);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x02"), &value), 1u);
  EXPECT_EQ(value, 1);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x03"), &value), 1u);
  EXPECT_EQ(value, -2);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x04"), &value), 1u);
  EXPECT_EQ(value, 2);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x04"), &value), 1u);
  EXPECT_EQ(value, 2);
}

TEST(VarintDecode, DecodeSigned64_MultiByte) {
  int64_t value = -1234;

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x80\x01"), &value), 2u);
  EXPECT_EQ(value, 64);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x81\x01"), &value), 2u);
  EXPECT_EQ(value, -65);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\x82\x01"), &value), 2u);
  EXPECT_EQ(value, 65);

  EXPECT_EQ(DecodeVarint(MakeBuffer("\xff\xff\xff\xff\x0f"), &value), 5u);
  EXPECT_EQ(value, std::numeric_limits<int32_t>::min());

  EXPECT_EQ(DecodeVarint(MakeBuffer("\xfe\xff\xff\xff\x0f"), &value), 5u);
  EXPECT_EQ(value, std::numeric_limits<int32_t>::max());

  EXPECT_EQ(DecodeVarint(MakeBuffer("\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"),
                         &value),
            10u);
  EXPECT_EQ(value, std::numeric_limits<int64_t>::min());

  EXPECT_EQ(DecodeVarint(MakeBuffer("\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"),
                         &value),
            10u);
  EXPECT_EQ(value, std::numeric_limits<int64_t>::max());
}

}  // namespace
}  // namespace pw::varint
