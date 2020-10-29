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
#include <span>
#include <type_traits>

#include "pw_assert/light.h"
#include "pw_status/status.h"

namespace pw::rpc {
namespace internal {

class BaseClientCall;

}  // namespace internal

class Client;

class ChannelOutput {
 public:
  // Creates a channel output with the provided name. The name is used for
  // logging only.
  constexpr ChannelOutput(const char* name) : name_(name) {}

  virtual ~ChannelOutput() = default;

  constexpr const char* name() const { return name_; }

  // Acquire a buffer into which to write an outgoing RPC packet. The
  // implementation is expected to handle synchronization if necessary.
  virtual std::span<std::byte> AcquireBuffer() = 0;

  // Sends the contents of a buffer previously obtained from AcquireBuffer().
  // This may be called with an empty span, in which case the buffer should be
  // released without sending any data.
  //
  // Returns OK if the operation succeeded, or an implementation-defined Status
  // value if there was an error. The implementation must NOT return
  // FAILED_PRECONDITION or INTERNAL, which are reserved by pw_rpc.
  virtual Status SendAndReleaseBuffer(std::span<const std::byte> buffer) = 0;

  void DiscardBuffer(std::span<const std::byte> buffer) {
    SendAndReleaseBuffer(buffer.first(0));
  }

 private:
  const char* name_;
};

class Channel {
 public:
  static constexpr uint32_t kUnassignedChannelId = 0;

  // Creates a dynamically assignable channel without a set ID or output.
  constexpr Channel()
      : id_(kUnassignedChannelId), output_(nullptr), client_(nullptr) {}

  // Creates a channel with a static ID. The channel's output can also be
  // static, or it can set to null to allow dynamically opening connections
  // through the channel.
  template <uint32_t kId>
  constexpr static Channel Create(ChannelOutput* output) {
    static_assert(kId != kUnassignedChannelId, "Channel ID cannot be 0");
    return Channel(kId, output);
  }

  // Creates a channel with a static ID from an enum value.
  template <auto kId,
            typename T = decltype(kId),
            typename = std::enable_if_t<std::is_enum_v<T>>,
            typename U = std::underlying_type_t<T>>
  constexpr static Channel Create(ChannelOutput* output) {
    constexpr U kIntId = static_cast<U>(kId);
    static_assert(kIntId >= 0, "Channel ID cannot be negative");
    static_assert(kIntId <= std::numeric_limits<uint32_t>::max(),
                  "Channel ID must fit in a uint32");
    return Create<static_cast<uint32_t>(kIntId)>(output);
  }

  constexpr uint32_t id() const { return id_; }
  constexpr bool assigned() const { return id_ != kUnassignedChannelId; }

 protected:
  constexpr Channel(uint32_t id, ChannelOutput* output)
      : id_(id), output_(output), client_(nullptr) {
    PW_ASSERT(id != kUnassignedChannelId);
  }

  ChannelOutput& output() const {
    PW_ASSERT(output_ != nullptr);
    return *output_;
  }

 private:
  friend class internal::BaseClientCall;
  friend class Client;

  constexpr Client* client() const { return client_; }
  constexpr void set_client(Client* client) { client_ = client; }

  uint32_t id_;
  ChannelOutput* output_;
  Client* client_;
};

}  // namespace pw::rpc
