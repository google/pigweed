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

#include "pw_checksum/crc16_ccitt.h"
#include "pw_log/log.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

constexpr byte kHdlcFrameDelimiter = byte{0x7E};
constexpr byte kHdlcEscape = byte{0x7D};
constexpr byte kHdlcUnescapingConstant = byte{0x20};

}  // namespace

Result<ConstByteSpan> Decoder::AddByte(const byte new_byte) {
  switch (state_) {
    case DecoderState::kPacketActive: {
      if (new_byte != kHdlcFrameDelimiter) {
        // Packet active case
        if (new_byte != kHdlcEscape) {
          return Result<ConstByteSpan>(AddEscapedByte(new_byte));
        }
        state_ = DecoderState::kEscapeNextByte;
        return Result<ConstByteSpan>(Status::UNAVAILABLE);
      }
      // Packet complete case
      state_ = DecoderState::kNoPacket;
      Status status = PacketStatus();
      if (status.ok()) {
        // Common case: Happy Packet
        // Returning size_ - 2 is to trim off the 2-byte CRC value.
        return Result<ConstByteSpan>(frame_buffer_.first(size_ - 2));
      }
      if (status == Status::DATA_LOSS && size_ == 0) {
        // Uncommon case: Dropped an ending frame delimiter byte somewhere.
        // This happens if a delimiter byte was lost or corrupted which causes
        // the decoder to fall out of sync with the incoming flow of packets.
        // Recovery is done by switching to active mode assuming that the frame
        // delimiter "close" byte we just saw is actually a start delimiter.
        PW_LOG_ERROR(
            "Detected empty packet. Assuming out of sync; trying recovery");
        clear();
        state_ = DecoderState::kPacketActive;
        return Result<ConstByteSpan>(Status::UNAVAILABLE);
      }
      // Otherwise, forward the status from PacketStatus().
      return Result<ConstByteSpan>(status);
    }

    case DecoderState::kEscapeNextByte: {
      byte escaped_byte = new_byte ^ kHdlcUnescapingConstant;
      if (escaped_byte != kHdlcEscape && escaped_byte != kHdlcFrameDelimiter) {
        PW_LOG_WARN(
            "Suspicious escaped byte: 0x%02x; should only need to escape frame "
            "delimiter and escape byte",
            static_cast<int>(escaped_byte));
      }
      state_ = DecoderState::kPacketActive;
      return Result<ConstByteSpan>(AddEscapedByte(escaped_byte));
    }

    case DecoderState::kNoPacket: {
      if (new_byte != kHdlcFrameDelimiter) {
        PW_LOG_ERROR("Unexpected starting byte to the frame: 0x%02x",
                     static_cast<int>(new_byte));
        return Result<ConstByteSpan>(Status::UNAVAILABLE);
      }
      clear();
      state_ = DecoderState::kPacketActive;
      return Result<ConstByteSpan>(Status::UNAVAILABLE);
    }
  }
  return Result<ConstByteSpan>(Status::UNAVAILABLE);
}

bool Decoder::CheckCrc() const {
  uint16_t expected_crc =
      checksum::Crc16Ccitt::Calculate(frame_buffer_.first(size_ - 2), 0xFFFF);
  uint16_t actual_crc;
  std::memcpy(&actual_crc, (frame_buffer_.data() + size_ - 2), 2);
  return actual_crc == expected_crc;
}

Status Decoder::AddEscapedByte(const byte new_byte) {
  if (size_ >= max_size()) {
    // Increasing the size to flag the overflow case when the packet is complete
    size_++;
    return Status::RESOURCE_EXHAUSTED;
  }
  frame_buffer_[size_++] = new_byte;
  return Status::UNAVAILABLE;
}

Status Decoder::PacketStatus() const {
  if (size_ < 2) {
    PW_LOG_ERROR(
        "Received %d-byte packet; packets must at least have 2 CRC bytes",
        static_cast<int>(size_));
    return Status::DATA_LOSS;
  }

  if (size_ > max_size()) {
    PW_LOG_ERROR("Packet size [%zu] exceeds the maximum buffer size [%zu]",
                 size_,
                 max_size());
    return Status::RESOURCE_EXHAUSTED;
  }

  if (!CheckCrc()) {
    PW_LOG_ERROR("CRC verification failed for packet");
    return Status::DATA_LOSS;
  }

  return Status::OK;
}

}  // namespace pw::hdlc_lite
