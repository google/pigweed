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

#include "pw_allocator/buffer.h"

#include <array>
#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::allocator::GetAlignedSubspan;
using pw::allocator::IsWithin;

TEST(BufferTest, GetAlignedSubspanWhenAligned) {
  alignas(16) std::array<std::byte, 256> bytes{};
  pw::Result<pw::ByteSpan> result = GetAlignedSubspan(bytes, 16);
  ASSERT_EQ(result.status(), pw::OkStatus());
  pw::ByteSpan aligned = result.value();
  EXPECT_EQ(bytes.data(), aligned.data());
  EXPECT_EQ(bytes.size(), aligned.size());
}

TEST(BufferTest, GetAlignedSubspanWhenUnaligned) {
  alignas(16) std::array<std::byte, 256> buffer{};
  pw::ByteSpan bytes(buffer);
  bytes = bytes.subspan(1);
  pw::Result<pw::ByteSpan> result = GetAlignedSubspan(bytes, 16);
  ASSERT_EQ(result.status(), pw::OkStatus());
  pw::ByteSpan aligned = result.value();
  EXPECT_EQ(bytes.data() + 15, aligned.data());
  EXPECT_EQ(bytes.size() - 15, aligned.size());
}

TEST(BufferTest, GetAlignedSubspanWhenEmpty) {
  pw::ByteSpan bytes;
  pw::Result<pw::ByteSpan> result = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
}

TEST(BufferTest, GetAlignedSubspanWhenTooSmall) {
  alignas(16) std::array<std::byte, 16> buffer{};
  pw::ByteSpan bytes(buffer);
  bytes = bytes.subspan(1);
  pw::Result<pw::ByteSpan> result = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(result.status(), pw::Status::ResourceExhausted());
}

TEST(BufferTest, IsWithin) {
  std::array<std::byte, 256> bytes{};
  void* ptr = bytes.data() + 32;
  size_t size = 32;
  EXPECT_TRUE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenOverlappingStart) {
  std::array<std::byte, 256> buffer{};
  pw::ByteSpan bytes(buffer);
  void* ptr = bytes.data();
  size_t size = 32;
  bytes = bytes.subspan(1);
  EXPECT_FALSE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenOverlappingEnd) {
  std::array<std::byte, 256> buffer{};
  pw::ByteSpan bytes(buffer);
  void* ptr = bytes.data() + 224;
  size_t size = 32;
  bytes = bytes.subspan(0, 255);
  EXPECT_FALSE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenDisjoint) {
  std::array<std::byte, 256> buffer{};
  pw::ByteSpan bytes(buffer);
  void* ptr = bytes.data();
  size_t size = 32;
  bytes = bytes.subspan(64);
  EXPECT_FALSE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenNull) {
  std::array<std::byte, 256> bytes{};
  void* ptr = nullptr;
  size_t size = 32;
  EXPECT_FALSE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenZeroSize) {
  std::array<std::byte, 256> bytes{};
  void* ptr = bytes.data();
  size_t size = 0;
  EXPECT_TRUE(IsWithin(ptr, size, bytes));
}

TEST(BufferTest, IsWithinWhenEmpty) {
  std::array<std::byte, 256> bytes{};
  void* ptr = bytes.data();
  size_t size = 32;
  EXPECT_FALSE(IsWithin(ptr, size, pw::ByteSpan()));
}

}  // namespace
