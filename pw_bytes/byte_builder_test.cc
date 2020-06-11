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

#include "pw_bytes//byte_builder.h"

#include <array>
#include <cstddef>

#include "gtest/gtest.h"

using std::byte;

template <typename... Args>
constexpr std::array<byte, sizeof...(Args)> MakeBytes(Args... args) noexcept {
  return {static_cast<byte>(args)...};
}

namespace pw {
namespace {

TEST(ByteBuilder, EmptyBuffer_SizeAndMaxSizeAreCorrect) {
  ByteBuilder bb(span<byte>{});

  EXPECT_TRUE(bb.empty());
  EXPECT_EQ(0u, bb.size());
  EXPECT_EQ(0u, bb.max_size());
}

TEST(ByteBuilder, NonEmptyBufferOfSize0_SizeAndMaxSizeAreCorrect) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  EXPECT_TRUE(bb.empty());
  EXPECT_EQ(0u, bb.size());
  EXPECT_EQ(3u, bb.max_size());
}

TEST(ByteBuilder, Constructor_InsertsEmptyBuffer) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  EXPECT_TRUE(bb.empty());
}

TEST(ByteBuilder, EmptyBuffer_Append) {
  ByteBuilder bb(span<byte>{});
  EXPECT_TRUE(bb.empty());

  auto bytesTestLiteral = MakeBytes(0x04, 0x05);

  EXPECT_FALSE(bb.append(bytesTestLiteral.data(), 2).ok());
  EXPECT_EQ(0u, bb.size());
  EXPECT_EQ(0u, bb.max_size());
}

TEST(ByteBuilder, NonEmptyBufferOfSize0_Append) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);
  EXPECT_TRUE(bb.empty());

  auto bytesTestLiteral = MakeBytes(0x04, 0x05);

  EXPECT_TRUE(bb.append(bytesTestLiteral.data(), 2).ok());
  EXPECT_EQ(byte{0x04}, bb.data()[0]);
  EXPECT_EQ(byte{0x05}, bb.data()[1]);
}

TEST(ByteBuilder, NonEmptyBufferOfSize0_Append_Partial_NotResourceExhausted) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  EXPECT_TRUE(bb.empty());

  auto bytesTestLiteral = MakeBytes(0x04, 0x05, 0x06, 0x07);

  EXPECT_TRUE(bb.append(bytesTestLiteral.data(), 3).ok());
  EXPECT_EQ(byte{0x04}, bb.data()[0]);
  EXPECT_EQ(byte{0x05}, bb.data()[1]);
  EXPECT_EQ(byte{0x06}, bb.data()[2]);
}

TEST(ByteBuilder, NonEmptyBufferOfSize0_Append_Partial_ResourceExhausted) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  EXPECT_TRUE(bb.empty());

  auto bytesTestLiteral = MakeBytes(0x04, 0x05, 0x06, 0x07);

  EXPECT_FALSE(bb.append(bytesTestLiteral.data(), 4).ok());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.last_status());

  EXPECT_EQ(byte{0x04}, bb.data()[0]);
  EXPECT_EQ(byte{0x05}, bb.data()[1]);
  EXPECT_EQ(byte{0x06}, bb.data()[2]);

  EXPECT_EQ(bb.size(), bb.max_size());
  EXPECT_EQ(3u, bb.size());
}

TEST(ByteBuilder, Append_RepeatedBytes) {
  ByteBuffer<8> bb;
  EXPECT_TRUE(bb.empty());

  EXPECT_TRUE(bb.append(7, byte{0x04}).ok());

  for (size_t i = 0; i < 7; i++) {
    EXPECT_EQ(byte{0x04}, bb.data()[i]);
  }
}

TEST(ByteBuilder, Append_Bytes_Full) {
  ByteBuffer<8> bb;

  EXPECT_EQ(8u, bb.max_size() - bb.size());

  EXPECT_TRUE(bb.append(8, byte{0x04}).ok());

  for (size_t i = 0; i < 8; i++) {
    EXPECT_EQ(byte{0x04}, bb.data()[i]);
  }
}

