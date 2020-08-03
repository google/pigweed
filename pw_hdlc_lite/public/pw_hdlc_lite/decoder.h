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
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::hdlc_lite {

// The Decoder class facilitates decoding of data frames using the HDLC-Lite
// protocol, by returning packets as they are decoded and storing incomplete
// data frames in a buffer.
//
// The Decoder class does not own the buffer it writes to. It can be used to
// write bytes to any buffer. The DecoderBuffer template class, defined below,
// allocates a buffer.
class Decoder {
 public:
  constexpr Decoder(ByteSpan buffer)
      : frame_buffer_(buffer), state_(DecoderState::kNoPacket), size_(0) {}

  // Parse a single byte of a HDLC stream. Returns a result object with the
  // complete packet if the latest byte finishes a frame, or a variety of
  // statuses in other cases, as follows:
  //
  //     OK - If the end of the data-frame was found and the packet was decoded
  //         successfully. The value of this Result<ConstByteSpan> object will
  //         be the payload.
  //     RESOURCE_EXHAUSTED - If the number of bytes added to the Decoder is
  //         greater than the buffer size. This function call will clear the
  //         decoder before returning.
  //     UNAVAILABLE - If the byte has been successfully escaped and added to
  //         the buffer, but we havent reached the end of the data-frame.
  //     DATA_LOSS - If the CRC verification process fails after seeing the
  //         ending Frame Delimiter byte (0x7E). Additionally happens when the
  //         FCS is not in the payload or when the data-frame does not start
  //         with the initial Frame Delimiter byte (0x7E).
  //
  Result<ConstByteSpan> AddByte(const std::byte b);

  // Returns the number of bytes of the active packet that are added to the
  // frame buffer.
  size_t size() const { return size_; }

  // Returns the maximum size of the Decoder's frame buffer.
  size_t max_size() const { return frame_buffer_.size(); }

  // Clears the frame buffer at the beginning of decoding the next packet.
  void clear() { size_ = 0; };

  // Indicates if the decoder is currently in the process of decoding a packet.
  bool IsPacketActive() { return state_ != DecoderState::kNoPacket; }

 private:
  // DecoderState enum class is used to make the Decoder a finite state machine.
  enum class DecoderState {
    kNoPacket,
    kPacketActive,
    kEscapeNextByte,
  };

  // Disallow Copy and Assign.
  Decoder(const Decoder&) = delete;

  Decoder& operator=(const Decoder&) = delete;

  // Will return true if the CRC is successfully verified.
  bool CheckCrc() const;

  // Attempts to write the escaped byte to the buffer and returns a Status
  // object accordingly:
  //
  //     RESOURCE_EXHAUSTED - If the buffer is out of space.
  //     UNAVAILABLE - If the byte has been successfully added to the buffer.
  //
  Status AddEscapedByte(std::byte new_byte);

  // Ensures the packet is correctly decoded and returns a status object
  // indicating if the packet can be returned. The three checks it does are:
  //     1. Checks if there are packet meets the minimum size of a HDLC-Lite
  //        packet.
  //     2. Checks that the frame buffer wasnt overflowed.
  //     3. Verifies if the CRC is correct
  // Will log errors accordingly.
  //
  Status PacketStatus() const;

  ByteSpan frame_buffer_;
  DecoderState state_;

  // The size_ variable represents the number of decoded bytes of the current
  // active packet.
  size_t size_;
};

// DecoderBuffers declare a buffer along with a Decoder.
template <size_t size_bytes>
class DecoderBuffer : public Decoder {
 public:
  DecoderBuffer() : Decoder(frame_buffer_) {}

  // Returns the maximum length of the bytes that can be inserted in the bytes
  // buffer.
  static constexpr size_t max_size() { return size_bytes; }

 private:
  std::array<std::byte, size_bytes> frame_buffer_;
};

}  // namespace pw::hdlc_lite
