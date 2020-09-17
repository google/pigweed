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

#include <array>
#include <span>
#include <string_view>

#include "pw_hdlc_lite/hdlc_channel.h"
#include "pw_hdlc_lite/rpc_server_packets.h"
#include "pw_hdlc_lite/sys_io_stream.h"
#include "pw_log/log.h"
#include "pw_rpc/echo_service_nanopb.h"
#include "pw_rpc/server.h"

using std::byte;

constexpr size_t kMaxTransmissionUnit = 100;

void ConstructServerAndReadAndProcessData() {
  pw::stream::SerialWriter channel_output_serial;
  std::array<byte, kMaxTransmissionUnit> channel_output_buffer;
  pw::rpc::HdlcChannelOutput hdlc_channel_output(
      channel_output_serial, channel_output_buffer, "HdlcChannelOutput");

  pw::rpc::Channel kChannels[] = {
      pw::rpc::Channel::Create<1>(&hdlc_channel_output)};
  pw::rpc::Server server(kChannels);

  pw::rpc::EchoService echo_service;

  server.RegisterService(echo_service);

  pw::rpc::ReadAndProcessData<kMaxTransmissionUnit>(server);
}

int main() {
  ConstructServerAndReadAndProcessData();
  return 0;
}
