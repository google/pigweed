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

#include <array>

#include "pw_hdlc_lite/decoder.h"
#include "pw_hdlc_lite/hdlc_channel.h"
#include "pw_hdlc_lite/sys_io_stream.h"
#include "pw_log/log.h"
#include "pw_rpc/server.h"
#include "pw_status/status.h"
#include "pw_sys_io/sys_io.h"

namespace pw::rpc {

// Utility function for reading the bytes through serial, decode the Bytes using
// the HDLC-Lite protocol and processing the decoded data.
template <size_t max_transmission_unit>
void ReadAndProcessData(Server& server) {
  hdlc_lite::DecoderBuffer<max_transmission_unit> decoder;

  pw::stream::SerialWriter channel_output_serial;
  std::array<std::byte, max_transmission_unit> channel_output_buffer;
  HdlcChannelOutput hdlc_channel_output(
      channel_output_serial, channel_output_buffer, "HdlcChannelOutput");

  std::byte data;

  while (true) {
    if (!pw::sys_io::ReadByte(&data).ok()) {
      return;
    }

    auto decoded_packet = decoder.AddByte(data);

    if (decoded_packet.ok()) {
      server.ProcessPacket(decoded_packet.value(), hdlc_channel_output);
    }
  }
}

}  // namespace pw::rpc
