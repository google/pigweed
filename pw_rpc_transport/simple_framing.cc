// Copyright 2023 The Pigweed Authors
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

#define PW_LOG_MODULE_NAME "PW_RPC"

#include "pw_rpc_transport/simple_framing.h"

#include <cinttypes>

#include "pw_log/log.h"

namespace pw::rpc::internal {

void LogReceivedRpcPacketTooLarge(size_t packet_size, size_t max_packet_size) {
  PW_LOG_WARN(
      "Received RPC packet (%d) bytes) is larger than max packet size (%d "
      "bytes)",
      static_cast<int>(packet_size),
      static_cast<int>(max_packet_size));
}

void LogMalformedRpcFrameHeader() {
  PW_LOG_WARN("Skipping malformed RPC frame header");
}

}  // namespace pw::rpc::internal
