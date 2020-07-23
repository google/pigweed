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

#include "pw_hdlc_lite/encoder.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "gtest/gtest.h"
#include "pw_stream/memory_stream.h"

using std::byte;

template <typename... Args>
constexpr std::array<byte, sizeof...(Args)> MakeBytes(Args... args) noexcept {
  return {static_cast<byte>(args)...};
}

namespace pw::hdlc_lite {
namespace {
// Size of the in-memory buffer to use for this test.
constexpr size_t kSinkBufferSize = 15;

TEST(Encoder, FrameFormatTest_1BytePayload) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 1> test_array = MakeBytes(0x41);
  constexpr std::array<byte, 5> expected_array =
      MakeBytes(0x7E, 0x41, 0x15, 0xB9, 0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 5u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, FrameFormatTest_EmptyPayload) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 4> expected_array =
      MakeBytes(0x7E, 0xFF, 0xFF, 0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(std::span<byte>(), memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 4u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, FrameFormatTest_9BytePayload) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 9> test_array =
      MakeBytes(0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39);
  constexpr std::array<byte, 13> expected_array = MakeBytes(0x7E,
                                                            0x31,
                                                            0x32,
                                                            0x33,
                                                            0x34,
                                                            0x35,
                                                            0x36,
                                                            0x37,
                                                            0x38,
                                                            0x39,
                                                            0xB1,
                                                            0x29,
                                                            0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 13u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, EncodingMultiplePayloads) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 1> test_array = MakeBytes(0x41);
  constexpr std::array<byte, 5> expected_array_1 =
      MakeBytes(0x7E, 0x41, 0x15, 0xB9, 0x7E);
  constexpr std::array<byte, 10> expected_array_2 =
      MakeBytes(0x7E, 0x41, 0x15, 0xB9, 0x7E, 0x7E, 0x41, 0x15, 0xB9, 0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 5u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array_1.data(),
                        memory_writer.bytes_written()),
            0);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 10u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array_2.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, EscapingTest_0x7D) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 1> test_array = MakeBytes(0x7D);
  constexpr std::array<byte, 6> expected_array =
      MakeBytes(0x7E, 0x7D, 0x5D, 0xCA, 0x4E, 0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 6u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, EscapingTest_0x7E) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 1> test_array = MakeBytes(0x7E);
  constexpr std::array<byte, 7> expected_array =
      MakeBytes(0x7E, 0x7D, 0x5E, 0xA9, 0x7D, 0x5E, 0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 7u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, EscapingTest_Mix) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 7> test_array =
      MakeBytes(0x7E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x7E);
  constexpr std::array<byte, 14> expected_array = MakeBytes(0x7E,
                                                            0x7D,
                                                            0x5E,
                                                            0x7B,
                                                            0x61,
                                                            0x62,
                                                            0x63,
                                                            0x7D,
                                                            0x5D,
                                                            0x7D,
                                                            0x5E,
                                                            0x49,
                                                            0xE5,
                                                            0x7E);

  EXPECT_TRUE(EncodeAndWritePayload(test_array, memory_writer).ok());
  EXPECT_EQ(memory_writer.bytes_written(), 14u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(Encoder, WriterErrorTest) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  constexpr std::array<byte, 12> test_array = MakeBytes(
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x40, 0x41);

  EXPECT_FALSE(EncodeAndWritePayload(test_array, memory_writer).ok());
}

}  // namespace
}  // namespace pw::hdlc_lite
