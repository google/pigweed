// Copyright 2023 The Pigweed Authors
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

#include "pw_rpc_transport/simple_framing.h"

#include <algorithm>
#include <array>
#include <random>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::rpc {
namespace {

constexpr size_t kMaxPacketSize = 256;

struct TestParams {
  size_t packet_size = 0;
  size_t max_frame_size = 0;
};

constexpr std::array<TestParams, 8> kTestCases = {
    // Packet fits in one frame.
    TestParams{.packet_size = 5, .max_frame_size = 100},
    // Typical parameters for RPC packet and mailbox frame size.
    TestParams{.packet_size = 100, .max_frame_size = 128},
    // Smallest packet.
    TestParams{.packet_size = 1, .max_frame_size = 16},
    // Small packet, small frame.
    TestParams{.packet_size = 16, .max_frame_size = 5},
    // Odd-sized packet, small frame.
    TestParams{.packet_size = 77, .max_frame_size = 16},
    // Frame size and packet size off by one.
    TestParams{.packet_size = 11, .max_frame_size = 10},
    // Almost at the limit.
    TestParams{.packet_size = kMaxPacketSize - 1,
               .max_frame_size = kMaxPacketSize - 2},
    // At the limit.
    TestParams{.packet_size = kMaxPacketSize,
               .max_frame_size = kMaxPacketSize}};

void MakePacket(ByteSpan dst_buffer) {
  static uint32_t rg_seed = 0x123;
  unsigned char c = 0;
  for (auto& i : dst_buffer) {
    i = std::byte{c++};
  }
  std::mt19937 rg(rg_seed++);
  std::shuffle(dst_buffer.begin(), dst_buffer.end(), rg);
}

void CopyFrame(RpcFrame frame, std::vector<std::byte>& dst) {
  std::copy(frame.header.begin(), frame.header.end(), std::back_inserter(dst));
  std::copy(
      frame.payload.begin(), frame.payload.end(), std::back_inserter(dst));
}

TEST(SimpleRpcFrameEncodeDecodeTest, EncodeThenDecode) {
  for (auto test_case : kTestCases) {
    const size_t packet_size = test_case.packet_size;
    const size_t max_frame_size = test_case.max_frame_size;
    PW_LOG_INFO("EncodeThenDecode: packet_size = %d, max_frame_size = %d",
                static_cast<int>(packet_size),
                static_cast<int>(max_frame_size));

    std::vector<std::byte> src(packet_size);
    MakePacket(src);

    std::vector<std::byte> encoded;
    std::vector<std::byte> decoded;

    SimpleRpcPacketEncoder<kMaxPacketSize> encoder;

    ASSERT_EQ(encoder.Encode(src,
                             max_frame_size,
                             [&encoded](RpcFrame& frame) {
                               CopyFrame(frame, encoded);
                               return OkStatus();
                             }),
              OkStatus());

    SimpleRpcPacketDecoder<kMaxPacketSize> decoder;

    ASSERT_EQ(decoder.Decode(encoded,
                             [&decoded](ConstByteSpan packet) {
                               std::copy(packet.begin(),
                                         packet.end(),
                                         std::back_inserter(decoded));
                             }),
              OkStatus());

    EXPECT_TRUE(std::equal(src.begin(), src.end(), decoded.begin()));
  }
}

TEST(SimpleRpcFrameEncodeDecodeTest, OneByteAtTimeDecoding) {
  for (auto test_case : kTestCases) {
    const size_t packet_size = test_case.packet_size;
    const size_t max_frame_size = test_case.max_frame_size;
    PW_LOG_INFO("EncodeThenDecode: packet_size = %d, max_frame_size = %d",
                static_cast<int>(packet_size),
                static_cast<int>(max_frame_size));

    std::vector<std::byte> src(packet_size);
    MakePacket(src);

    std::vector<std::byte> encoded;
    std::vector<std::byte> decoded;

    SimpleRpcPacketEncoder<kMaxPacketSize> encoder;

    ASSERT_EQ(encoder.Encode(src,
                             max_frame_size,
                             [&encoded](RpcFrame& frame) {
                               CopyFrame(frame, encoded);
                               return OkStatus();
                             }),
              OkStatus());

    SimpleRpcPacketDecoder<kMaxPacketSize> decoder;

    for (std::byte b : encoded) {
      auto buffer_span = span(&b, 1);
      ASSERT_EQ(decoder.Decode(buffer_span,
                               [&decoded](ConstByteSpan packet) {
                                 std::copy(packet.begin(),
                                           packet.end(),
                                           std::back_inserter(decoded));
                               }),
                OkStatus());
    }

    EXPECT_TRUE(std::equal(src.begin(), src.end(), decoded.begin()));
  }
}

TEST(SimpleRpcFrameTest, MissingFirstFrame) {
  // Sends two packets, the first packet is missing its first frame. The decoder
  // ignores the remaining frames of the first packet but still picks up the
  // second packet.
  constexpr size_t kPacketSize = 77;
  constexpr size_t kMaxFrameSize = 16;

  std::vector<std::byte> src1(kPacketSize);
  MakePacket(src1);

  std::vector<std::byte> src2(kPacketSize);
  MakePacket(src2);

  std::vector<std::byte> decoded;

  SimpleRpcPacketEncoder<kMaxPacketSize> encoder;
  struct EncodeState {
    size_t frame_counter = 0;
    std::vector<std::byte> encoded;
  } state;

  ASSERT_EQ(encoder.Encode(src1,
                           kMaxFrameSize,
                           [&state](RpcFrame& frame) {
                             state.frame_counter++;
                             if (state.frame_counter > 1) {
                               // Skip the first frame.
                               CopyFrame(frame, state.encoded);
                             }
                             return OkStatus();
                           }),
            OkStatus());

  ASSERT_EQ(encoder.Encode(src2,
                           kMaxFrameSize,
                           [&state](RpcFrame& frame) {
                             CopyFrame(frame, state.encoded);
                             return OkStatus();
                           }),
            OkStatus());

  SimpleRpcPacketDecoder<kMaxPacketSize> decoder;

  ASSERT_EQ(decoder.Decode(state.encoded,
                           [&decoded](ConstByteSpan packet) {
                             std::copy(packet.begin(),
                                       packet.end(),
                                       std::back_inserter(decoded));
                           }),
            OkStatus());

  EXPECT_TRUE(std::equal(src2.begin(), src2.end(), decoded.begin()));
}

TEST(SimpleRpcFrameTest, MissingInternalFrame) {
  // Sends two packets, the first packet is missing its second frame. The
  // decoder ignores the remaining frames of the first packet and the second
  // packet as well but eventually stumbles upon the frame header in the third
  // packet and processes that packet.
  constexpr size_t kPacketSize = 77;
  constexpr size_t kMaxFrameSize = 16;

  std::vector<std::byte> src1(kPacketSize);
  MakePacket(src1);

  std::vector<std::byte> src2(kPacketSize);
  MakePacket(src2);

  std::vector<std::byte> src3(kPacketSize);
  MakePacket(src3);

  std::vector<std::byte> decoded;

  SimpleRpcPacketEncoder<kMaxPacketSize> encoder;
  struct EncodeState {
    size_t frame_counter = 0;
    std::vector<std::byte> encoded;
  } encode_state;

  ASSERT_EQ(encoder.Encode(src1,
                           kMaxFrameSize,
                           [&encode_state](RpcFrame& frame) {
                             encode_state.frame_counter++;
                             if (encode_state.frame_counter != 2) {
                               // Skip the second frame.
                               CopyFrame(frame, encode_state.encoded);
                             }
                             return OkStatus();
                           }),
            OkStatus());

  ASSERT_EQ(encoder.Encode(src2,
                           kMaxFrameSize,
                           [&encode_state](RpcFrame& frame) {
                             CopyFrame(frame, encode_state.encoded);
                             return OkStatus();
                           }),
            OkStatus());

  ASSERT_EQ(encoder.Encode(src3,
                           kMaxFrameSize,
                           [&encode_state](RpcFrame& frame) {
                             CopyFrame(frame, encode_state.encoded);
                             return OkStatus();
                           }),
            OkStatus());

  SimpleRpcPacketDecoder<kMaxPacketSize> decoder;

  // First packet is decoded but it doesn't have correct bytes, as one of its
  // frames has never been received. Second packet is not received because its
  // header has been consumed by the first packet. By that point the decoder
  // knows that something is wrong and tries to recover as soon as it receives
  // bytes that look as the valid header. So we eventually receive the third
  // packet and it is correct.
  struct DecodeState {
    std::vector<std::byte> decoded1;
    std::vector<std::byte> decoded2;
    size_t packet_counter = 0;
  } decode_state;

  ASSERT_EQ(
      decoder.Decode(encode_state.encoded,
                     [&decode_state](ConstByteSpan packet) {
                       decode_state.packet_counter++;
                       if (decode_state.packet_counter == 1) {
                         std::copy(packet.begin(),
                                   packet.end(),
                                   std::back_inserter(decode_state.decoded1));
                       }
                       if (decode_state.packet_counter == 2) {
                         std::copy(packet.begin(),
                                   packet.end(),
                                   std::back_inserter(decode_state.decoded2));
                       }
                     }),
      OkStatus());

  EXPECT_EQ(decode_state.packet_counter, 2ul);

  EXPECT_EQ(decode_state.decoded1.size(), src1.size());
  EXPECT_FALSE(
      std::equal(src1.begin(), src1.end(), decode_state.decoded1.begin()));

  EXPECT_TRUE(
      std::equal(src3.begin(), src3.end(), decode_state.decoded2.begin()));
}

TEST(SimpleRpcPacketEncoder, PacketTooBig) {
  SimpleRpcPacketEncoder<kMaxPacketSize> encoder;
  constexpr size_t kMaxFrameSize = 100;
  std::array<std::byte, kMaxPacketSize + 1> src{};

  EXPECT_EQ(
      encoder.Encode(src, kMaxFrameSize, [](RpcFrame&) { return OkStatus(); }),
      Status::FailedPrecondition());
}

TEST(SimpleRpcPacketEncoder, MaxFrameSizeTooSmall) {
  SimpleRpcPacketEncoder<kMaxPacketSize> encoder;
  std::array<std::byte, kMaxPacketSize> src{};

  EXPECT_EQ(encoder.Encode(
                src, encoder.kHeaderSize, [](RpcFrame&) { return OkStatus(); }),
            Status::FailedPrecondition());

  EXPECT_EQ(
      encoder.Encode(
          src, encoder.kHeaderSize + 1, [](RpcFrame&) { return OkStatus(); }),
      OkStatus());
}

TEST(SimpleRpcFrameTest, EncoderBufferLargerThanDecoderBuffer) {
  constexpr size_t kLargePacketSize = 150;
  constexpr size_t kSmallPacketSize = 120;
  constexpr size_t kMaxFrameSize = 16;

  // Decoder isn't able to receive the whole packet because it needs to be
  // buffered but the internal buffer is too small; the packet is thus
  // discarded. The second packet is received without issues as it's small
  // enough to fit in the decoder buffer.
  constexpr size_t kEncoderMaxPacketSize = 256;
  constexpr size_t kDecoderMaxPacketSize = 128;

  std::vector<std::byte> src1(kLargePacketSize);
  MakePacket(src1);

  std::vector<std::byte> src2(kSmallPacketSize);
  MakePacket(src1);

  std::vector<std::byte> encoded;
  std::vector<std::byte> decoded;

  SimpleRpcPacketEncoder<kEncoderMaxPacketSize> encoder;

  ASSERT_EQ(encoder.Encode(src1,
                           kMaxFrameSize,
                           [&encoded](RpcFrame& frame) {
                             CopyFrame(frame, encoded);
                             return OkStatus();
                           }),
            OkStatus());

  ASSERT_EQ(encoder.Encode(src2,
                           kMaxFrameSize,
                           [&encoded](RpcFrame& frame) {
                             CopyFrame(frame, encoded);
                             return OkStatus();
                           }),
            OkStatus());

  SimpleRpcPacketDecoder<kDecoderMaxPacketSize> decoder;

  // We have to decode piecemeal here because otherwise the decoder can just
  // pluck the packet from `encoded` without internally buffering it.
  for (std::byte b : encoded) {
    auto buffer_span = span(&b, 1);
    ASSERT_EQ(decoder.Decode(buffer_span,
                             [&decoded](ConstByteSpan packet) {
                               std::copy(packet.begin(),
                                         packet.end(),
                                         std::back_inserter(decoded));
                             }),
              OkStatus());
  }

  EXPECT_TRUE(std::equal(src2.begin(), src2.end(), decoded.begin()));
}

}  // namespace
}  // namespace pw::rpc
