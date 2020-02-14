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

#include "pw_kvs/alignment.h"

#include <string_view>

#include "gtest/gtest.h"

namespace pw::kvs {
namespace {

using std::byte;

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

constexpr std::string_view kData =
    "123456789_123456789_123456789_123456789_123456789_"   //  50
    "123456789_123456789_123456789_123456789_123456789_";  // 100

const span<const byte> kBytes = as_bytes(span(kData));

TEST(AlignedWriter, VaryingLengthWriteCalls) {
  static constexpr size_t kAlignment = 10;

  // The output function checks that the data is properly aligned and matches
  // the expected value (should always be 123456789_...).
  OutputToFunction output([](span<const byte> data) {
    EXPECT_EQ(data.size() % kAlignment, 0u);
    EXPECT_EQ(kData.substr(0, data.size()),
              std::string_view(reinterpret_cast<const char*>(data.data()),
                               data.size()));
    return StatusWithSize(data.size());
  });

  AlignedWriterBuffer<32> writer(kAlignment, output);

  // Write values smaller than the alignment.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(0, 1)).status());
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(1, 9)).status());

  // Write values larger than the alignment but smaller than the buffer.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(10, 11)).status());

  // Exactly fill the remainder of the buffer.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(21, 11)).status());

  // Fill the buffer more than once.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(32, 66)).status());

  // Write nothing.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(98, 0)).status());

  // Write the remaining data.
  EXPECT_EQ(Status::OK, writer.Write(kBytes.subspan(98, 2)).status());

  auto result = writer.Flush();
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(kData.size(), result.size());
}

TEST(AlignedWriter, DestructorFlushes) {
  static size_t called_with_bytes;

  called_with_bytes = 0;

  OutputToFunction output([](span<const byte> data) {
    called_with_bytes += data.size();
    return StatusWithSize(data.size());
  });

  {
    AlignedWriterBuffer<64> writer(3, output);
    writer.Write(as_bytes(span("What is this?")));
    EXPECT_EQ(called_with_bytes, 0u);  // Buffer not full; no output yet.
  }

  EXPECT_EQ(called_with_bytes, AlignUp(sizeof("What is this?"), 3));
}

}  // namespace
}  // namespace pw::kvs
