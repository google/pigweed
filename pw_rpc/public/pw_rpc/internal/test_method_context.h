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
#pragma once

#include <array>
#include <cstddef>

#include "pw_assert/assert.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/fake_channel_output.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc::internal::test {

// Collects everything needed to invoke a particular RPC.
template <typename Service, uint32_t kMethodId>
class InvocationContext {
 public:
  Service& service() { return service_; }

  // Sets the channel ID, which defaults to an arbitrary value.
  void set_channel_id(uint32_t id) { channel_ = Channel(id, &output_); }

  // The total number of responses sent, which may be larger than
  // responses.max_size().
  size_t total_responses() const { return output_.total_responses(); }

  // True if the RPC has completed.
  bool done() const { return output_.done(); }

  // The status of the stream. Only valid if done() is true.
  Status status() const {
    PW_ASSERT(done());
    return output_.last_status();
  }

  void SendClientError(Status error) {
    std::byte packet[kNoPayloadPacketSizeBytes];
    PW_ASSERT(server_
                  .ProcessPacket(Packet(PacketType::CLIENT_ERROR,
                                        channel_.id(),
                                        service_.id(),
                                        kMethodId,
                                        {},
                                        error)
                                     .Encode(packet)
                                     .value(),
                                 output_)
                  .ok());
  }

  void SendCancel() {
    std::byte packet[kNoPayloadPacketSizeBytes];
    PW_ASSERT(
        server_
            .ProcessPacket(
                Packet(
                    PacketType::CANCEL, channel_.id(), service_.id(), kMethodId)
                    .Encode(packet)
                    .value(),
                output_)
            .ok());
  }

 protected:
  template <typename... Args>
  InvocationContext(const Method& method,
                    FakeChannelOutput& output,
                    Args&&... service_args)
      : output_(output),
        channel_(Channel::Create<123>(&output_)),
        server_(std::span(&channel_, 1)),
        service_(std::forward<Args>(service_args)...),
        server_call_(static_cast<internal::Server&>(server_),
                     static_cast<internal::Channel&>(channel_),
                     service_,
                     method) {
    server_.RegisterService(service_);
  }

  template <size_t kMaxPayloadSize = 32>
  void SendClientStream(ConstByteSpan payload) {
    std::byte packet[kNoPayloadPacketSizeBytes + 3 + kMaxPayloadSize];
    PW_ASSERT(server_
                  .ProcessPacket(Packet(PacketType::CLIENT_STREAM,
                                        channel_.id(),
                                        service_.id(),
                                        kMethodId,
                                        payload)
                                     .Encode(packet)
                                     .value(),
                                 output_)
                  .ok());
  }

  void SendClientStreamEnd() {
    std::byte packet[kNoPayloadPacketSizeBytes];
    PW_ASSERT(server_
                  .ProcessPacket(Packet(PacketType::CLIENT_STREAM_END,
                                        channel_.id(),
                                        service_.id(),
                                        kMethodId)
                                     .Encode(packet)
                                     .value(),
                                 output_)
                  .ok());
  }

  // Invokes the RPC, optionally with a request argument.
  template <auto kMethod, typename T, typename... RequestArg>
  auto call(RequestArg&&... request) {
    static_assert(sizeof...(request) <= 1);
    output_.clear();
    T responder = GetResponder<T>();
    return CallMethodImplFunction<kMethod>(
        InvocationContext<Service, kMethodId>::server_call(),
        std::forward<RequestArg>(request)...,
        responder);
  }

  template <typename T>
  T GetResponder() {
    return T(InvocationContext<Service, kMethodId>::server_call());
  }

  internal::ServerCall& server_call() { return server_call_; }

 private:
  static constexpr size_t kNoPayloadPacketSizeBytes =
      2 /* type */ + 2 /* channel */ + 5 /* service */ + 5 /* method */ +
      2 /* status */;

  FakeChannelOutput& output_;
  rpc::Channel channel_;
  rpc::Server server_;
  Service service_;
  internal::ServerCall server_call_;
};

}  // namespace pw::rpc::internal::test
