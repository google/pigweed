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
#include "pw_rpc_system_server/rpc_server.h"

#include "pw_hdlc_lite/rpc_channel.h"
#include "pw_hdlc_lite/rpc_packets.h"
#include "pw_log/log.h"
#include "pw_stream/socket_stream.h"

namespace pw::rpc_system_server {
constexpr size_t kMaxTransmissionUnit = 256;
constexpr uint32_t kMaxHdlcFrameSize = 256;
constexpr uint32_t kSocketPort = 33000;
inline constexpr uint8_t kDefaultRpcAddress = 'R';

pw::stream::SocketStream socket_stream;
pw::hdlc_lite::RpcChannelOutputBuffer<kMaxTransmissionUnit> hdlc_channel_output(
    socket_stream, pw::hdlc_lite::kDefaultRpcAddress, "HDLC channel");
pw::rpc::Channel channels[] = {
    pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
pw::rpc::Server server(channels);

void Init() {
  pw::log_basic::SetOutput([](std::string_view log) {
    pw::hdlc_lite::WriteInformationFrame(
        1, std::as_bytes(std::span(log)), socket_stream);
  });

  socket_stream.Init(kSocketPort);
}

pw::rpc::Server& Server() { return server; }

Status Start() {
  // Declare a buffer for decoding incoming HDLC frames.
  std::array<std::byte, kMaxTransmissionUnit> input_buffer;
  hdlc_lite::Decoder decoder(input_buffer);

  while (true) {
    std::array<std::byte, kMaxHdlcFrameSize> data;
    auto ret_val = socket_stream.Read(data);
    if (ret_val.ok()) {
      for (auto byte : ret_val.value()) {
        if (auto result = decoder.Process(byte); result.ok()) {
          hdlc_lite::Frame& frame = result.value();
          if (frame.address() == kDefaultRpcAddress) {
            server.ProcessPacket(frame.data(), hdlc_channel_output);
          }
        }
      }
    }
  }
}
}  // namespace pw::rpc_system_server
