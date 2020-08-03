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

#include "pw_hdlc_lite/decoder.h"

#include <array>
#include <cstddef>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_result/result.h"
#include "pw_stream/memory_stream.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

TEST(DecoderBuffer, 1BytePayload) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 5> data_frame = bytes::Array<0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::memcmp(result.value().data(),
                        expected_payload.data(),
                        expected_payload.size()),
            0);
}

TEST(DecoderBuffer, 0BytePayload) {
  std::array<byte, 4> data_frame = bytes::Array<0x7E, 0xFF, 0xFF, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.value().empty());
}

TEST(DecoderBuffer, 9BytePayload) {
  std::array<byte, 9> expected_payload =
      bytes::Array<0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39>();
  std::array<byte, 13> data_frame = bytes::Array<0x7E,
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
                                                 0x7E>();
  DecoderBuffer<15> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::memcmp(result.value().data(),
                        expected_payload.data(),
                        expected_payload.size()),
            0);
}

TEST(DecoderBuffer, MultiFrameDecoding) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 10> multiple_data_frames = bytes::
      Array<0x7E, 0x41, 0x15, 0xB9, 0x7E, 0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  DecoderBuffer<12> decoder;

  for (size_t i = 0; i < multiple_data_frames.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(multiple_data_frames[i]);
    if (i == 4u || i == 9u) {
      EXPECT_EQ(std::memcmp(result.value().data(),
                            expected_payload.data(),
                            expected_payload.size()),
                0);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, UnescapingDataFrame_0x7D) {
  std::array<byte, 1> expected_payload = bytes::Array<0x7D>();
  std::array<byte, 6> data_frame =
      bytes::Array<0x7E, 0x7D, 0x5D, 0xCA, 0x4E, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::memcmp(result.value().data(),
                        expected_payload.data(),
                        expected_payload.size()),
            0);
}

TEST(DecoderBuffer, UnescapingDataFrame_0x7E) {
  std::array<byte, 1> expected_payload = bytes::Array<0x7E>();
  std::array<byte, 7> data_frame =
      bytes::Array<0x7E, 0x7D, 0x5E, 0xA9, 0x7D, 0x5E, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::memcmp(result.value().data(),
                        expected_payload.data(),
                        expected_payload.size()),
            0);
}

TEST(DecoderBuffer, UnescapingDataFrame_Mix) {
  std::array<byte, 7> expected_payload =
      bytes::Array<0x7E, 0x7B, 0x61, 0x62, 0x63, 0x7D, 0x7E>();
  ;
  std::array<byte, 14> data_frame = bytes::Array<0x7E,
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
                                                 0x7E>();
  DecoderBuffer<15> decoder;

  for (size_t i = 0; i < data_frame.size() - 1; i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    EXPECT_EQ(result.status(), Status::UNAVAILABLE);
  }
  Result<ConstByteSpan> result =
      decoder.AddByte(data_frame[data_frame.size() - 1]);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(std::memcmp(result.value().data(),
                        expected_payload.data(),
                        expected_payload.size()),
            0);
}

TEST(DecoderBuffer, IncorrectCRC) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 5> incorrect_data_frame =
      bytes::Array<0x7E, 0x41, 0x15, 0xB8, 0x7E>();
  std::array<byte, 5> correct_data_frame =
      bytes::Array<0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < incorrect_data_frame.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(incorrect_data_frame[i]);
    if (i == incorrect_data_frame.size() - 1) {
      EXPECT_EQ(result.status(), Status::DATA_LOSS);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }

  for (size_t i = 0; i < correct_data_frame.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(correct_data_frame[i]);
    if (i == 4u) {
      EXPECT_TRUE(result.ok());
      EXPECT_EQ(std::memcmp(result.value().data(),
                            expected_payload.data(),
                            expected_payload.size()),
                0);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, BufferSizeLessThan2EdgeCase) {
  std::array<byte, 3> data_frame = bytes::Array<0x7E, 0xFF, 0x7E>();
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frame.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    if (i == data_frame.size() - 1) {
      EXPECT_EQ(result.status(), Status::DATA_LOSS);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, BufferOutOfSpace_SkipsRestOfFrame) {
  std::array<byte, 5> data_frame = bytes::Array<0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  DecoderBuffer<2> decoder;

  for (size_t i = 0; i < data_frame.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frame[i]);
    if (i >= 3u) {
      EXPECT_EQ(result.status(), Status::RESOURCE_EXHAUSTED);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, BufferOutOfSpace_SkipsRestOfFrameAndDecodesNext) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 11> data_frames = bytes::
      Array<0x7E, 0x41, 0x42, 0x15, 0xB9, 0x7E, 0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  DecoderBuffer<3> decoder;

  for (size_t i = 0; i < data_frames.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frames[i]);
    if (i == 4u || i == 5u) {
      EXPECT_EQ(result.status(), Status::RESOURCE_EXHAUSTED);
    } else if (i == 10u) {
      EXPECT_EQ(std::memcmp(result.value().data(),
                            expected_payload.data(),
                            expected_payload.size()),
                0);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, UnexpectedStartingByte) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 12> data_frames = bytes::Array<0x7E,
                                                  0x41,
                                                  0x15,
                                                  0xB9,
                                                  0x7E,  // End of 1st Packet
                                                  0xAA,  // Garbage bytes
                                                  0xAA,  // Garbage bytes
                                                  0x7E,
                                                  0x41,
                                                  0x15,
                                                  0xB9,
                                                  0x7E>();  // End of 2nd Packet
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frames.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frames[i]);
    if (i == 4u || i == 11u) {
      EXPECT_TRUE(result.ok());
      EXPECT_EQ(std::memcmp(result.value().data(),
                            expected_payload.data(),
                            expected_payload.size()),
                0);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

TEST(DecoderBuffer, RecoveringMissingFrameDelimiterCase) {
  std::array<byte, 1> expected_payload = bytes::Array<0x41>();
  std::array<byte, 12> data_frames =
      bytes::Array<0x7E,
                   0x41,
                   0x15,
                   0xB9,
                   0x7E,  // End of 1st Packet
                   0x41,
                   0x7E,  // End of Packet with missing start frame delimiter
                   0x7E,
                   0x41,
                   0x15,
                   0xB9,
                   0x7E>();  // End of 2nd Packet
  DecoderBuffer<10> decoder;

  for (size_t i = 0; i < data_frames.size(); i++) {
    Result<ConstByteSpan> result = decoder.AddByte(data_frames[i]);
    if (i == 4u || i == 11u) {
      EXPECT_TRUE(result.ok());
      EXPECT_EQ(std::memcmp(result.value().data(),
                            expected_payload.data(),
                            expected_payload.size()),
                0);
    } else {
      EXPECT_EQ(result.status(), Status::UNAVAILABLE);
    }
  }
}

}  // namespace
}  // namespace pw::hdlc_lite
