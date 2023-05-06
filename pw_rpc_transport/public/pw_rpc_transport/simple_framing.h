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

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "rpc_transport.h"

namespace pw::rpc {

// The following encoder and decoder implement a very simple RPC framing
// protocol where the first frame contains the total packet size in the header
// and up to max frame size bytes in the payload. The subsequent frames of the
// same packet have an empty header and the rest of the packet in their payload.
//
// First frame header also contains a special marker as an attempt to
// resynchronize the receiver if some frames were not sent (although we expect
// all transports using this framing type to be reliable, it's still possible
// that some random transport write timeout result in only the first few frames
// being sent and others dropped; in that case we attempt best effort recovery
// by effectively skipping the input until we see something that resembles a
// valid header).
//
// Both encoder and decoder are not thread-safe. The caller must ensure their
// correct use in a multi-threaded environment.

namespace internal {

void LogReceivedRpcPacketTooLarge(size_t packet_size, size_t max_packet_size);
void LogMalformedRpcFrameHeader();

}  // namespace internal

template <size_t kMaxPacketSize>
class SimpleRpcPacketEncoder
    : public RpcPacketEncoder<SimpleRpcPacketEncoder<kMaxPacketSize>> {
  static_assert(kMaxPacketSize <= 1 << 16);

 public:
  static constexpr size_t kHeaderSize = 4;
  static constexpr uint16_t kFrameMarker = 0x27f1;

  // Encodes `packet` with a simple framing protocol and split the resulting
  // frame into chunks of `RpcFrame`s where every `RpcFrame` is no longer than
  // `max_frame_size`. Calls `callback` for for each of the resulting
  // `RpcFrame`s.
  Status Encode(ConstByteSpan rpc_packet,
                size_t max_frame_size,
                OnRpcFrameEncodedCallback&& callback) {
    if (rpc_packet.size() > kMaxPacketSize) {
      return Status::FailedPrecondition();
    }
    if (max_frame_size <= kHeaderSize) {
      return Status::FailedPrecondition();
    }

    // First frame. This is the only frame that contains a header.
    const auto first_frame_size =
        std::min(max_frame_size - kHeaderSize, rpc_packet.size());

    std::array<std::byte, kHeaderSize> header{
        std::byte{kFrameMarker & 0xff},
        std::byte{(kFrameMarker >> 8) & 0xff},
        static_cast<std::byte>(rpc_packet.size() & 0xff),
        static_cast<std::byte>((rpc_packet.size() >> 8) & 0xff),
    };

    RpcFrame frame{.header = span(header),
                   .payload = rpc_packet.first(first_frame_size)};
    PW_TRY(callback(frame));
    auto remaining = rpc_packet.subspan(first_frame_size);

    // Second and subsequent frames (if any).
    while (!remaining.empty()) {
      auto fragment_size = std::min(max_frame_size, remaining.size());
      RpcFrame next_frame{.header = {},
                          .payload = remaining.first(fragment_size)};
      PW_TRY(callback(next_frame));
      remaining = remaining.subspan(fragment_size);
    }

    return OkStatus();
  }
};

template <size_t kMaxPacketSize>
class SimpleRpcPacketDecoder
    : public RpcPacketDecoder<SimpleRpcPacketDecoder<kMaxPacketSize>> {
  using Encoder = SimpleRpcPacketEncoder<kMaxPacketSize>;

 public:
  SimpleRpcPacketDecoder() { ExpectHeader(); }

  // Find and decodes `RpcFrame`s in `buffer`. `buffer` may contain zero or
  // more frames for zero or more packets. Calls `callback` for each
  // well-formed packet. Malformed packets are ignored and dropped.
  Status Decode(ConstByteSpan buffer, OnRpcPacketDecodedCallback&& callback) {
    while (!buffer.empty()) {
      switch (state_) {
        case State::kReadingHeader: {
          buffer = buffer.subspan(ReadHeader(buffer));
          break;
        }
        case State::kReadingPayload: {
          // Payload can only follow a valid header, reset the flag here so
          // that next invalid header logs again.
          already_logged_invalid_header_ = false;
          buffer = buffer.subspan(ReadPayload(buffer, callback));
          break;
        }
      }
    }
    return OkStatus();
  }

 private:
  enum class State {
    kReadingHeader,
    kReadingPayload,
  };

  size_t ReadHeader(ConstByteSpan buffer);

  size_t ReadPayload(ConstByteSpan buffer,
                     const OnRpcPacketDecodedCallback& callback);

  void ExpectHeader() {
    state_ = State::kReadingHeader;
    bytes_read_ = 0;
    bytes_remaining_ = Encoder::kHeaderSize;
  }

  void ExpectPayload(size_t size) {
    state_ = State::kReadingPayload;
    bytes_read_ = 0;
    bytes_remaining_ = size;
  }

  std::array<std::byte, kMaxPacketSize> packet_{};
  std::array<std::byte, Encoder::kHeaderSize> header_{};

  // Current decoder state.
  State state_;
  // How many bytes were read in the current state.
  size_t bytes_read_ = 0;
  // How many bytes remain to read in the current state.
  size_t bytes_remaining_ = 0;
  // When true, discard the received payload instead of buffering it (because
  // it's too big to buffer).
  bool discard_payload_ = false;
  // When true, skip logging on invalid header if we already logged. This is
  // to prevent logging on every payload byte of a malformed frame.
  bool already_logged_invalid_header_ = false;
};

template <size_t kMaxPacketSize>
size_t SimpleRpcPacketDecoder<kMaxPacketSize>::ReadHeader(
    ConstByteSpan buffer) {
  const auto read_size = std::min(buffer.size(), bytes_remaining_);
  bool header_available = false;
  PW_DASSERT(read_size <= Encoder::kHeaderSize);

  std::memcpy(header_.data() + bytes_read_, buffer.data(), read_size);
  bytes_read_ += read_size;
  bytes_remaining_ -= read_size;
  header_available = bytes_remaining_ == 0;

  if (header_available) {
    uint16_t marker = (static_cast<uint16_t>(header_[1]) << 8) |
                      static_cast<uint16_t>(header_[0]);
    uint16_t packet_size = (static_cast<uint16_t>(header_[3]) << 8) |
                           static_cast<uint16_t>(header_[2]);

    if (marker != Encoder::kFrameMarker) {
      // We expected a header but received some data that is definitely not
      // a header. Skip it and keep waiting for the next header. This could
      // also be a false positive, e.g. in the worst case we treat some
      // random data as a header: even then we should eventually be able to
      // stumble upon a real header and start processing packets again.
      ExpectHeader();
      // Consume only a single byte since we're looking for a header in a
      // broken stream and it could start at the next byte.
      if (!already_logged_invalid_header_) {
        internal::LogMalformedRpcFrameHeader();
        already_logged_invalid_header_ = true;
      }
      return 1;
    }
    if (packet_size > kMaxPacketSize) {
      // Consume both header and packet without saving it, as it's too big
      // for the buffer. This is likely due to max packet size mismatch
      // between the encoder and the decoder.
      internal::LogReceivedRpcPacketTooLarge(packet_size, kMaxPacketSize);
      discard_payload_ = true;
    }
    ExpectPayload(packet_size);
  }

  return read_size;
}

template <size_t kMaxPacketSize>
size_t SimpleRpcPacketDecoder<kMaxPacketSize>::ReadPayload(
    ConstByteSpan buffer, const OnRpcPacketDecodedCallback& callback) {
  if (buffer.size() >= bytes_remaining_ && bytes_read_ == 0) {
    const auto read_size = bytes_remaining_;
    // We have the whole packet available in the buffer, no need to copy
    // it into an internal buffer.
    callback(buffer.first(read_size));
    ExpectHeader();
    return read_size;
  }
  // Frame has been split between multiple inputs: assembling it in
  // an internal buffer.
  const auto read_size = std::min(buffer.size(), bytes_remaining_);

  // We could be discarding the payload if it was too big to fit into our
  // packet buffer.
  if (!discard_payload_) {
    PW_DASSERT(bytes_read_ + read_size <= packet_.size());
    std::memcpy(packet_.data() + bytes_read_, buffer.data(), read_size);
  }

  bytes_read_ += read_size;
  bytes_remaining_ -= read_size;
  if (bytes_remaining_ == 0) {
    if (discard_payload_) {
      discard_payload_ = false;
    } else {
      auto packet_span = span(packet_);
      callback(packet_span.first(bytes_read_));
    }
    ExpectHeader();
  }
  return read_size;
}

}  // namespace pw::rpc
