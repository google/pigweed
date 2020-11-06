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

#include "pw_hdlc_lite/encoder.h"
#include "pw_hdlc_lite/rpc_channel.h"
#include "pw_hdlc_lite/rpc_packets.h"
#include "pw_hdlc_lite/sys_io_stream.h"
#include "pw_log/log.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/unit_test_service.h"

// TODO(frolv): This file is largely copied from the HDLC RPC example. It should
// be updated to use the system RPC server facade when that is ready.

namespace pw {
namespace {

constexpr size_t kMaxTransmissionUnit = 256;

// Used to write HDLC data to pw::sys_io.
stream::SysIoWriter writer;

// Set up the output channel for the pw_rpc server to use to use.
hdlc_lite::RpcChannelOutputBuffer<kMaxTransmissionUnit> hdlc_channel_output(
    writer, hdlc_lite::kDefaultRpcAddress, "HDLC channel");

rpc::Channel channels[] = {rpc::Channel::Create<1>(&hdlc_channel_output)};

// Declare the pw_rpc server with the HDLC channel.
rpc::Server server(channels);

unit_test::UnitTestService unit_test_service;

void RegisterServices() { server.RegisterService(unit_test_service); }

}  // namespace

extern "C" int main() {
  // Send log messages to HDLC address 1. This prevents logs from interfering
  // with pw_rpc communications.
  log_basic::SetOutput([](std::string_view log) {
    hdlc_lite::WriteInformationFrame(1, std::as_bytes(std::span(log)), writer);
  });

  RegisterServices();

  // Declare a buffer for decoding incoming HDLC frames.
  std::array<std::byte, kMaxTransmissionUnit> input_buffer;

  PW_LOG_INFO("Starting pw_rpc server");
  hdlc_lite::ReadAndProcessPackets(server, hdlc_channel_output, input_buffer);

  return 0;
}

}  // namespace pw
