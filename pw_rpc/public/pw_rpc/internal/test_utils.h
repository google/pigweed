// Copyright 2021 The Pigweed Authors
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

#include "pw_assert/assert.h"
#include "pw_rpc/client.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/server.h"

namespace pw::rpc::internal {

template <size_t kOutputBufferSize>
class TestOutput : public ChannelOutput {
 public:
  static constexpr size_t buffer_size() { return kOutputBufferSize; }

  constexpr TestOutput(const char* name = "TestOutput")
      : ChannelOutput(name), sent_data_ {}
  {}

  std::span<std::byte> AcquireBuffer() override { return buffer_; }

  Status SendAndReleaseBuffer(std::span<const std::byte> buffer) override {
    if (buffer.empty()) {
      return OkStatus();
    }

    PW_ASSERT(buffer.data() == buffer_.data());

    packet_count_ += 1;
    sent_data_ = buffer;
    Result<internal::Packet> result = internal::Packet::FromBuffer(sent_data_);
    EXPECT_EQ(OkStatus(), result.status());
    sent_packet_ = result.value_or(internal::Packet());
    return send_status_;
  }

  std::span<const std::byte> buffer() const { return buffer_; }

  size_t packet_count() const { return packet_count_; }

  void set_send_status(Status status) { send_status_ = status; }

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
  Status send_status_;
};

// Version of the Server with extra methods exposed for testing.
class TestServer : public Server {
 public:
  using Server::FindCall;
};

template <typename Service,
          size_t kOutputBufferSize = 128,
          uint32_t kChannelId = 99,
          uint32_t kServiceId = 16>
class ServerContextForTest {
 public:
  static constexpr uint32_t channel_id() { return kChannelId; }
  static constexpr uint32_t service_id() { return kServiceId; }

  ServerContextForTest(const internal::Method& method)
      : channel_(Channel::Create<kChannelId>(&output_)),
        server_(std::span(&channel_, 1)),
        service_(kServiceId),
        context_(static_cast<Server&>(server_),
                 static_cast<Channel&>(channel_),
                 service_,
                 method,
                 0) {
    server_.RegisterService(service_);
  }

  // Create packets for this context's channel, service, and method.
  internal::Packet request(std::span<const std::byte> payload) const {
    return internal::Packet(internal::PacketType::REQUEST,
                            kChannelId,
                            kServiceId,
                            context_.method().id(),
                            0,
                            payload);
  }

  internal::Packet response(Status status,
                            std::span<const std::byte> payload = {}) const {
    return internal::Packet(internal::PacketType::RESPONSE,
                            kChannelId,
                            kServiceId,
                            context_.method().id(),
                            0,
                            payload,
                            status);
  }

  internal::Packet server_stream(std::span<const std::byte> payload) const {
    return internal::Packet(internal::PacketType::SERVER_STREAM,
                            kChannelId,
                            kServiceId,
                            context_.method().id(),
                            0,
                            payload);
  }

  internal::Packet client_stream(std::span<const std::byte> payload) const {
    return internal::Packet(internal::PacketType::CLIENT_STREAM,
                            kChannelId,
                            kServiceId,
                            context_.method().id(),
                            0,
                            payload);
  }

  const internal::CallContext& get() { return context_; }
  auto& output() { return output_; }
  TestServer& server() { return static_cast<TestServer&>(server_); }
  Service& service() { return service_; }

 private:
  TestOutput<kOutputBufferSize> output_;
  rpc::Channel channel_;
  rpc::Server server_;
  Service service_;

  const internal::CallContext context_;
};

template <size_t kOutputBufferSize = 128,
          size_t input_buffer_size = 128,
          uint32_t kChannelId = 99,
          uint32_t kServiceId = 16,
          uint32_t kMethodId = 111>
class ClientContextForTest {
 public:
  static constexpr uint32_t channel_id() { return kChannelId; }
  static constexpr uint32_t service_id() { return kServiceId; }
  static constexpr uint32_t method_id() { return kMethodId; }

  ClientContextForTest()
      : channel_(Channel::Create<kChannelId>(&output_)),
        client_(std::span(&channel_, 1)) {}

  const auto& output() const { return output_; }
  Channel& channel() { return static_cast<Channel&>(channel_); }
  Client& client() { return client_; }

  // Sends a packet to be processed by the client. Returns the client's
  // ProcessPacket status.
  Status SendPacket(internal::PacketType type,
                    Status status = OkStatus(),
                    std::span<const std::byte> payload = {}) {
    uint32_t call_id =
        output_.packet_count() > 0 ? output_.sent_packet().call_id() : 0;

    internal::Packet packet(
        type, kChannelId, kServiceId, kMethodId, call_id, payload, status);
    std::byte buffer[input_buffer_size];
    Result result = packet.Encode(buffer);
    EXPECT_EQ(result.status(), OkStatus());
    return client_.ProcessPacket(result.value_or(ConstByteSpan()));
  }

  Status SendResponse(Status status, std::span<const std::byte> payload = {}) {
    return SendPacket(internal::PacketType::RESPONSE, status, payload);
  }

  Status SendServerStream(std::span<const std::byte> payload) {
    return SendPacket(internal::PacketType::SERVER_STREAM, OkStatus(), payload);
  }

 private:
  TestOutput<kOutputBufferSize> output_;
  rpc::Channel channel_;
  Client client_;
};

}  // namespace pw::rpc::internal
