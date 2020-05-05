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

#include <cstdint>

#include "pw_span/span.h"

namespace pw::rpc {

class ChannelOutput {
 public:
  virtual ~ChannelOutput() = default;

  // Acquire a buffer into which to write an outgoing RPC packet.
  virtual span<std::byte> AcquireBuffer() = 0;

  // Sends the contents of the buffer from AcquireBuffer().
  virtual void SendAndReleaseBuffer(size_t size) = 0;
};

class Channel {
 public:
  // Creates a dynamically assignable channel without a set ID or output.
  constexpr Channel() : id_(kUnassignedChannelId), output_(nullptr) {}

  // Creates a channel with a static ID. The channel's output can also be
  // static, or it can set to null to allow dynamically opening connections
  // through the channel.
  constexpr Channel(uint32_t id, ChannelOutput* output)
      : id_(id), output_(output) {}

  constexpr uint32_t id() const { return id_; }

  void Write(span<std::byte> packet);

 private:
  static constexpr uint32_t kUnassignedChannelId = 0;
  uint32_t id_;
  ChannelOutput* output_;
};

}  // namespace pw::rpc
