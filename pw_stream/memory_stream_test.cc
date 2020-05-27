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

#include "pw_stream/memory_stream.h"

#include "gtest/gtest.h"
#include "pw_preprocessor/compiler.h"

namespace pw::stream {
namespace {

// Size of the in-memory buffer to use for this test.
constexpr size_t kSinkBufferSize = 1013;

struct TestStruct {
  uint8_t day;
  uint8_t month;
  uint16_t year;
};

constexpr TestStruct kExpectedStruct = {.day = 18, .month = 5, .year = 2020};

std::array<std::byte, kSinkBufferSize> memory_buffer;

TEST(MemoryWriter, BytesWritten) {
  MemoryWriter memory_writer(memory_buffer);
  EXPECT_EQ(memory_writer.bytes_written(), 0u);
  Status status =
      memory_writer.Write(&kExpectedStruct, sizeof(kExpectedStruct));
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(memory_writer.bytes_written(), sizeof(kExpectedStruct));
}

TEST(MemoryWriter, ValidateContents) {
  MemoryWriter memory_writer(memory_buffer);
  EXPECT_TRUE(
      memory_writer.Write(&kExpectedStruct, sizeof(kExpectedStruct)).ok());

  span<const std::byte> written_data = memory_writer.WrittenData();
  EXPECT_EQ(written_data.size_bytes(), sizeof(kExpectedStruct));
  TestStruct temp;
  std::memcpy(&temp, written_data.data(), written_data.size_bytes());
  EXPECT_EQ(memcmp(&temp, &kExpectedStruct, sizeof(kExpectedStruct)), 0);
}

TEST(MemoryWriter, MultipleWrites) {
  constexpr size_t kTempBufferSize = 72;
  std::byte buffer[kTempBufferSize] = {};
  size_t counter = 0;

  MemoryWriter memory_writer(memory_buffer);

  do {
    for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = std::byte(counter++);
    }
  } while (memory_writer.Write(span(buffer)) != Status::RESOURCE_EXHAUSTED);

  // Ensure that we counted up to at least the sink buffer size. This can be
  // more since we write to the sink via in intermediate buffer.
  EXPECT_GE(counter, kSinkBufferSize);

  EXPECT_EQ(memory_writer.bytes_written(), kSinkBufferSize);

  counter = 0;
  for (const std::byte& value : memory_writer.WrittenData()) {
    EXPECT_EQ(value, std::byte(counter++));
  }
}

TEST(MemoryWriter, EmptyData) {
  std::byte buffer[5] = {};

  MemoryWriter memory_writer(memory_buffer);
  EXPECT_TRUE(memory_writer.Write(buffer, 0).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 0u);
}

#if CHECK_TEST_CRASHES

// TODO(amontanez): Ensure that this test triggers an assert.
TEST(MemoryWriter, NullPointer) {
  MemoryWriter memory_writer(memory_buffer);
  memory_writer.Write(nullptr, 21);
}

#endif  // CHECK_TEST_CRASHES

}  // namespace
}  // namespace pw::stream