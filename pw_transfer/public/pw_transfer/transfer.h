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

#include <cstddef>
#include <cstdint>
#include <limits>

#include "pw_bytes/span.h"
#include "pw_transfer/handler.h"
#include "pw_transfer/internal/client_connection.h"
#include "pw_transfer/internal/server_context.h"
#include "pw_transfer/transfer.raw_rpc.pb.h"

namespace pw::transfer {
namespace internal {

struct Chunk;

}  // namespace internal

class TransferService : public generated::Transfer<TransferService> {
 public:
  // Initializes a TransferService that can be registered with an RPC server.
  //
  // max_chunk_size_bytes is the largest amount of data that can be sent within
  // a single transfer chunk (read or write), excluding any transport layer
  // overhead. Not all of this size is used to send data -- there is additional
  // overhead in the pw_rpc and pw_transfer protocols (typically ~22B/chunk).
  //
  // max_pending_bytes is the maximum amount of data to ask for at a
  // time during a write transfer, unless told a more restrictive amount by a
  // transfer handler. This size can span multiple chunks. A larger value
  // generally increases the efficiency of write transfers when sent over a
  // reliable transport. However, if the underlying transport is unreliable,
  // larger values could slow down a transfer in the event of repeated packet
  // loss.
  constexpr TransferService(uint32_t max_chunk_size_bytes,
                            uint32_t max_pending_bytes)
      : read_transfers_(internal::kRead, handlers_),
        write_transfers_(internal::kWrite, handlers_),
        client_(max_pending_bytes, max_chunk_size_bytes) {}

  TransferService(const TransferService&) = delete;
  TransferService(TransferService&&) = delete;

  TransferService& operator=(const TransferService&) = delete;
  TransferService& operator=(TransferService&&) = delete;

  void Read(ServerContext&, RawServerReaderWriter& reader_writer) {
    client_.InitializeRead(reader_writer, [this](ConstByteSpan message) {
      HandleChunk(message, internal::kRead);
    });
  }

  void Write(ServerContext&, RawServerReaderWriter& reader_writer) {
    client_.InitializeWrite(reader_writer, [this](ConstByteSpan message) {
      HandleChunk(message, internal::kWrite);
    });
  }

  void RegisterHandler(internal::Handler& handler) {
    handlers_.push_front(handler);
  }

  void UnregisterHandler(internal::Handler& handler) {
    handlers_.remove(handler);
  }

 private:
  // Calls transfer.Finish() and sends the final status chunk.
  void FinishTransfer(internal::ServerContext& transfer, Status status);

  // Sends a out data chunk for a read transfer. Returns true if the data was
  // sent successfully.
  bool SendNextReadChunk(internal::ServerContext& context);

  void HandleChunk(ConstByteSpan message, internal::TransferType type);

  // All registered transfer handlers.
  IntrusiveList<internal::Handler> handlers_;

  internal::ServerContextPool read_transfers_;
  internal::ServerContextPool write_transfers_;

  // Stores the RPC streams and parameters for communicating with the client.
  internal::ClientConnection client_;
};

}  // namespace pw::transfer
