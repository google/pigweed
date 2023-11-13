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

#include "pw_system/transfer_service.h"

#include "pw_system/file_manager.h"

namespace pw::system {
namespace {
// The maximum number of concurrent transfers the thread should support as
// either a client or a server. These can be set to 0 (if only using one or
// the other).
constexpr size_t kMaxConcurrentClientTransfers = 5;
constexpr size_t kMaxConcurrentServerTransfers = 3;

// The maximum payload size that can be transmitted by the system's
// transport stack. This would typically be defined within some transport
// header.
constexpr size_t kMaxTransmissionUnit = 512;

// The maximum amount of data that should be sent within a single transfer
// packet. By necessity, this should be less than the max transmission unit.
//
// pw_transfer requires some additional per-packet overhead, so the actual
// amount of data it sends may be lower than this.
constexpr size_t kMaxTransferChunkSizeBytes = 480;

// In a write transfer, the maximum number of bytes to receive at one time
// (potentially across multiple chunks), unless specified otherwise by the
// transfer handler's stream::Writer.
constexpr size_t kDefaultMaxBytesToReceive = 1024;

// Buffers for storing and encoding chunks (see documentation above).
std::array<std::byte, kMaxTransferChunkSizeBytes> chunk_buffer;
std::array<std::byte, kMaxTransmissionUnit> encode_buffer;

transfer::Thread<kMaxConcurrentClientTransfers, kMaxConcurrentServerTransfers>
    transfer_thread(chunk_buffer, encode_buffer);

transfer::TransferService transfer_service(transfer_thread,
                                           kDefaultMaxBytesToReceive);
}  // namespace

void RegisterTransferService(rpc::Server& rpc_server) {
  rpc_server.RegisterService(transfer_service);
}

void InitTransferService() {
  // the handlers need to be registered after the transfer thread has started
  for (auto handler : GetFileManager().GetTransferHandlers()) {
    transfer_service.RegisterHandler(*handler);
  }
}

transfer::TransferThread& GetTransferThread() { return transfer_thread; }

}  // namespace pw::system
