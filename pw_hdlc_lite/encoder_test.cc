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
#include "pw_bytes/array.h"
#include "pw_hdlc_lite_private/protocol.h"
#include "pw_stream/memory_stream.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

constexpr uint8_t kAddress = 0x7B;  // 123
constexpr byte kControl = byte{0};

class WriteInfoFrame : public ::testing::Test {
 protected:
  WriteInfoFrame() : writer_(buffer_) {}

  stream::MemoryWriter writer_;
  std::array<byte, 32> buffer_;
};

#define EXPECT_ENCODER_WROTE(...)                                           \
  do {                                                                      \
    constexpr auto expected_data = (__VA_ARGS__);                           \
    EXPECT_EQ(writer_.bytes_written(), expected_data.size());               \
    EXPECT_EQ(                                                              \
        std::memcmp(                                                        \
            writer_.data(), expected_data.data(), writer_.bytes_written()), \
        0);                                                                 \
  } while (0)

TEST_F(WriteInfoFrame, EmptyPayload) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, std::span<byte>(), writer_));
  EXPECT_ENCODER_WROTE(
      bytes::Concat(kFlag, kAddress, kControl, uint32_t{0x8D12B2C2}, kFlag));
}

TEST_F(WriteInfoFrame, OneBytePayload) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::String("A"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag, kAddress, kControl, 'A', uint32_t{0xA63E2FA5}, kFlag));
}

TEST_F(WriteInfoFrame, OneBytePayload_Escape0x7d) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::Array<0x7d>(), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kControl,
                                     kEscape,
                                     byte{0x7d} ^ byte{0x20},
                                     uint32_t{0x89515322},
                                     kFlag));
}

TEST_F(WriteInfoFrame, OneBytePayload_Escape0x7E) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::Array<0x7e>(), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kControl,
                                     kEscape,
                                     byte{0x7e} ^ byte{0x20},
                                     uint32_t{0x10580298},
                                     kFlag));
}

TEST_F(WriteInfoFrame, AddressNeedsEscaping) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(0x7d, bytes::String("A"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag, kEscape, byte{0x5d}, kControl, 'A', uint32_t{0xA2B35317}, kFlag));
}

TEST_F(WriteInfoFrame, Crc32NeedsEscaping) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::String("abcdefg"), writer_));

  // The CRC-32 is 0x38B9FC7E, so the 0x7E must be escaped.
  constexpr auto expected_crc32 = bytes::Array<0x7d, 0x5e, 0xfc, 0xb9, 0x38>();
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kControl,
                                     bytes::String("abcdefg"),
                                     expected_crc32,
                                     kFlag));
}

TEST_F(WriteInfoFrame, MultiplePayloads) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::String("ABC"), writer_));
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(kAddress, bytes::String("DEF"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kControl,
                                     bytes::String("ABC"),
                                     uint32_t{0x14E2FC99},
                                     kFlag,
                                     kFlag,
                                     kAddress,
                                     kControl,
                                     bytes::String("DEF"),
                                     uint32_t{0x2D025C3A},
                                     kFlag));
}

TEST_F(WriteInfoFrame, PayloadWithNoEscapes) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(
                kAddress, bytes::String("123456789012345678901234"), writer_));

  // Fill the memory writer's buffer.
  ASSERT_EQ(writer_.bytes_written(), buffer_.size());

  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kControl,
                                     bytes::String("123456789012345678901234"),
                                     uint32_t{0x50AA35EC},
                                     kFlag));
}

TEST_F(WriteInfoFrame, PayloadWithMultipleEscapes) {
  ASSERT_EQ(Status::Ok(),
            WriteInformationFrame(
                kAddress,
                bytes::Array<0x7E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x7E>(),
                writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag,
      kAddress,
      kControl,
      bytes::
          Array<0x7D, 0x5E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x5D, 0x7D, 0x5E>(),
      uint32_t{0x1B8D505E},
      kFlag));
}

TEST_F(WriteInfoFrame, WriterError) {
  constexpr auto data = bytes::Initialized<sizeof(buffer_)>(0x7e);

  EXPECT_EQ(Status::ResourceExhausted(),
            WriteInformationFrame(kAddress, data, writer_));
}

}  // namespace
}  // namespace pw::hdlc_lite
