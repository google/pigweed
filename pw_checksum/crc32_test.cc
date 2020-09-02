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
#include "pw_bytes/array.h"

namespace pw::checksum {
namespace {

// The expected CRC32 values were calculated using
//
//   http://www.sunshine2k.de/coding/javascript/crc/crc_js.html
//
// with polynomial 0x4C11DB7, initial value 0xFFFFFFFF.

constexpr auto kBytes = bytes::Array<1, 2, 3, 4, 5, 6, 7, 8, 9>();
constexpr auto kBytesPart0 = bytes::Array<1, 2, 3, 4, 5>();
constexpr auto kBytesPart1 = bytes::Array<6, 7, 8, 9>();
constexpr uint32_t kBufferCrc = 0x40EFAB9E;

constexpr std::string_view kString =
    "In the beginning the Universe was created. This has made a lot of "
    "people very angry and been widely regarded as a bad move.";
constexpr uint32_t kStringCrc = 0x9EC87F88;

TEST(Crc32, Empty) {
  EXPECT_EQ(Crc32::Calculate(std::span<std::byte>()), PW_CHECKSUM_EMPTY_CRC32);
}

TEST(Crc32, Buffer) {
  EXPECT_EQ(Crc32::Calculate(std::as_bytes(std::span(kBytes))), kBufferCrc);
}

TEST(Crc32, String) {
  EXPECT_EQ(Crc32::Calculate(std::as_bytes(std::span(kString))), kStringCrc);
}

TEST(Crc32Class, ByteByByte) {
  Crc32 crc;
  for (std::byte b : kBytes) {
    crc.Update(b);
  }
  EXPECT_EQ(crc.value(), kBufferCrc);
}

TEST(Crc32Class, Buffer) {
  Crc32 crc32;
  crc32.Update(std::as_bytes(std::span(kBytes)));
  EXPECT_EQ(crc32.value(), kBufferCrc);
}

TEST(Crc32Class, BufferAppend) {
  Crc32 crc32;
  crc32.Update(kBytesPart0);
  crc32.Update(kBytesPart1);
  EXPECT_EQ(crc32.value(), kBufferCrc);
}

TEST(Crc32Class, String) {
  Crc32 crc32;
  crc32.Update(std::as_bytes(std::span(kString)));
  EXPECT_EQ(crc32.value(), kStringCrc);
}

extern "C" uint32_t CallChecksumCrc32(const void* data, size_t size_bytes);
extern "C" uint32_t CallChecksumCrc32Append(const void* data,
                                            size_t size_bytes,
                                            uint32_t value);

TEST(Crc32FromC, Buffer) {
  EXPECT_EQ(CallChecksumCrc32(kBytes.data(), kBytes.size()), kBufferCrc);
}

TEST(Crc32FromC, String) {
  EXPECT_EQ(CallChecksumCrc32(kString.data(), kString.size()), kStringCrc);
}

TEST(Crc32AppendFromC, Buffer) {
  uint32_t crc = PW_CHECKSUM_EMPTY_CRC32;
  for (std::byte b : kBytes) {
    crc = CallChecksumCrc32Append(&b, 1, crc);
  }

  EXPECT_EQ(crc, kBufferCrc);
}

TEST(Crc32AppendFromC, String) {
  EXPECT_EQ(CallChecksumCrc32Append(
                kString.data(), kString.size(), PW_CHECKSUM_EMPTY_CRC32),
            kStringCrc);
}

}  // namespace
}  // namespace pw::checksum
