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
  EXPECT_EQ(status, Status::Ok());
  EXPECT_EQ(memory_writer.bytes_written(), sizeof(kExpectedStruct));
}  // namespace

TEST(MemoryWriter, ValidateContents) {
  MemoryWriter memory_writer(memory_buffer);
  EXPECT_TRUE(
      memory_writer.Write(&kExpectedStruct, sizeof(kExpectedStruct)).ok());

  std::span<const std::byte> written_data = memory_writer.WrittenData();
  EXPECT_EQ(written_data.size_bytes(), sizeof(kExpectedStruct));
  TestStruct temp;
  std::memcpy(&temp, written_data.data(), written_data.size_bytes());
  EXPECT_EQ(memcmp(&temp, &kExpectedStruct, sizeof(kExpectedStruct)), 0);
}

TEST(MemoryWriter, MultipleWrites) {
  constexpr size_t kTempBufferSize = 72;
  std::byte buffer[kTempBufferSize] = {};

  for (std::byte& value : memory_buffer) {
    value = std::byte(0);
  }
  MemoryWriter memory_writer(memory_buffer);

  size_t counter = 0;
  while (memory_writer.ConservativeWriteLimit() >= kTempBufferSize) {
    for (size_t i = 0; i < sizeof(buffer); ++i) {
      buffer[i] = std::byte(counter++);
    }
    EXPECT_EQ(memory_writer.Write(std::span(buffer)), Status::Ok());
  }

  EXPECT_GT(memory_writer.ConservativeWriteLimit(), 0u);
  EXPECT_LT(memory_writer.ConservativeWriteLimit(), kTempBufferSize);

  EXPECT_EQ(memory_writer.Write(std::span(buffer)),
            Status::ResourceExhausted());
  EXPECT_EQ(memory_writer.bytes_written(), counter);

  counter = 0;
  for (const std::byte& value : memory_writer.WrittenData()) {
    EXPECT_EQ(value, std::byte(counter++));
  }
}

TEST(MemoryWriter, FullWriter) {
  constexpr size_t kTempBufferSize = 32;
  std::byte buffer[kTempBufferSize] = {};
  const int fill_byte = 0x25;
  memset(buffer, fill_byte, sizeof(buffer));

  for (std::byte& value : memory_buffer) {
    value = std::byte(0);
  }
  MemoryWriter memory_writer(memory_buffer);

  while (memory_writer.ConservativeWriteLimit() > 0) {
    size_t bytes_to_write =
        std::min(sizeof(buffer), memory_writer.ConservativeWriteLimit());
    EXPECT_EQ(memory_writer.Write(std::span(buffer, bytes_to_write)),
              Status::Ok());
  }

  EXPECT_EQ(memory_writer.ConservativeWriteLimit(), 0u);

  EXPECT_EQ(memory_writer.Write(std::span(buffer)), Status::OutOfRange());
  EXPECT_EQ(memory_writer.bytes_written(), memory_buffer.size());

  for (const std::byte& value : memory_writer.WrittenData()) {
    EXPECT_EQ(value, std::byte(fill_byte));
  }
}

TEST(MemoryWriter, EmptyData) {
  std::byte buffer[5] = {};

  MemoryWriter memory_writer(memory_buffer);
  EXPECT_EQ(memory_writer.Write(buffer, 0), Status::Ok());
  EXPECT_EQ(memory_writer.bytes_written(), 0u);
}

TEST(MemoryWriter, ValidateContents_SingleByteWrites) {
  MemoryWriter memory_writer(memory_buffer);
  EXPECT_TRUE(memory_writer.Write(std::byte{0x01}).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 1u);
  EXPECT_EQ(memory_writer.data()[0], std::byte{0x01});

  EXPECT_TRUE(memory_writer.Write(std::byte{0x7E}).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 2u);
  EXPECT_EQ(memory_writer.data()[1], std::byte{0x7E});
}

#define TESTING_CHECK_FAILURES_IS_SUPPORTED 0
#if TESTING_CHECK_FAILURES_IS_SUPPORTED

// TODO(amontanez): Ensure that this test triggers an assert.
TEST(MemoryWriter, NullPointer) {
  MemoryWriter memory_writer(memory_buffer);
  memory_writer.Write(nullptr, 21);
}

// TODO(davidrogers): Ensure that this test triggers an assert.
TEST(MemoryReader, NullSpan) {
  ByteSpan dest(nullptr, 5);
  MemoryReader memory_reader(memory_buffer);
  memory_reader.Read(dest);
}

