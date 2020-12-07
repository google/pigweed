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

#include <cstddef>
#include <cstdint>

#include "pw_hdlc_lite/rpc_channel.h"
#include "pw_hdlc_lite/rpc_packets.h"
#include "pw_log/log.h"
#include "pw_rpc_system_server/rpc_server.h"
#include "pw_stream/socket_stream.h"

namespace pw::rpc::system_server {
namespace {

constexpr size_t kMaxTransmissionUnit = 256;
constexpr uint16_t kSocketPort = 33000;

stream::SocketStream socket_stream;
hdlc_lite::RpcChannelOutputBuffer<kMaxTransmissionUnit> hdlc_channel_output(
    socket_stream, hdlc_lite::kDefaultRpcAddress, "HDLC channel");
Channel channels[] = {rpc::Channel::Create<1>(&hdlc_channel_output)};
rpc::Server server(channels);

}  // namespace

void Init() {
  log_basic::SetOutput([](std::string_view log) {
    hdlc_lite::WriteInformationFrame(
        1, std::as_bytes(std::span(log)), socket_stream);
  });

  socket_stream.Init(kSocketPort);
}

rpc::Server& Server() { return server; }

Status Start() {
  // Declare a buffer for decoding incoming HDLC frames.
  std::array<std::byte, kMaxTransmissionUnit> input_buffer;
  hdlc_lite::Decoder decoder(input_buffer);

  while (true) {
    std::array<std::byte, kMaxTransmissionUnit> data;
    auto ret_val = socket_stream.Read(data);
    if (ret_val.ok()) {
      for (std::byte byte : ret_val.value()) {
        if (auto result = decoder.Process(byte); result.ok()) {
          hdlc_lite::Frame& frame = result.value();
          if (frame.address() == hdlc_lite::kDefaultRpcAddress) {
            server.ProcessPacket(frame.data(), hdlc_channel_output);
          }
        }
      }
    }
  }
}

}  // namespace pw::rpc::system_server
