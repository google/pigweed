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

#include <span>

#include "pw_rpc/channel.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {

class Packet;

class Channel : public rpc::Channel {
 public:
  Channel() = delete;

  constexpr Channel(uint32_t id, ChannelOutput* output)
      : rpc::Channel(id, output) {}

  class OutputBuffer {
   public:
    constexpr OutputBuffer() = default;

    OutputBuffer(const OutputBuffer&) = delete;

    OutputBuffer(OutputBuffer&& other) { *this = std::move(other); }

    ~OutputBuffer() { PW_DCHECK(buffer_.empty()); }

    OutputBuffer& operator=(const OutputBuffer&) = delete;

    OutputBuffer& operator=(OutputBuffer&& other) {
      PW_DCHECK(buffer_.empty());
      buffer_ = other.buffer_;
      other.buffer_ = {};
      return *this;
    }

    // Returns a portion of this OutputBuffer to use as the packet payload.
    std::span<std::byte> payload(const Packet& packet) const;

    bool Contains(std::span<const std::byte> buffer) const {
      return buffer.data() >= buffer_.data() &&
             buffer.data() + buffer.size() <= buffer_.data() + buffer_.size();
    }

    bool empty() const { return buffer_.empty(); }

   private:
    friend class Channel;

    explicit constexpr OutputBuffer(std::span<std::byte> buffer)
        : buffer_(buffer) {}

    std::span<std::byte> buffer_;
  };

  // Acquires a buffer for the packet.
  OutputBuffer AcquireBuffer() const {
    return OutputBuffer(output().AcquireBuffer());
  }

  Status Send(const internal::Packet& packet) {
    OutputBuffer buffer = AcquireBuffer();
    return Send(buffer, packet);
  }

  Status Send(OutputBuffer& output, const internal::Packet& packet);

  void Release(OutputBuffer& buffer) {
    output().DiscardBuffer(buffer.buffer_);
    buffer.buffer_ = {};
  }
};

}  // namespace pw::rpc::internal
