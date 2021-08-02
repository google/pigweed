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

#include "pw_transfer/transfer.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk.h"

namespace pw::transfer {
namespace internal {

Status Context::Start(Type type, Handler& handler) {
  PW_DCHECK(!active());

  if (type == kRead) {
    PW_TRY(handler.PrepareRead());
  } else {
    PW_TRY(handler.PrepareWrite());
  }

  type_ = type;
  handler_ = &handler;
  offset_ = 0;
  return OkStatus();
}

void Context::Finish(Status status) {
  PW_DCHECK(active());

  if (type_ == kRead) {
    handler_->FinalizeRead(status);
  } else {
    handler_->FinalizeWrite(status);
  }

  handler_ = nullptr;
}

}  // namespace internal

void TransferService::Read(ServerContext&,
                           RawServerReaderWriter& reader_writer) {
  read_stream_ = std::move(reader_writer);

  read_stream_.set_on_next(
      [this](ConstByteSpan message) { OnReadMessage(message); });
}

void TransferService::Write(ServerContext&,
                            RawServerReaderWriter& reader_writer) {
  // TODO(frolv): Implement server-side write transfers.
  reader_writer.Finish(Status::Unimplemented());
}

void TransferService::SendStatusChunk(RawServerReaderWriter& stream,
                                      uint32_t transfer_id,
                                      Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id;
  chunk.status = status.code();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, stream.PayloadBuffer());
  if (result.ok()) {
    stream.Write(result.value());
  }
}

bool TransferService::SendNextReadChunk(internal::Context& context) {
  if (context.pending_bytes() == 0) {
    return false;
  }

  ByteSpan buffer = read_stream_.PayloadBuffer();

  // Begin by doing a partial encode of all the metadata fields, leaving the
  // buffer with usable space for the chunk data at the end.
  Chunk::MemoryEncoder encoder(buffer);
  encoder.WriteTransferId(context.transfer_id());
  encoder.WriteOffset(context.offset());

  // Reserve space for the data proto field overhead and use the remainder of
  // the buffer for the chunk data.
  size_t reserved_size = encoder.size() + 1 /* data key */ + 5 /* data size */;

  ByteSpan data_buffer = buffer.subspan(reserved_size);
  size_t max_bytes_to_send =
      std::min(context.pending_bytes(), context.max_chunk_size_bytes());

  if (max_bytes_to_send < data_buffer.size()) {
    data_buffer = data_buffer.first(max_bytes_to_send);
  }

  Result<ByteSpan> data = context.reader().Read(data_buffer);
  if (data.status().IsOutOfRange()) {
    // No more data to read.
    encoder.WriteRemainingBytes(0);
    context.set_pending_bytes(0);
  } else if (!data.ok()) {
    read_stream_.ReleaseBuffer();
    return false;
  } else {
    encoder.WriteData(data.value());
    context.set_offset(context.offset() + data.value().size());
    context.set_pending_bytes(context.pending_bytes() - data.value().size());
  }

  return read_stream_.Write(encoder).ok();
}

Result<internal::Context*> TransferService::GetOrStartReadTransfer(
    uint32_t id) {
  internal::Context* new_transfer = nullptr;

  // Check if the ID belongs to an active transfer. If not, pick an inactive
  // slot to start a new transfer.
  for (internal::Context& transfer : read_transfers_) {
    if (transfer.active()) {
      if (transfer.transfer_id() == id) {
        return &transfer;
      }
    } else {
      new_transfer = &transfer;
    }
  }

  if (!new_transfer) {
    return Status::ResourceExhausted();
  }

  // Try to start the new transfer by checking if a handler for it exists.
  auto handler = std::find_if(handlers_.begin(), handlers_.end(), [&](auto& h) {
    return h.id() == id;
  });

  if (handler == handlers_.end()) {
    return Status::NotFound();
  }

  PW_TRY(new_transfer->Start(internal::Context::kRead, *handler));
  return new_transfer;
}

void TransferService::OnReadMessage(ConstByteSpan message) {
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
  internal::Chunk parameters;

  if (Status status = internal::DecodeChunk(message, parameters);
      !status.ok()) {
    // No special handling required here. The client will retransmit the chunk
    // when no response is received.
    PW_LOG_ERROR("Failed to decode incoming read transfer chunk");
    return;
  }

  Result<internal::Context*> result =
      GetOrStartReadTransfer(parameters.transfer_id);
  if (!result.ok()) {
    PW_LOG_ERROR("Error handling read transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 static_cast<int>(result.status().code()));
    SendStatusChunk(read_stream_, parameters.transfer_id, result.status());
    return;
  }

  internal::Context& transfer = *result.value();

  if (parameters.status.has_value()) {
    // Transfer has been terminated (successfully or not).
    Status status = parameters.status.value();
    if (!status.ok()) {
      PW_LOG_ERROR("Transfer %u failed with status %d",
                   static_cast<unsigned>(parameters.transfer_id),
                   static_cast<int>(status.code()));
    }
    transfer.Finish(status);
    return;
  }

  if (!parameters.pending_bytes.has_value()) {
    // Malformed chunk.
    SendStatusChunk(
        read_stream_, parameters.transfer_id, Status::InvalidArgument());
    transfer.Finish(Status::InvalidArgument());
    return;
  }

  // Update local transfer fields based on the received chunk.
  if (transfer.offset() != parameters.offset) {
    // TODO(frolv): pw_stream does not yet support seeking, so this temporarily
    // cancels the transfer. Once seeking is added, this should be updated.
    //
    //   transfer.set_offset(parameters.offset.value());
    //   transfer.Seek(transfer.offset());
    //
    SendStatusChunk(
        read_stream_, parameters.transfer_id, Status::Unimplemented());
    transfer.Finish(Status::Unimplemented());
    return;
  }

  if (parameters.max_chunk_size_bytes.has_value()) {
    transfer.set_max_chunk_size_bytes(parameters.max_chunk_size_bytes.value());
  }

  transfer.set_pending_bytes(parameters.pending_bytes.value());
  while (SendNextReadChunk(transfer)) {
    // Empty.
  }
}

}  // namespace pw::transfer
