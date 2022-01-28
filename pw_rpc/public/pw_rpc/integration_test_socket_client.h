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
        channel_(Channel::Create<kChannelId>(&channel_output_)),
        client_(std::span(&channel_, 1)) {}

  Client& client() { return client_; }

  // Connects to the specified host:port and starts a background thread to read
  // packets from the socket.
  Status Start(const char* host, uint16_t port) {
    PW_TRY(stream_.Connect(host, port));
    std::thread{&SocketClientContext::ProcessPackets, this}.detach();
    return OkStatus();
  }

  // Calls Start for localhost.
  Status Start(uint16_t port) { return Start("localhost", port); }

 private:
  void ProcessPackets();

  stream::SocketStream stream_;
  hdlc::RpcChannelOutput channel_output_;
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
        PW_ASSERT(client_.ProcessPacket(frame.data()).ok());
      }
    }
  }
}

}  // namespace pw::rpc::integration_test
