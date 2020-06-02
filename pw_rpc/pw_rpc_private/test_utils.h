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

#include <cstddef>
#include <cstdint>

#include "pw_rpc/channel.h"
#include "pw_rpc/internal/packet.h"
#include "pw_span/span.h"

namespace pw::rpc {

template <size_t buffer_size>
class TestOutput : public ChannelOutput {
 public:
  constexpr TestOutput(uint32_t id) : ChannelOutput(id), sent_packet_{} {}

  span<std::byte> AcquireBuffer() override { return buffer_; }

  void SendAndReleaseBuffer(size_t size) override {
    sent_packet_ = {buffer_, size};
  }

  const span<const std::byte>& sent_packet() const { return sent_packet_; }

 private:
  std::byte buffer_[buffer_size];
  span<const std::byte> sent_packet_;
};

namespace internal {

class Method;

}  // namespace internal

template <typename Service,
          size_t output_buffer_size = 128,
          uint32_t channel_id = 99,
          uint32_t service_id = 16>
class ServerContextForTest {
 public:
  static constexpr uint32_t kChannelId = channel_id;
  static constexpr uint32_t kServiceId = service_id;

  ServerContextForTest(const internal::Method& method)
      : output_(32),
        channel_(Channel::Create<kChannelId>(&output_)),
        service_(kServiceId),
        context_(channel_, service_, method) {}

  ServerContextForTest() : ServerContextForTest(service_.method) {}

  // Creates a packet for this context's channel, service, and method.
  internal::Packet packet(span<const std::byte> payload) const {
    return internal::Packet(internal::PacketType::RPC,
                            kChannelId,
                            kServiceId,
                            context_.method_->id(),
                            payload,
                            Status::OK);
  }

  ServerContext& get() { return context_; }
  const auto& output() const { return output_; }

 private:
  TestOutput<output_buffer_size> output_;
  Channel channel_;
  Service service_;

  ServerContext context_;
};

}  // namespace pw::rpc
