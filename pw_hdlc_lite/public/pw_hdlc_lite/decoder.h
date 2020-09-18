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
#include <functional>  // std::invoke

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::hdlc_lite {

// Represents the contents of an HDLC frame -- the unescaped data between two
// flag bytes. Instances of Frame are only created when a full, valid frame has
// been read.
//
// For now, the Frame class assumes single-byte address and control fields and a
// 32-bit frame check sequence (FCS).
class Frame {
 private:
  static constexpr size_t kAddressSize = 1;
  static constexpr size_t kControlSize = 1;
  static constexpr size_t kFcsSize = sizeof(uint32_t);

 public:
  // The minimum size of a frame, excluding control bytes (flag or escape).
  static constexpr size_t kMinSizeBytes =
      kAddressSize + kControlSize + kFcsSize;

  // Creates a Frame with the specified data. The data MUST be valid frame data
  // with a verified frame check sequence.
  explicit constexpr Frame(ConstByteSpan data) : frame_(data) {
    // TODO(pwbug/246): Use PW_DASSERT when available.
    // PW_DASSERT(data.size() >= kMinSizeBytes);
  }

  constexpr unsigned address() const {
    return std::to_integer<unsigned>(frame_[0]);
  }

  constexpr std::byte control() const { return frame_[kAddressSize]; }

  constexpr ConstByteSpan data() const {
    return frame_.subspan(kAddressSize + kControlSize,
                          frame_.size() - kMinSizeBytes);
  }

 private:
  ConstByteSpan frame_;
};

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
      : buffer_(buffer), current_frame_size_(0), state_(State::kInterFrame) {}

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

  // Parses a single byte of an HDLC stream. Returns a Result with the complete
  // frame if the byte completes a frame. The status is the following:
  //
  //     OK - A frame was successfully decoded. The Result contains the Frame,
  //         which is invalidated by the next Process call.
  //     UNAVAILABLE - No frame is available.
  //     RESOURCE_EXHAUSTED - A frame completed, but it was too large to fit in
  //         the decoder's buffer.
  //     DATA_LOSS - A frame completed, but it was invalid. The frame was
  //         incomplete or the frame check sequence verification failed.
  //
  Result<Frame> Process(std::byte b);

  // Processes a span of data and calls the provided callback with each frame or
  // error.
  template <typename F, typename... Args>
  void Process(ConstByteSpan data, F&& callback, Args&&... args) {
    for (std::byte b : data) {
      auto result = Process(b);
      if (result.status() != Status::Unavailable()) {
        std::invoke(
            std::forward<F>(callback), std::forward<Args>(args)..., result);
      }
    }
  }

  // Returns the maximum size of the Decoder's frame buffer.
  size_t max_size() const { return buffer_.size(); }

  // Clears and resets the decoder.
  void clear() {
    current_frame_size_ = 0;
    state_ = State::kInterFrame;
  };

 private:
  // State enum class is used to make the Decoder a finite state machine.
  enum class State {
    kInterFrame,
    kFrame,
    kFrameEscape,
  };

  void AppendByte(std::byte new_byte);

  Status CheckFrame() const;

  bool VerifyFrameCheckSequence() const;

  const ByteSpan buffer_;

  size_t current_frame_size_;

  State state_;
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
  static_assert(size_bytes >= Frame::kMinSizeBytes);

  std::array<std::byte, size_bytes> frame_buffer_;
};

}  // namespace pw::hdlc_lite
