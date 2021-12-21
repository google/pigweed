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

#define PW_LOG_MODULE_NAME "TRN"

#include "pw_transfer/transfer.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/internal/chunk.h"

namespace pw::transfer {

void TransferService::HandleChunk(ConstByteSpan message,
                                  internal::TransferType type) {
  // All incoming chunks in a client read transfer are transfer parameter
  // updates, except for the final chunk, which is an acknowledgement of
  // completion.
  //
  // Transfer parameters may contain the following fields:
  //
  //   - transfer_id (required)
  //   - pending_bytes (required)
  //   - offset (required)
  //   - max_chunk_size_bytes
  //   - min_delay_microseconds (not yet supported)
  //
  internal::Chunk chunk;

  if (Status status = internal::DecodeChunk(message, chunk); !status.ok()) {
    // No special handling required here. The client will retransmit the chunk
    // when no response is received.
    PW_LOG_ERROR("Failed to decode incoming transfer chunk");
    return;
  }

  internal::ServerContextPool& pool =
      type == internal::kRead ? read_transfers_ : write_transfers_;
  rpc::RawServerReaderWriter& stream =
      type == internal::kRead ? client_.read_stream() : client_.write_stream();

  Result<internal::ServerContext*> result =
      chunk.IsInitialChunk() ? pool.StartTransfer(chunk.transfer_id,
                                                  work_queue_,
                                                  stream,
                                                  chunk_timeout_,
                                                  max_retries_)
                             : pool.GetPendingTransfer(chunk.transfer_id);
  if (!result.ok()) {
    client_.SendStatusChunk(type, chunk.transfer_id, result.status());
    PW_LOG_ERROR("Error handling chunk for transfer %u: %d",
                 static_cast<unsigned>(chunk.transfer_id),
                 result.status().code());
    return;
  }

  internal::ServerContext& transfer = *result.value();

  if (chunk.status.has_value()) {
    // Transfer has been terminated (successfully or not).
    const Status status = chunk.status.value();
    if (!status.ok()) {
      PW_LOG_ERROR("Receiver terminated transfer %u with status %d",
                   static_cast<unsigned>(chunk.transfer_id),
                   static_cast<int>(status.code()));
    }
    if (transfer.active()) {
      transfer.Finish(status).IgnoreError();
    }
    return;
  }

  if (transfer.ReadChunkData(
          chunk_data_buffer_, client_.max_parameters(), chunk)) {
    // Call this synchronously for now. Later, this will be deferred within a
    // work queue.
    transfer.ProcessChunk(chunk_data_buffer_, client_.max_parameters());
  }
}

}  // namespace pw::transfer
