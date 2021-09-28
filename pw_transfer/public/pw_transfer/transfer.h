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

#include <limits>

#include "pw_bytes/span.h"
#include "pw_transfer/handler.h"
#include "pw_transfer/internal/server_context.h"
#include "pw_transfer/transfer.raw_rpc.pb.h"

namespace pw::transfer {

class TransferService : public generated::Transfer<TransferService> {
 public:
  // Initializes a TransferService that can be registered with an RPC server.
  //
  // max_chunk_size_bytes is the largest amount of data that can be sent within
  // a single transfer chunk (read or write), excluding any transport layer
  // overhead. Not all of this size is used to send data -- there is additional
  // overhead in the pw_rpc and pw_transfer protocols (typically ~22B/chunk).
  //
  // default_max_bytes_to_receive is the maximum amount of data to ask for at a
  // time during a write transfer, unless told a more restrictive amount by a
  // transfer handler. This size can span multiple chunks. A larger value
  // generally increases the efficiency of write transfers when sent over a
  // reliable transport. However, if the underlying transport is unreliable,
  // larger values could slow down a transfer in the event of repeated packet
  // loss.
  constexpr TransferService(size_t max_chunk_size_bytes,
                            size_t default_max_bytes_to_receive)
      : read_transfers_(internal::ServerContext::kRead, handlers_),
        write_transfers_(internal::ServerContext::kWrite, handlers_),
        max_chunk_size_bytes_(max_chunk_size_bytes),
        default_max_bytes_to_receive_(default_max_bytes_to_receive) {}

  TransferService(const TransferService&) = delete;
  TransferService(TransferService&&) = delete;

  TransferService& operator=(const TransferService&) = delete;
  TransferService& operator=(TransferService&&) = delete;

  void Read(ServerContext&, RawServerReaderWriter& reader_writer);

  void Write(ServerContext&, RawServerReaderWriter& reader_writer);

  void RegisterHandler(internal::Handler& handler) {
    handlers_.push_front(handler);
  }

  void UnregisterHandler(internal::Handler& handler) {
    handlers_.remove(handler);
  }

 private:
  void SendStatusChunk(RawServerReaderWriter& stream,
                       uint32_t transfer_id,
                       Status status);

  // Calls transfer.Finish() and sends the final status chunk.
  void FinishTransfer(internal::ServerContext& transfer, Status status);

  // Sends a out data chunk for a read transfer. Returns true if the data was
  // sent successfully.
  bool SendNextReadChunk(internal::ServerContext& context);

  void OnReadMessage(ConstByteSpan message);
  void OnWriteMessage(ConstByteSpan message);

  // All registered transfer handlers.
  IntrusiveList<internal::Handler> handlers_;

  // Persistent streams for read and write transfers. The server never closes
  // these streams -- they remain open until the client ends them.
  RawServerReaderWriter read_stream_;
  RawServerReaderWriter write_stream_;

  internal::ServerContextPool read_transfers_;
  internal::ServerContextPool write_transfers_;

  size_t max_chunk_size_bytes_;
  size_t default_max_bytes_to_receive_;
};

}  // namespace pw::transfer
