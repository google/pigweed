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
#include <cstddef>
#include <limits>
#include <span>

#include "pw_hdlc_lite/rpc_packets.h"
#include "pw_hdlc_lite/sys_io_stream.h"
#include "pw_rpc/server.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_sys_io/sys_io.h"

namespace hdlc_example {

using RpcTaskCallback = void (*)(void*);

// A trivial implementation of a task loop that interleaves RPC message
// processing and the execution of a give callback. This class is useful for
// example code that attempts to run multiple services, where one or more
// services need to be flushed periodically to move forward.
class RpcTaskLoop final {
 public:
  // Runs the callback and processes RPC messages in a loop. This function will
  // not return and must be called last in example code.
  static void RunForever(pw::rpc::Server& server,
                         pw::rpc::ChannelOutput& output,
                         std::span<std::byte> decode_buffer,
                         RpcTaskCallback callback,
                         void* context = nullptr) {
    pw::hdlc_lite::Decoder decoder(decode_buffer);
    std::byte data;

    while (true) {
      pw::Status io_status = pw::sys_io::TryReadByte(&data);
      PW_DCHECK(io_status != pw::Status::Unimplemented());
      if (io_status.ok()) {
        if (auto result = decoder.Process(data); result.ok()) {
          pw::hdlc_lite::Frame& frame = result.value();
          if (frame.address() == pw::hdlc_lite::kDefaultRpcAddress) {
            server.ProcessPacket(frame.data(), output);
          }
        }
        // If data is in the pipe, prioritize reading and processing.
        continue;
      }
      // If no data is in the pipe, invoke the callback.
      callback(context);
    }
  }
};

}  // namespace hdlc_example
