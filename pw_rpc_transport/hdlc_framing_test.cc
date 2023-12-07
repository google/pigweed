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

#include "pw_rpc_transport/hdlc_framing.h"

#include <algorithm>
#include <array>

#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

TEST(HdlcRpc, EncodeThenDecode) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kPacketSize = 100;
  constexpr size_t kMaxFrameSize = 20;
  // Expecting six frames due to HDLC overhead.
  constexpr size_t kNumFramesExpected = 6;

  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;
  std::array<std::byte, kPacketSize> packet{};

  struct EncodeState {
    size_t num_frames = 0;
    size_t offset = 0;
    std::array<std::byte, 2 * kMaxPacketSize> encoded{};
  } state;

  std::fill(packet.begin(), packet.end(), std::byte{0x42});
  ASSERT_EQ(encoder.Encode(packet,
                           kMaxFrameSize,
                           [&state](RpcFrame& frame) {
                             state.num_frames++;
                             EXPECT_TRUE(frame.header.empty());
                             std::copy(frame.payload.begin(),
                                       frame.payload.end(),
                                       state.encoded.begin() + state.offset);
                             state.offset += frame.payload.size();
                             return OkStatus();
                           }),
            OkStatus());

  EXPECT_EQ(state.num_frames, kNumFramesExpected);

  std::array<std::byte, kMaxPacketSize> decoded{};
  HdlcRpcPacketDecoder<kMaxPacketSize> decoder;

  ASSERT_EQ(decoder.Decode(state.encoded,
                           [&decoded](ConstByteSpan packet_to_decode) {
                             std::copy(packet_to_decode.begin(),
                                       packet_to_decode.end(),
                                       decoded.begin());
                           }),
            OkStatus());

  EXPECT_TRUE(std::equal(packet.begin(), packet.end(), decoded.begin()));
}

TEST(HdlcRpc, PacketTooLong) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kMaxFrameSize = 100;

  std::array<std::byte, kMaxPacketSize + 1> packet{};
  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;

  EXPECT_EQ(encoder.Encode(
                packet, kMaxFrameSize, [](RpcFrame&) { return OkStatus(); }),
            Status::FailedPrecondition());
}

TEST(HdlcRpcFrame, MaxFrameSizeIsZero) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kMaxFrameSize = 0;

  std::array<std::byte, kMaxPacketSize> packet{};
  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;

  EXPECT_EQ(encoder.Encode(
                packet, kMaxFrameSize, [](RpcFrame&) { return OkStatus(); }),
            Status::FailedPrecondition());
}

TEST(HdlcRpcFrame, MaxSizeHdlcPayload) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kPacketSize = 256;
  constexpr size_t kMaxFrameSize = 20;
  constexpr auto kHdlcEscapeByte = std::byte{0x7e};

  std::array<std::byte, kPacketSize> packet{};
  std::fill(packet.begin(), packet.end(), kHdlcEscapeByte);

  struct EncodeState {
    size_t offset = 0;
    std::array<std::byte, 2 * kMaxPacketSize + kHdlcProtocolOverheadBytes>
        encoded{};
  } state;

  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;
  ASSERT_EQ(encoder.Encode(packet,
                           kMaxFrameSize,
                           [&state](RpcFrame& frame) {
                             EXPECT_TRUE(frame.header.empty());
                             std::copy(frame.payload.begin(),
                                       frame.payload.end(),
                                       state.encoded.begin() + state.offset);
                             state.offset += frame.payload.size();
                             return OkStatus();
                           }),
            OkStatus());

  std::array<std::byte, kMaxPacketSize> decoded{};
  HdlcRpcPacketDecoder<kMaxPacketSize> decoder;

  ASSERT_EQ(decoder.Decode(state.encoded,
                           [&decoded](ConstByteSpan packet_to_decode) {
                             std::copy(packet_to_decode.begin(),
                                       packet_to_decode.end(),
                                       decoded.begin());
                           }),
            OkStatus());

  EXPECT_TRUE(std::equal(packet.begin(), packet.end(), decoded.begin()));
}

TEST(HdlcRpc, CallbackErrorPropagation) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kPacketSize = 256;
  constexpr size_t kMaxFrameSize = 20;

  std::array<std::byte, kPacketSize> packet{};
  std::fill(packet.begin(), packet.end(), std::byte{0x42});

  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;
  EXPECT_EQ(
      encoder.Encode(packet,
                     kMaxFrameSize,
                     [](RpcFrame&) { return Status::PermissionDenied(); }),
      Status::PermissionDenied());
}

TEST(HdlcRpcFrame, OneByteAtTimeDecoding) {
  constexpr size_t kMaxPacketSize = 256;
  constexpr size_t kPacketSize = 100;
  constexpr size_t kMaxFrameSize = 8;

  HdlcRpcPacketEncoder<kMaxPacketSize> encoder;
  std::array<std::byte, kPacketSize> packet{};
  std::vector<std::byte> encoded;

  std::fill(packet.begin(), packet.end(), std::byte{0x42});
  ASSERT_EQ(encoder.Encode(packet,
                           kMaxFrameSize,
                           [&encoded](RpcFrame& frame) {
                             std::copy(frame.payload.begin(),
                                       frame.payload.end(),
                                       std::back_inserter(encoded));
                             return OkStatus();
                           }),
            OkStatus());

  std::array<std::byte, kMaxPacketSize> decoded{};
  HdlcRpcPacketDecoder<kMaxPacketSize> decoder;

  for (std::byte b : encoded) {
    auto buffer_span = span(&b, 1);
    ASSERT_EQ(decoder.Decode(buffer_span,
                             [&decoded](ConstByteSpan packet_to_decode) {
                               std::copy(packet_to_decode.begin(),
                                         packet_to_decode.end(),
                                         decoded.begin());
                             }),
              OkStatus());
  }

  EXPECT_TRUE(std::equal(packet.begin(), packet.end(), decoded.begin()));
}

}  // namespace
}  // namespace pw::rpc