TEST(ByteBuilder, Append_Bytes_Exhausted) {
  ByteBuffer<8> bb;

  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.append(9, byte{0x04}).status());

  for (size_t i = 0; i < 8; i++) {
    EXPECT_EQ(byte{0x04}, bb.data()[i]);
  }
}

TEST(ByteBuilder, Append_Partial) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<12> bb;

  EXPECT_TRUE(bb.append(buffer.data(), 2).ok());
  EXPECT_EQ(2u, bb.size());
  EXPECT_EQ(byte{0x01}, bb.data()[0]);
  EXPECT_EQ(byte{0x02}, bb.data()[1]);
}

TEST(ByteBuilder, EmptyBuffer_Resize_WritesNothing) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  bb.resize(0);
  EXPECT_TRUE(bb.ok());
}

TEST(ByteBuilder, EmptyBuffer_Resize_Larger_Fails) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuilder bb(buffer);

  bb.resize(1);

  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.append(9, byte{0x04}).status());
}

TEST(ByteBuilder, Resize_Smaller) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<8> bb;

  EXPECT_TRUE(bb.append(buffer.data(), 3).ok());

  bb.resize(1);
  EXPECT_TRUE(bb.ok());
  EXPECT_EQ(1u, bb.size());
  EXPECT_EQ(byte{0x01}, bb.data()[0]);
}

TEST(ByteBuilder, Resize_Clear) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<8> bb;

  EXPECT_TRUE(bb.append(buffer.data(), 3).ok());

  bb.resize(0);
  EXPECT_TRUE(bb.ok());
  EXPECT_EQ(0u, bb.size());
  EXPECT_TRUE(bb.empty());
}

TEST(ByteBuilder, Resize_Larger_Fails) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<8> bb;

  EXPECT_TRUE(bb.append(buffer.data(), 3).ok());

  EXPECT_EQ(3u, bb.size());
  bb.resize(5);
  EXPECT_EQ(3u, bb.size());
  EXPECT_EQ(bb.status(), Status::OUT_OF_RANGE);
}

TEST(ByteBuilder, Status_StartsOk) {
  ByteBuffer<16> bb;
  EXPECT_EQ(Status::OK, bb.status());
  EXPECT_EQ(Status::OK, bb.last_status());
}

TEST(ByteBuilder, Status_StatusAndLastStatusUpdate) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<2> bb;

  EXPECT_FALSE(bb.append(buffer.data(), 3).ok());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.status());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.last_status());

  bb.resize(4);
  EXPECT_EQ(Status::OUT_OF_RANGE, bb.status());
  EXPECT_EQ(Status::OUT_OF_RANGE, bb.last_status());

  EXPECT_FALSE(bb.append(buffer.data(), 0).ok());
  EXPECT_EQ(Status::OUT_OF_RANGE, bb.status());
  EXPECT_EQ(Status::OK, bb.last_status());
}

TEST(ByteBuilder, Status_ClearStatus_SetsStatuesToOk) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<2> bb;

  EXPECT_FALSE(bb.append(buffer.data(), 3).ok());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.status());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.last_status());

  bb.clear_status();
  EXPECT_EQ(Status::OK, bb.status());
  EXPECT_EQ(Status::OK, bb.last_status());
}

TEST(ByteBuilder, PushBack) {
  ByteBuffer<12> bb;
  bb.push_back(byte{0x01});
  EXPECT_EQ(Status::OK, bb.last_status());
  EXPECT_EQ(1u, bb.size());
  EXPECT_EQ(byte{0x01}, bb.data()[0]);
}

TEST(ByteBuilder, PushBack_Full) {
  ByteBuffer<1> bb;
  bb.push_back(byte{0x01});
  EXPECT_EQ(Status::OK, bb.last_status());
  EXPECT_EQ(1u, bb.size());
}

