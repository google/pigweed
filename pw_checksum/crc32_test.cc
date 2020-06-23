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
#include "pw_checksum/crc32.h"

#include <span>
#include <string_view>

#include "gtest/gtest.h"

namespace pw::checksum {
namespace {

// The expected CRC32 values were calculated using
//
//   http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
//
// with polynomial 0x4C11DB7, initial value 0xFFFFFFFF.

constexpr uint8_t kBytes[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint8_t kBytesPart0[] = {1, 2, 3, 4, 5};
constexpr uint8_t kBytesPart1[] = {6, 7, 8, 9};
constexpr uint32_t kBufferCrc = 0x40EFAB9E;

constexpr std::string_view kString =
    "In the beginning the Universe was created. This has made a lot of "
    "people very angry and been widely regarded as a bad move.";
constexpr uint32_t kStringCrc = 0x9EC87F88;

TEST(Crc32, Empty) {
  EXPECT_EQ(Crc32(std::span<std::byte>()), ~kCrc32InitialValue);
}

TEST(Crc32, ByteByByte) {
  uint32_t crc;
  crc = Crc32(std::byte{kBytes[0]});
  for (size_t i = 1; i < sizeof(kBytes); i++) {
    crc = Crc32(std::byte{kBytes[i]}, crc);
  }
  EXPECT_EQ(crc, kBufferCrc);
}

TEST(Crc32, Buffer) {
  EXPECT_EQ(Crc32(as_bytes(std::span(kBytes))), kBufferCrc);
}

TEST(Crc32, BufferAppend) {
  uint32_t crc = Crc32(as_bytes(std::span(kBytesPart0)));
  EXPECT_EQ(Crc32(as_bytes(std::span(kBytesPart1)), crc), kBufferCrc);
}

TEST(Crc32, String) {
  EXPECT_EQ(Crc32(as_bytes(std::span(kString))), kStringCrc);
}

extern "C" uint32_t CallChecksumCrc32(const void* data, size_t size_bytes);
extern "C" uint32_t CallChecksumCrc32Append(const void* data,
                                            size_t size_bytes);

TEST(Crc32FromC, Buffer) {
  EXPECT_EQ(CallChecksumCrc32(kBytes, sizeof(kBytes)), kBufferCrc);
}

TEST(Crc32FromC, String) {
  EXPECT_EQ(CallChecksumCrc32(kString.data(), kString.size()), kStringCrc);
}

TEST(Crc32AppendFromC, Buffer) {
  EXPECT_EQ(CallChecksumCrc32(kBytes, sizeof(kBytes)), kBufferCrc);
}

TEST(Crc32AppendFromC, String) {
  EXPECT_EQ(CallChecksumCrc32Append(kString.data(), kString.size()),
            kStringCrc);
}

}  // namespace
}  // namespace pw::checksum
