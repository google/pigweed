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

// Internal-only testing utilities. public/pw_rpc/test_method_context.h provides
// improved public-facing utilities for testing RPC services.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc {

template <size_t output_buffer_size>
class TestOutput : public ChannelOutput {
 public:
  static constexpr size_t buffer_size() { return output_buffer_size; }

  constexpr TestOutput(const char* name = "TestOutput")
      : ChannelOutput(name), sent_data_{} {}

  std::span<std::byte> AcquireBuffer() override { return buffer_; }

  void SendAndReleaseBuffer(size_t size) override {
    if (size == 0u) {
      return;
    }

    packet_count_ += 1;
    sent_data_ = std::span(buffer_.data(), size);
    EXPECT_EQ(Status::OK,
              internal::Packet::FromBuffer(sent_data_, sent_packet_));
  }

  std::span<const std::byte> buffer() const { return buffer_; }

  size_t packet_count() const { return packet_count_; }

  const std::span<const std::byte>& sent_data() const { return sent_data_; }
  const internal::Packet& sent_packet() const {
    EXPECT_GT(packet_count_, 0u);
    return sent_packet_;
  }

 private:
  std::array<std::byte, buffer_size()> buffer_;
  std::span<const std::byte> sent_data_;
  internal::Packet sent_packet_;
  size_t packet_count_ = 0;
};

// Version of the internal::Server with extra methods exposed for testing.
class TestServer : public internal::Server {
 public:
  using internal::Server::writers;
};

template <typename Service,
          size_t output_buffer_size = 128,
          uint32_t channel_id = 99,
          uint32_t service_id = 16>
class ServerContextForTest {
 public:
  static constexpr uint32_t kChannelId = channel_id;
  static constexpr uint32_t kServiceId = service_id;

  ServerContextForTest(const internal::Method& method)
      : channel_(Channel::Create<kChannelId>(&output_)),
        server_(std::span(&channel_, 1)),
        service_(kServiceId),
        context_(static_cast<internal::Server&>(server_),
                 static_cast<internal::Channel&>(channel_),
                 service_,
                 method) {
    server_.RegisterService(service_);
  }

  ServerContextForTest() : ServerContextForTest(service_.method) {}

  // Creates a packet for this context's channel, service, and method.
  internal::Packet packet(std::span<const std::byte> payload) const {
    return internal::Packet(internal::PacketType::RPC,
                            kChannelId,
                            kServiceId,
                            context_.method().id(),
                            payload,
                            Status::OK);
  }

  internal::ServerCall& get() { return context_; }
  const auto& output() const { return output_; }
  TestServer& server() { return static_cast<TestServer&>(server_); }

 private:
  TestOutput<output_buffer_size> output_;
  Channel channel_;
  Server server_;
  Service service_;

  internal::ServerCall context_;
};

}  // namespace pw::rpc