TEST(ByteBuilder, PushBack_Full_ResourceExhausted) {
  ByteBuffer<1> bb;
  bb.push_back(byte{0x01});
  bb.push_back(byte{0x01});

  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, bb.last_status());
  EXPECT_EQ(1u, bb.size());
}

TEST(ByteBuilder, PopBack) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<3> bb;

  bb.append(buffer.data(), 3);

  bb.pop_back();
  EXPECT_EQ(Status::OK, bb.last_status());
  EXPECT_EQ(2u, bb.size());
  EXPECT_EQ(byte{0x01}, bb.data()[0]);
  EXPECT_EQ(byte{0x02}, bb.data()[1]);
}

TEST(ByteBuilder, PopBack_Empty) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<3> bb;
  bb.append(buffer.data(), 3);

  bb.pop_back();
  bb.pop_back();
  bb.pop_back();
  EXPECT_EQ(Status::OK, bb.last_status());
  EXPECT_EQ(0u, bb.size());
  EXPECT_TRUE(bb.empty());
}

TEST(ByteBuffer, Assign) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<10> one;
  ByteBuffer<10> two;

  one.append(buffer.data(), 3);
  EXPECT_EQ(byte{0x01}, one.data()[0]);
  EXPECT_EQ(byte{0x02}, one.data()[1]);
  EXPECT_EQ(byte{0x03}, one.data()[2]);

  two = one;
  EXPECT_EQ(byte{0x01}, two.data()[0]);
  EXPECT_EQ(byte{0x02}, two.data()[1]);
  EXPECT_EQ(byte{0x03}, two.data()[2]);

  auto bytesTestLiteral = MakeBytes(0x04, 0x05, 0x06, 0x07);
  one.append(bytesTestLiteral.data(), 2);
  two.append(bytesTestLiteral.data(), 4);
  EXPECT_EQ(5u, one.size());
  EXPECT_EQ(7u, two.size());
  EXPECT_EQ(byte{0x04}, one.data()[3]);
  EXPECT_EQ(byte{0x05}, one.data()[4]);
  EXPECT_EQ(byte{0x04}, two.data()[3]);
  EXPECT_EQ(byte{0x05}, two.data()[4]);
  EXPECT_EQ(byte{0x06}, two.data()[5]);
  EXPECT_EQ(byte{0x07}, two.data()[6]);

  two.push_back(byte{0x01});
  two.push_back(byte{0x01});
  two.push_back(byte{0x01});
  two.push_back(byte{0x01});
  ASSERT_EQ(Status::RESOURCE_EXHAUSTED, two.status());
  ASSERT_EQ(Status::RESOURCE_EXHAUSTED, two.last_status());

  one = two;
  EXPECT_EQ(byte{0x01}, two.data()[7]);
  EXPECT_EQ(byte{0x01}, two.data()[8]);
  EXPECT_EQ(byte{0x01}, two.data()[9]);
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, one.status());
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, one.last_status());
}

TEST(ByteBuffer, CopyConstructFromSameSize) {
  ByteBuffer<10> one;
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);

  one.append(buffer.data(), 3);
  EXPECT_EQ(byte{0x01}, one.data()[0]);
  EXPECT_EQ(byte{0x02}, one.data()[1]);
  EXPECT_EQ(byte{0x03}, one.data()[2]);

  ByteBuffer<10> two(one);
  EXPECT_EQ(byte{0x01}, two.data()[0]);
  EXPECT_EQ(byte{0x02}, two.data()[1]);
  EXPECT_EQ(byte{0x03}, two.data()[2]);
}

TEST(ByteBuffer, CopyConstructFromSmaller) {
  std::array<byte, 3> buffer = MakeBytes(0x01, 0x02, 0x03);
  ByteBuffer<2> one;
  one.append(buffer.data(), 2);
  ByteBuffer<3> two(one);

  EXPECT_EQ(byte{0x01}, two.data()[0]);
  EXPECT_EQ(byte{0x02}, two.data()[1]);

  EXPECT_EQ(Status::OK, two.status());
}

}  // namespace
}  // namespace pw
