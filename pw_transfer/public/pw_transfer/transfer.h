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
#include "pw_transfer/internal/config.h"
#include "pw_transfer/internal/server_context.h"
#include "pw_transfer/transfer.raw_rpc.pb.h"
#include "pw_work_queue/work_queue.h"

namespace pw::transfer {
namespace internal {

struct Chunk;

}  // namespace internal

class TransferService : public pw_rpc::raw::Transfer::Service<TransferService> {
 public:
  // Initializes a TransferService that can be registered with an RPC server.
  //
  // The transfer service requires a work queue to perform deferred tasks, such
  // as handling transfer timeouts and retries. This work queue does not need to
  // be unique to the transfer service; it may be shared with other parts of the
  // system.
  //
  // The provided buffer is used to stage data from transfer chunks before it is
  // written out to the writer. The size of this buffer is the largest amount of
  // data that can be sent in a single transfer chunk, excluding any transport
  // layer overhead.
  //
  // max_pending_bytes is the maximum amount of data to ask for at a
  // time during a write transfer, unless told a more restrictive amount by a
  // transfer handler. This size can span multiple chunks. A larger value
  // generally increases the efficiency of write transfers when sent over a
  // reliable transport. However, if the underlying transport is unreliable,
  // larger values could slow down a transfer in the event of repeated packet
  // loss.
  TransferService(
      work_queue::WorkQueue& work_queue,
      ByteSpan transfer_data_buffer,
      uint32_t max_pending_bytes,
      chrono::SystemClock::duration chunk_timeout = cfg::kDefaultChunkTimeout,
      uint8_t max_retries = cfg::kDefaultMaxRetries)
      : read_transfers_(internal::kRead, handlers_),
        write_transfers_(internal::kWrite, handlers_),
        work_queue_(work_queue),
        client_(max_pending_bytes, transfer_data_buffer.size()),
        chunk_data_buffer_(transfer_data_buffer),
        chunk_timeout_(chunk_timeout),
        max_retries_(max_retries) {}

  TransferService(const TransferService&) = delete;
  TransferService(TransferService&&) = delete;

  TransferService& operator=(const TransferService&) = delete;
  TransferService& operator=(TransferService&&) = delete;

  void Read(RawServerReaderWriter& reader_writer) {
    client_.InitializeRead(reader_writer, [this](ConstByteSpan message) {
      HandleChunk(message, internal::kRead);
    });
  }

  void Write(RawServerReaderWriter& reader_writer) {
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

  work_queue::WorkQueue& work_queue_;

  // Stores the RPC streams and parameters for communicating with the client.
  internal::ClientConnection client_;

  internal::ChunkDataBuffer chunk_data_buffer_;

  chrono::SystemClock::duration chunk_timeout_;
  uint8_t max_retries_;
};

// A transfer service with its own buffer for transfer data.
template <size_t kSizeBytes>
class TransferServiceBuffer : public TransferService {
 public:
  constexpr TransferServiceBuffer(work_queue::WorkQueue& work_queue,
                                  uint32_t max_pending_bytes)
      : TransferService(work_queue, transfer_data_buffer_, max_pending_bytes) {}

 private:
  std::array<std::byte, kSizeBytes> transfer_data_buffer_;
};

}  // namespace pw::transfer
