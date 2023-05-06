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
#pragma once

#include <array>

#include "pw_bytes/span.h"
#include "pw_hdlc/decoder.h"
#include "pw_hdlc/encoder.h"
#include "pw_result/result.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_stream/memory_stream.h"
#include "rpc_transport.h"

namespace pw::rpc {

inline constexpr size_t kHdlcProtocolOverheadBytes = 14;

template <size_t kMaxPacketSize>
class HdlcRpcPacketEncoder
    : public RpcPacketEncoder<HdlcRpcPacketEncoder<kMaxPacketSize>> {
 public:
  // Encodes `packet` as HDLC UI frame and splits the resulting frame into
  // chunks of `RpcFrame`s where every `RpcFrame` is no longer than
  // `max_frame_size`. Calls `callback` for each of the resulting `RpcFrame`s.
  //
  // Returns:
  // * FAILED_PRECONDITION if `packet` is too long or `max_frame_size` is zero.
  // * The underlying HDLC encoding error if it fails to generate a UI frame.
  // * The underlying callback invocation error from the first failed callback.
  //
  Status Encode(ConstByteSpan packet,
                size_t max_frame_size,
                OnRpcFrameEncodedCallback&& callback) {
    if (packet.size() > kMaxPacketSize) {
      return Status::FailedPrecondition();
    }
    if (max_frame_size == 0) {
      return Status::FailedPrecondition();
    }
    stream::MemoryWriter writer(buffer_);
    // HDLC addresses are not directly used by the transport. Addressing is done
    // by the RPC routing layer using pw_rpc channel ids.
    PW_TRY(hdlc::WriteUIFrame(/*address=*/0, packet, writer));

    auto remaining = writer.WrittenData();
    while (!remaining.empty()) {
      auto next_fragment_size = std::min(max_frame_size, remaining.size());
      auto fragment = remaining.first(next_fragment_size);
      // No header needed for HDLC: frame payload is already HDLC-encoded and
      // includes frame delimiters.
      RpcFrame frame{.header = {}, .payload = fragment};
      PW_TRY(callback(frame));
      remaining = remaining.subspan(next_fragment_size);
    }

    return OkStatus();
  }

 private:
  // Buffer for HDLC-encoded data. Must be 2x of the max packet size to
  // accommodate HDLC escape bytes for the worst case where each payload byte
  // must be escaped, plus 14 bytes for the HDLC protocol overhead.
  static constexpr size_t kEncodeBufferSize =
      2 * kMaxPacketSize + kHdlcProtocolOverheadBytes;
  std::array<std::byte, kEncodeBufferSize> buffer_;
};

template <size_t kMaxPacketSize>
class HdlcRpcPacketDecoder
    : public RpcPacketDecoder<HdlcRpcPacketDecoder<kMaxPacketSize>> {
 public:
  HdlcRpcPacketDecoder() : decoder_(decode_buffer_) {}

  // Finds and decodes HDLC frames in `buffer` and calls `callback` for each
  // well-formed frame. Malformed frames are ignored and dropped quietly.
  Status Decode(ConstByteSpan buffer, OnRpcPacketDecodedCallback&& callback) {
    decoder_.Process(
        buffer,
        [callback = std::move(callback)](Result<hdlc::Frame> hdlc_frame) {
          if (hdlc_frame.ok()) {
            callback(hdlc_frame->data());
          }
        });
    return OkStatus();
  }

 private:
  // decode_buffer_ is used to store a decoded HDLC packet, including the
  // payload (of up to kMaxPacketSize), address (varint that is always 0 in our
  // case), control flag and checksum. The total size of the non-payload
  // components is kMinContentSizeBytes.
  std::array<std::byte, kMaxPacketSize + hdlc::Frame::kMinContentSizeBytes>
      decode_buffer_{};
  hdlc::Decoder decoder_;
};

}  // namespace pw::rpc
