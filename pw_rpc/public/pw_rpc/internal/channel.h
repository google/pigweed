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

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/lock.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {

class Packet;

class Channel : public rpc::Channel {
 public:
  Channel() = delete;

  constexpr Channel(uint32_t id, ChannelOutput* output)
      : rpc::Channel(id, output) {}

  // Represents a buffer acquired from a ChannelOutput.
  class OutputBuffer {
   public:
    constexpr OutputBuffer() = default;

    OutputBuffer(const OutputBuffer&) = delete;

    OutputBuffer(OutputBuffer&& other) { *this = std::move(other); }

    ~OutputBuffer() { PW_DASSERT(buffer_.empty()); }

    OutputBuffer& operator=(const OutputBuffer&) = delete;

    OutputBuffer& operator=(OutputBuffer&& other) {
      PW_DASSERT(buffer_.empty());
      buffer_ = other.buffer_;
      other.buffer_ = {};
      return *this;
    }

    // Returns a portion of this OutputBuffer to use as the packet payload.
    std::span<std::byte> payload(const Packet& packet) const;

    bool Contains(std::span<const std::byte> other) const {
      return !buffer_.empty() && other.data() >= buffer_.data() &&
             other.data() + other.size() <= buffer_.data() + buffer_.size();
    }

    bool empty() const { return buffer_.empty(); }

   private:
    friend class Channel;

    explicit constexpr OutputBuffer(std::span<std::byte> buffer)
        : buffer_(buffer) {}

    std::span<std::byte> buffer_;
  };

  // Acquires a buffer for the packet.
  OutputBuffer AcquireBuffer() const PW_LOCKS_EXCLUDED(rpc_lock()) {
    return OutputBuffer(output().AcquireBuffer());
  }

  // Sends an RPC packet. Acquires and uses a ChannelOutput buffer.
  Status Send(const internal::Packet& packet) PW_LOCKS_EXCLUDED(rpc_lock()) {
    return SendSpan(output().AcquireBuffer(), packet);
  }

  // Sends an RPC packet using the provided output buffer.
  Status SendBuffer(OutputBuffer& buffer, const internal::Packet& packet)
      PW_UNLOCK_FUNCTION(rpc_lock()) {
    // TODO(pwbug/597): It would be cleaner if the Call object released the
    //     OutputBuffer, unlocked the RPC mutex, and then passed a span to this
    //     function.
    const std::span released_buffer = buffer.buffer_;
    buffer.buffer_ = {};
    rpc_lock().unlock();
    return SendSpan(released_buffer, packet);
  }

  void Release(OutputBuffer& buffer) PW_UNLOCK_FUNCTION(rpc_lock()) {
    const ConstByteSpan released_buffer = buffer.buffer_;
    buffer.buffer_ = {};
    rpc_lock().unlock();
    output().DiscardBuffer(released_buffer);
  }

  // Allow setting the channel ID for tests.
  using rpc::Channel::set_channel_id;

 private:
  Status SendSpan(ByteSpan buffer, const internal::Packet& packet) const
      PW_LOCKS_EXCLUDED(rpc_lock());
};

}  // namespace pw::rpc::internal
