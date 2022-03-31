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

#include <cstdint>
#include <span>
#include <thread>

#include "pw_hdlc/rpc_channel.h"
#include "pw_hdlc/rpc_packets.h"
#include "pw_rpc/integration_testing.h"
#include "pw_status/try.h"
#include "pw_stream/socket_stream.h"

namespace pw::rpc::integration_test {

// Wraps an RPC client with a socket stream and a channel configured to use it.
// Useful for integration tests that run across a socket.
template <size_t kMaxTransmissionUnit>
class SocketClientContext {
 public:
  constexpr SocketClientContext()
      : channel_output_(stream_, hdlc::kDefaultRpcAddress, "socket"),
        channel_output_with_manipulator_(channel_output_),
        channel_(
            Channel::Create<kChannelId>(&channel_output_with_manipulator_)),
        client_(std::span(&channel_, 1)) {}

  Client& client() { return client_; }

  // Connects to the specified host:port and starts a background thread to read
  // packets from the socket.
  Status Start(const char* host, uint16_t port) {
    PW_TRY(stream_.Connect(host, port));
    std::thread{&SocketClientContext::ProcessPackets, this}.detach();
    return OkStatus();
  }

  void SetEgressChannelManipulator(
      ChannelManipulator* new_channel_manipulator) {
    channel_output_with_manipulator_.set_channel_manipulator(
        new_channel_manipulator);
  }

  void SetIngressChannelManipulator(
      ChannelManipulator* new_channel_manipulator) {
    if (new_channel_manipulator != nullptr) {
      new_channel_manipulator->set_send_packet([&](ConstByteSpan payload) {
        return client_.ProcessPacket(payload);
      });
    }
    ingress_channel_manipulator_ = new_channel_manipulator;
  }

  // Calls Start for localhost.
  Status Start(uint16_t port) { return Start("localhost", port); }

 private:
  void ProcessPackets();

  class ChannelOutputWithManipulator : public ChannelOutput {
   public:
    ChannelOutputWithManipulator(ChannelOutput& actual_output)
        : ChannelOutput(actual_output.name()),
          actual_output_(actual_output),
          channel_manipulator_(nullptr) {}

    void set_channel_manipulator(ChannelManipulator* new_channel_manipulator) {
      if (new_channel_manipulator != nullptr) {
        new_channel_manipulator->set_send_packet(
            ChannelManipulator::SendCallback([&](
                ConstByteSpan
                    payload) __attribute__((no_thread_safety_analysis)) {
              return actual_output_.Send(payload);
            }));
      }
      channel_manipulator_ = new_channel_manipulator;
    }

    size_t MaximumTransmissionUnit() override {
      return actual_output_.MaximumTransmissionUnit();
    }
    Status Send(std::span<const std::byte> buffer) override
        PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock()) {
      if (channel_manipulator_ != nullptr) {
        return channel_manipulator_->ProcessAndSend(buffer);
      }

      return actual_output_.Send(buffer);
    }

   private:
    ChannelOutput& actual_output_;
    ChannelManipulator* channel_manipulator_;
  };

  stream::SocketStream stream_;
  hdlc::RpcChannelOutput channel_output_;
  ChannelOutputWithManipulator channel_output_with_manipulator_;
  ChannelManipulator* ingress_channel_manipulator_;
  Channel channel_;
  Client client_;
};

template <size_t kMaxTransmissionUnit>
void SocketClientContext<kMaxTransmissionUnit>::ProcessPackets() {
  std::byte decode_buffer[kMaxTransmissionUnit];
  hdlc::Decoder decoder(decode_buffer);

  while (true) {
    std::byte byte[1];
    Result<ByteSpan> read = stream_.Read(byte);

    if (!read.ok() || read->size() == 0u) {
      continue;
    }

    if (auto result = decoder.Process(*byte); result.ok()) {
      hdlc::Frame& frame = result.value();
      if (frame.address() == hdlc::kDefaultRpcAddress) {
        if (ingress_channel_manipulator_ != nullptr) {
          PW_ASSERT(
              ingress_channel_manipulator_->ProcessAndSend(frame.data()).ok());
        } else {
          PW_ASSERT(client_.ProcessPacket(frame.data()).ok());
        }
      }
    }
  }
}

}  // namespace pw::rpc::integration_test
