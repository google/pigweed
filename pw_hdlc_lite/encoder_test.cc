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

#define EXPECT_ENCODER_WROTE(...)                                           \
  do {                                                                      \
    constexpr auto expected_data = (__VA_ARGS__);                           \
    EXPECT_EQ(writer_.bytes_written(), expected_data.size());               \
    EXPECT_EQ(                                                              \
        std::memcmp(                                                        \
            writer_.data(), expected_data.data(), writer_.bytes_written()), \
        0);                                                                 \
  } while (0)

class WriteUnnumberedFrame : public ::testing::Test {
 protected:
  WriteUnnumberedFrame() : writer_(buffer_) {}

  stream::MemoryWriter writer_;
  std::array<byte, 32> buffer_;
};

constexpr byte kUnnumberedControl = byte{0x3};

TEST_F(WriteUnnumberedFrame, EmptyPayload) {
  ASSERT_EQ(Status::Ok(), WriteUIFrame(kAddress, std::span<byte>(), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag, kAddress, kUnnumberedControl, uint32_t{0x141BE378}, kFlag));
}

TEST_F(WriteUnnumberedFrame, OneBytePayload) {
  ASSERT_EQ(Status::Ok(), WriteUIFrame(kAddress, bytes::String("A"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag, kAddress, kUnnumberedControl, 'A', uint32_t{0x8D137C66}, kFlag));
}

TEST_F(WriteUnnumberedFrame, OneBytePayload_Escape0x7d) {
  ASSERT_EQ(Status::Ok(),
            WriteUIFrame(kAddress, bytes::Array<0x7d>(), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     kEscape,
                                     byte{0x7d} ^ byte{0x20},
                                     uint32_t{0xA27C00E1},
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, OneBytePayload_Escape0x7E) {
  ASSERT_EQ(Status::Ok(),
            WriteUIFrame(kAddress, bytes::Array<0x7e>(), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     kEscape,
                                     byte{0x7e} ^ byte{0x20},
                                     uint32_t{0x3B75515B},
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, AddressNeedsEscaping) {
  ASSERT_EQ(Status::Ok(), WriteUIFrame(0x7d, bytes::String("A"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kEscape,
                                     byte{0x5d},
                                     kUnnumberedControl,
                                     'A',
                                     uint32_t{0x899E00D4},
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, Crc32NeedsEscaping) {
  ASSERT_EQ(Status::Ok(), WriteUIFrame(kAddress, bytes::String("a"), writer_));

  // The CRC-32 is 0xB67D5CAE, so the 0x7D must be escaped.
  constexpr auto expected_crc32 = bytes::Array<0xae, 0x5c, 0x7d, 0x5d, 0xb6>();
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     bytes::String("a"),
                                     expected_crc32,
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, MultiplePayloads) {
  ASSERT_EQ(Status::Ok(),
            WriteUIFrame(kAddress, bytes::String("ABC"), writer_));
  ASSERT_EQ(Status::Ok(),
            WriteUIFrame(kAddress, bytes::String("DEF"), writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     bytes::String("ABC"),
                                     uint32_t{0x06575377},
                                     kFlag,
                                     kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     bytes::String("DEF"),
                                     uint32_t{0x3FB7F3D4},
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, PayloadWithNoEscapes) {
  ASSERT_EQ(
      Status::Ok(),
      WriteUIFrame(kAddress, bytes::String("1995 toyota corolla"), writer_));

  EXPECT_ENCODER_WROTE(bytes::Concat(kFlag,
                                     kAddress,
                                     kUnnumberedControl,
                                     bytes::String("1995 toyota corolla"),
                                     uint32_t{0x56560172},
                                     kFlag));
}

TEST_F(WriteUnnumberedFrame, PayloadWithMultipleEscapes) {
  ASSERT_EQ(
      Status::Ok(),
      WriteUIFrame(kAddress,
                   bytes::Array<0x7E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x7E>(),
                   writer_));
  EXPECT_ENCODER_WROTE(bytes::Concat(
      kFlag,
      kAddress,
      kUnnumberedControl,
      bytes::
          Array<0x7D, 0x5E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x5D, 0x7D, 0x5E>(),
      uint32_t{0x950257BD},
      kFlag));
}

TEST_F(WriteUnnumberedFrame, WriterError) {
  constexpr auto data = bytes::Initialized<sizeof(buffer_)>(0x7e);
  EXPECT_EQ(Status::ResourceExhausted(), WriteUIFrame(kAddress, data, writer_));
}

}  // namespace
}  // namespace pw::hdlc_lite
