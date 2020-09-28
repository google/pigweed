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
#include "pw_hdlc_lite_private/protocol.h"

namespace pw::hdlc_lite {
namespace {

using std::byte;

TEST(Frame, Fields) {
  static constexpr auto kFrameData = bytes::String("1234\xa3\xe0\xe3\x9b");
  constexpr Frame frame(kFrameData);

  static_assert(frame.address() == unsigned{'1'});
  static_assert(frame.control() == byte{'2'});

  static_assert(frame.data().size() == 2u);
  static_assert(frame.data()[0] == byte{'3'});
  static_assert(frame.data()[1] == byte{'4'});
}

TEST(Decoder, Clear) {
  DecoderBuffer<8> decoder;

  // Process a partial packet
  decoder.Process(bytes::String("~1234abcd"),
                  [](const Result<Frame>&) { FAIL(); });

  decoder.clear();
  Status status = Status::Unknown();

  decoder.Process(
      bytes::String("~1234\xa3\xe0\xe3\x9b~"),
      [&status](const Result<Frame>& result) { status = result.status(); });

  EXPECT_EQ(Status::Ok(), status);
}

TEST(Decoder, ExactFit) {
  DecoderBuffer<8> decoder;

  for (byte b : bytes::String("~1234\xa3\xe0\xe3\x9b")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }
  auto result = decoder.Process(kFlag);
  ASSERT_EQ(Status::Ok(), result.status());
  ASSERT_EQ(result.value().data().size(), 2u);
  ASSERT_EQ(result.value().data()[0], byte{'3'});
  ASSERT_EQ(result.value().data()[1], byte{'4'});
}

TEST(Decoder, MinimumSizedBuffer) {
  DecoderBuffer<6> decoder;

  for (byte b : bytes::String("~12\xcd\x44\x53\x4f")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }

  auto result = decoder.Process(kFlag);
  ASSERT_EQ(Status::Ok(), result.status());
  EXPECT_EQ(result.value().data().size(), 0u);
}

TEST(Decoder, TooLargeForBuffer_ReportsResourceExhausted) {
  DecoderBuffer<8> decoder;

  for (byte b : bytes::String("~123456789")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }
  EXPECT_EQ(Status::ResourceExhausted(), decoder.Process(kFlag).status());

  for (byte b : bytes::String("~123456789012345678901234567890")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }
  EXPECT_EQ(Status::ResourceExhausted(), decoder.Process(kFlag).status());
}

TEST(Decoder, TooLargeForBuffer_StaysWithinBufferBoundaries) {
  std::array<byte, 16> buffer = bytes::Initialized<16>('?');

  Decoder decoder(std::span(buffer.data(), 8));

  for (byte b : bytes::String("~1234567890123456789012345678901234567890")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }

  for (size_t i = 8; i < buffer.size(); ++i) {
    ASSERT_EQ(byte{'?'}, buffer[i]);
  }

  EXPECT_EQ(Status::ResourceExhausted(), decoder.Process(kFlag).status());
}

TEST(Decoder, TooLargeForBuffer_DecodesNextFrame) {
  DecoderBuffer<8> decoder;

  for (byte b : bytes::String("~123456789012345678901234567890")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }
  EXPECT_EQ(Status::ResourceExhausted(), decoder.Process(kFlag).status());

  for (byte b : bytes::String("1234\xa3\xe0\xe3\x9b")) {
    EXPECT_EQ(Status::Unavailable(), decoder.Process(b).status());
  }
  EXPECT_EQ(Status::Ok(), decoder.Process(kFlag).status());
}

}  // namespace
}  // namespace pw::hdlc_lite