// TODO(davidrogers): Ensure that this test triggers an assert.
TEST(MemoryReader, NullPointer) {
  MemoryReader memory_reader(memory_buffer);
  memory_reader.Read(nullptr, 21);
}

#endif  // TESTING_CHECK_FAILURES_IS_SUPPORTED

TEST(MemoryReader, SingleFullRead) {
  constexpr size_t kTempBufferSize = 32;

  std::array<std::byte, kTempBufferSize> source;
  std::array<std::byte, kTempBufferSize> dest;

  uint8_t counter = 0;
  for (std::byte& value : source) {
    value = std::byte(counter++);
  }

  MemoryReader memory_reader(source);

  // Read exactly the available bytes.
  EXPECT_EQ(memory_reader.ConservativeReadLimit(), dest.size());
  Result<ByteSpan> result = memory_reader.Read(dest);
  EXPECT_EQ(result.status(), Status::Ok());
  EXPECT_EQ(result.value().size_bytes(), dest.size());

  ASSERT_EQ(source.size(), result.value().size_bytes());
  for (size_t i = 0; i < source.size(); i++) {
    EXPECT_EQ(source[i], result.value()[i]);
  }

  // Shoud be no byte remaining.
  EXPECT_EQ(memory_reader.ConservativeReadLimit(), 0u);
  result = memory_reader.Read(dest);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(MemoryReader, EmptySpanRead) {
  constexpr size_t kTempBufferSize = 32;
  std::array<std::byte, kTempBufferSize> source;

  // Use a span with nullptr and zero length;
  ByteSpan dest(nullptr, 0);
  EXPECT_EQ(dest.size_bytes(), 0u);

  MemoryReader memory_reader(source);

  // Read exactly the available bytes.
  Result<ByteSpan> result = memory_reader.Read(dest);
  EXPECT_EQ(result.status(), Status::Ok());
  EXPECT_EQ(result.value().size_bytes(), 0u);
  EXPECT_EQ(result.value().data(), dest.data());

  // Shoud be original bytes remaining.
  EXPECT_EQ(memory_reader.ConservativeReadLimit(), source.size());
}

TEST(MemoryReader, SinglePartialRead) {
  constexpr size_t kTempBufferSize = 32;
  std::array<std::byte, kTempBufferSize> source;
  std::array<std::byte, kTempBufferSize * 2> dest;

  uint8_t counter = 0;
  for (std::byte& value : source) {
    value = std::byte(counter++);
  }

  MemoryReader memory_reader(source);

  // Try and read double the bytes available. Use the pointer/size version of
  // the API.
  Result<ByteSpan> result = memory_reader.Read(dest.data(), dest.size());
  EXPECT_EQ(result.status(), Status::Ok());
  EXPECT_EQ(result.value().size_bytes(), source.size());

  ASSERT_EQ(source.size(), result.value().size_bytes());
  for (size_t i = 0; i < source.size(); i++) {
    EXPECT_EQ(source[i], result.value()[i]);
  }

  // Shoud be no byte remaining.
  EXPECT_EQ(memory_reader.ConservativeReadLimit(), 0u);
  result = memory_reader.Read(dest);
  EXPECT_EQ(result.status(), Status::OutOfRange());
}

TEST(MemoryReader, MultipleReads) {
  constexpr size_t kTempBufferSize = 32;

  std::array<std::byte, kTempBufferSize * 5> source;
  std::array<std::byte, kTempBufferSize> dest;

  uint8_t counter = 0;

  for (std::byte& value : source) {
    value = std::byte(counter++);
  }

  MemoryReader memory_reader(source);

  size_t source_chunk_base = 0;

  while (memory_reader.ConservativeReadLimit() > 0) {
    size_t read_limit = memory_reader.ConservativeReadLimit();

    // Try and read a chunk of bytes.
    Result<ByteSpan> result = memory_reader.Read(dest);
    EXPECT_EQ(result.status(), Status::Ok());
    EXPECT_EQ(result.value().size_bytes(), dest.size());
    EXPECT_EQ(memory_reader.ConservativeReadLimit(),
              read_limit - result.value().size_bytes());

    // Verify the chunk of byte that was read.
    for (size_t i = 0; i < result.value().size_bytes(); i++) {
      EXPECT_EQ(source[source_chunk_base + i], result.value()[i]);
    }
    source_chunk_base += result.value().size_bytes();
  }
}

}  // namespace
}  // namespace pw::stream
