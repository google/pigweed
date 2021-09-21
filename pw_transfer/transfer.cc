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

void TransferService::Read(ServerContext&,
                           RawServerReaderWriter& reader_writer) {
  read_stream_ = std::move(reader_writer);

  read_stream_.set_on_next(
      [this](ConstByteSpan message) { OnReadMessage(message); });
}

void TransferService::Write(ServerContext&,
                            RawServerReaderWriter& reader_writer) {
  write_stream_ = std::move(reader_writer);

  write_stream_.set_on_next(
      [this](ConstByteSpan message) { OnWriteMessage(message); });
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
    stream.Write(result.value())
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  }
}

bool TransferService::SendNextReadChunk(internal::ServerContext& context) {
  if (context.pending_bytes() == 0) {
    return false;
  }

  ByteSpan buffer = read_stream_.PayloadBuffer();

  // Begin by doing a partial encode of all the metadata fields, leaving the
  // buffer with usable space for the chunk data at the end.
  Chunk::MemoryEncoder encoder(buffer);
  encoder.WriteTransferId(context.transfer_id())
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  encoder.WriteOffset(context.offset())
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

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
    encoder.WriteRemainingBytes(0)
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    context.set_pending_bytes(0);
  } else if (!data.ok()) {
    read_stream_.ReleaseBuffer();
    return false;
  } else {
    encoder.WriteData(data.value())
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    context.set_offset(context.offset() + data.value().size());
    context.set_pending_bytes(context.pending_bytes() - data.value().size());
  }

  return read_stream_.Write(encoder).ok();
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

  Result<internal::ServerContext*> result =
      read_transfers_.GetOrStartTransfer(parameters.transfer_id);
  if (!result.ok()) {
    PW_LOG_ERROR("Error handling read transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 static_cast<int>(result.status().code()));
    SendStatusChunk(read_stream_, parameters.transfer_id, result.status());
    return;
  }

  internal::ServerContext& transfer = *result.value();

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
    transfer.set_max_chunk_size_bytes(
        std::min(static_cast<size_t>(parameters.max_chunk_size_bytes.value()),
                 max_chunk_size_bytes_));
  }

  transfer.set_pending_bytes(parameters.pending_bytes.value());
  while (SendNextReadChunk(transfer)) {
    // Empty.
  }
}

void TransferService::OnWriteMessage(ConstByteSpan message) {
  // Process an incoming chunk during a client write transfer. The chunk may
  // either be the initial "start write" chunk (which only contains the transfer
  // ID), or a data chunk.
  internal::Chunk chunk;

  if (Status status = internal::DecodeChunk(message, chunk); !status.ok()) {
    PW_LOG_ERROR("Failed to decode incoming write transfer chunk");
    return;
  }

  // Try to find an active write transfer for the requested ID, or start a new
  // one if a writable TransferHandler is registered for it.
  Result<internal::ServerContext*> maybe_context =
      write_transfers_.GetOrStartTransfer(chunk.transfer_id);
  if (!maybe_context.ok()) {
    PW_LOG_ERROR("Error handling write transfer %u: %d",
                 static_cast<unsigned>(chunk.transfer_id),
                 static_cast<int>(maybe_context.status().code()));
    SendStatusChunk(write_stream_, chunk.transfer_id, maybe_context.status());
    return;
  }

  internal::ServerContext& transfer = *maybe_context.value();

  // Check for a client-side error terminating the transfer.
  if (chunk.status.has_value()) {
    transfer.Finish(chunk.status.value());
    return;
  }

  // Copy data from the chunk into the transfer handler's Writer, if it is at
  // the offset the transfer is currently expecting. Under some circumstances,
  // the chunk's data may be empty (e.g. a zero-length transfer). In that case,
  // handle the chunk as if the data exists.
  bool chunk_data_processed = false;

  if (chunk.offset == transfer.offset()) {
    if (chunk.data.empty()) {
      chunk_data_processed = true;
    } else if (chunk.data.size() <= transfer.pending_bytes()) {
      if (Status status = transfer.writer().Write(chunk.data); !status.ok()) {
        SendStatusChunk(write_stream_, chunk.transfer_id, status);
        transfer.Finish(status);
        return;
      }
      transfer.set_offset(transfer.offset() + chunk.data.size());
      transfer.set_pending_bytes(transfer.pending_bytes() - chunk.data.size());
      chunk_data_processed = true;
    } else {
      // End the transfer, as this indcates a bug with the client implementation
      // where it doesn't respect pending_bytes. Trying to recover from here
      // could potentially result in an infinite transfer loop.
      PW_LOG_ERROR(
          "Received more data than what was requested; terminating transfer.");
      SendStatusChunk(write_stream_, chunk.transfer_id, Status::Internal());
      transfer.Finish(Status::Internal());
      return;
    }
  } else {
    // Bad offset; reset pending_bytes to send another parameters chunk.
    transfer.set_pending_bytes(0);
  }

  // When the client sets remaining_bytes to 0, it indicates completion of the
  // transfer. Acknowledge the completion through a status chunk and clean up.
  if (chunk_data_processed && chunk.remaining_bytes == 0) {
    SendStatusChunk(write_stream_, chunk.transfer_id, OkStatus());
    transfer.Finish(OkStatus());
    return;
  }

  if (transfer.pending_bytes() > 0) {
    // Expecting more data to be sent by the client. Wait for the next chunk.
    return;
  }

  // All pending data has been received. Send a new parameters chunk to start
  // the next batch.
  transfer.set_pending_bytes(
      std::min(default_max_bytes_to_receive_,
               transfer.writer().ConservativeWriteLimit()));

  internal::Chunk parameters = {};
  parameters.transfer_id = transfer.transfer_id();
  parameters.offset = transfer.offset();
  parameters.pending_bytes = transfer.pending_bytes();
  parameters.max_chunk_size_bytes = MaxWriteChunkSize(
      transfer, max_chunk_size_bytes_, write_stream_.channel_id());

  // If the parameters can't be encoded or sent, it most likely indicates a
  // transport-layer issue, so there isn't much that can be done by the transfer
  // service. The client will time out and can try to restart the transfer.
  Result<ConstByteSpan> data =
      internal::EncodeChunk(parameters, write_stream_.PayloadBuffer());
  if (data.ok()) {
    if (Status status = write_stream_.Write(*data); !status.ok()) {
      PW_LOG_ERROR("Failed to write parameters for transfer %u: %d",
                   static_cast<unsigned>(parameters.transfer_id),
                   status.code());
      transfer.Finish(Status::Internal());
    }
  } else {
    PW_LOG_ERROR("Failed to encode parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 data.status().code());
    write_stream_.ReleaseBuffer();
    transfer.Finish(Status::Internal());
  }
}

}  // namespace pw::transfer
