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

#include "pw_bytes/alignment.h"

#include "pw_unit_test/framework.h"

namespace pw {
namespace {

TEST(AlignUp, Zero) {
  EXPECT_EQ(0u, AlignUp(0, 1));
  EXPECT_EQ(0u, AlignUp(0, 2));
  EXPECT_EQ(0u, AlignUp(0, 15));
}

TEST(AlignUp, Aligned) {
  for (size_t i = 1; i < 130; ++i) {
    EXPECT_EQ(i, AlignUp(i, i));
    EXPECT_EQ(2 * i, AlignUp(2 * i, i));
    EXPECT_EQ(3 * i, AlignUp(3 * i, i));
  }
}

TEST(AlignUp, NonAligned_PowerOf2) {
  EXPECT_EQ(32u, AlignUp(1, 32));
  EXPECT_EQ(32u, AlignUp(31, 32));
  EXPECT_EQ(64u, AlignUp(33, 32));
  EXPECT_EQ(64u, AlignUp(45, 32));
  EXPECT_EQ(64u, AlignUp(63, 32));
  EXPECT_EQ(128u, AlignUp(127, 32));
}

TEST(AlignUp, NonAligned_NonPowerOf2) {
  EXPECT_EQ(2u, AlignUp(1, 2));

  EXPECT_EQ(15u, AlignUp(1, 15));
  EXPECT_EQ(15u, AlignUp(14, 15));
  EXPECT_EQ(30u, AlignUp(16, 15));
}

TEST(AlignDown, Zero) {
  EXPECT_EQ(0u, AlignDown(0, 1));
  EXPECT_EQ(0u, AlignDown(0, 2));
  EXPECT_EQ(0u, AlignDown(0, 15));
}

TEST(AlignDown, Aligned) {
  for (size_t i = 1; i < 130; ++i) {
    EXPECT_EQ(i, AlignDown(i, i));
    EXPECT_EQ(2 * i, AlignDown(2 * i, i));
    EXPECT_EQ(3 * i, AlignDown(3 * i, i));
  }
}

TEST(AlignDown, NonAligned_PowerOf2) {
  EXPECT_EQ(0u, AlignDown(1, 32));
  EXPECT_EQ(0u, AlignDown(31, 32));
  EXPECT_EQ(32u, AlignDown(33, 32));
  EXPECT_EQ(32u, AlignDown(45, 32));
  EXPECT_EQ(32u, AlignDown(63, 32));
  EXPECT_EQ(96u, AlignDown(127, 32));
}

TEST(AlignDown, NonAligned_NonPowerOf2) {
  EXPECT_EQ(0u, AlignDown(1, 2));

  EXPECT_EQ(0u, AlignDown(1, 15));
  EXPECT_EQ(0u, AlignDown(14, 15));
  EXPECT_EQ(15u, AlignDown(16, 15));
}

TEST(Padding, Zero) {
  EXPECT_EQ(0u, Padding(0, 1));
  EXPECT_EQ(0u, Padding(0, 2));
  EXPECT_EQ(0u, Padding(0, 15));
}

TEST(Padding, Aligned) {
  for (size_t i = 1; i < 130; ++i) {
    EXPECT_EQ(0u, Padding(i, i));
    EXPECT_EQ(0u, Padding(2 * i, i));
    EXPECT_EQ(0u, Padding(3 * i, i));
  }
}

TEST(Padding, NonAligned_PowerOf2) {
  EXPECT_EQ(31u, Padding(1, 32));
  EXPECT_EQ(1u, Padding(31, 32));
  EXPECT_EQ(31u, Padding(33, 32));
  EXPECT_EQ(19u, Padding(45, 32));
  EXPECT_EQ(1u, Padding(63, 32));
  EXPECT_EQ(1u, Padding(127, 32));
}

TEST(Padding, NonAligned_NonPowerOf2) {
  EXPECT_EQ(1u, Padding(1, 2));

  EXPECT_EQ(14u, Padding(1, 15));
  EXPECT_EQ(1u, Padding(14, 15));
  EXPECT_EQ(14u, Padding(16, 15));
}

TEST(GetAlignedSubspan, AlignedSpanIsUnchanged) {
  alignas(16) std::array<std::byte, 256> bytes{};
  pw::ByteSpan aligned = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(bytes.data(), aligned.data());
  EXPECT_EQ(bytes.size(), aligned.size());
}

TEST(GetAlignedSubspan, UnalignedSpanIsAdvanced) {
  alignas(16) std::array<std::byte, 256> buffer{};
  pw::ByteSpan bytes(buffer);
  bytes = bytes.subspan(1);
  pw::ByteSpan aligned = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(bytes.data() + 15, aligned.data());
  EXPECT_EQ(bytes.size() - 15, aligned.size());
}

TEST(GetAlignedSubspan, EmptySpanReturnsEmpty) {
  pw::ByteSpan bytes;
  pw::ByteSpan aligned = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(aligned.size(), 0u);
}

TEST(GetAlignedSubspan, SpanTooSmallForAlignmentReturnsEmptySpan) {
  alignas(16) std::array<std::byte, 16> buffer{};
  pw::ByteSpan bytes(buffer);
  bytes = bytes.subspan(1);
  pw::ByteSpan aligned = GetAlignedSubspan(bytes, 16);
  EXPECT_EQ(aligned.size(), 0u);
}

}  // namespace
}  // namespace pw
