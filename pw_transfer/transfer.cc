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
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk.h"

namespace pw::transfer {

void TransferService::Read(ServerContext&,
                           RawServerReaderWriter& reader_writer) {
  read_stream_ = std::move(reader_writer);

  read_stream_.set_on_next([this](ConstByteSpan message) {
    HandleChunk(message, internal::ServerContext::kRead);
  });
}

void TransferService::Write(ServerContext&,
                            RawServerReaderWriter& reader_writer) {
  write_stream_ = std::move(reader_writer);

  write_stream_.set_on_next([this](ConstByteSpan message) {
    HandleChunk(message, internal::ServerContext::kWrite);
  });
}

void TransferService::SendStatusChunk(RawServerReaderWriter& stream,
                                      uint32_t transfer_id,
                                      Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id;
  chunk.status = status.code();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, stream.PayloadBuffer());

  if (!result.ok()) {
    PW_LOG_ERROR("Failed to encode final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id));
    return;
  }

  if (!stream.Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id));
    return;
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
  encoder.WriteTransferId(context.transfer_id()).IgnoreError();
  encoder.WriteOffset(context.offset()).IgnoreError();

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
    encoder.WriteRemainingBytes(0).IgnoreError();
    context.set_pending_bytes(0);
  } else if (data.ok()) {
    encoder.WriteData(data.value()).IgnoreError();
    context.set_offset(context.offset() + data.value().size());
    context.set_pending_bytes(context.pending_bytes() - data.value().size());
  } else {
    read_stream_.ReleaseBuffer();
    return false;
  }

  return read_stream_.Write(encoder).ok();
}

void TransferService::FinishTransfer(internal::ServerContext& transfer,
                                     Status status) {
  const uint32_t id = transfer.transfer_id();
  PW_LOG_INFO("Transfer %u completed with status %u; sending final chunk",
              static_cast<unsigned>(id),
              status.code());
  status.Update(transfer.Finish(status));

  RawServerReaderWriter& stream =
      transfer.type() == internal::ServerContext::kRead ? read_stream_
                                                        : write_stream_;
  SendStatusChunk(stream, id, status);
}

void TransferService::HandleChunk(ConstByteSpan message,
                                  internal::ServerContext::Type type) {
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

  internal::ServerContextPool& pool = type == internal::ServerContext::kRead
                                          ? read_transfers_
                                          : write_transfers_;

  Result<internal::ServerContext*> result =
      pool.GetOrStartTransfer(chunk.transfer_id);
  if (!result.ok()) {
    SendStatusChunk(
        type == internal::ServerContext::kRead ? read_stream_ : write_stream_,
        chunk.transfer_id,
        result.status());
    PW_LOG_ERROR("Error handling chunk for transfer %u: %d",
                 static_cast<unsigned>(chunk.transfer_id),
                 result.status().code());
    return;
  }

  internal::ServerContext& transfer = *result.value();

  if (chunk.status.has_value()) {
    // Transfer has been terminated (successfully or not).
    Status status = chunk.status.value();
    if (!status.ok()) {
      PW_LOG_ERROR("Receiver terminated transfer %u with status %d",
                   static_cast<unsigned>(chunk.transfer_id),
                   static_cast<int>(status.code()));
    }
    transfer.Finish(status).IgnoreError();
    return;
  }

  if (type == internal::ServerContext::kRead) {
    HandleReadChunk(transfer, chunk);
  } else {
    HandleWriteChunk(transfer, chunk);
  }
}

void TransferService::HandleReadChunk(internal::ServerContext& transfer,
                                      const internal::Chunk& parameters) {
  if (!parameters.pending_bytes.has_value()) {
    // Malformed chunk.
    FinishTransfer(transfer, Status::InvalidArgument());
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
    FinishTransfer(transfer, Status::Unimplemented());
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

void TransferService::HandleWriteChunk(internal::ServerContext& transfer,
                                       const internal::Chunk& chunk) {
  if (chunk.offset != transfer.offset()) {
    // Bad offset; reset pending_bytes to send another parameters chunk.
    PW_LOG_DEBUG("Transfer %u expected offset %u, received %u",
                 static_cast<unsigned>(transfer.transfer_id()),
                 static_cast<unsigned>(transfer.offset()),
                 static_cast<unsigned>(chunk.offset));
    SendWriteTransferParameters(transfer);
    return;
  }

  if (chunk.data.size() > transfer.pending_bytes()) {
    // End the transfer, as this indcates a bug with the client implementation
    // where it doesn't respect pending_bytes. Trying to recover from here
    // could potentially result in an infinite transfer loop.
    PW_LOG_ERROR(
        "Received more data than what was requested; terminating transfer.");
    FinishTransfer(transfer, Status::Internal());
    return;
  }

  // Write the received data to the writer.
  if (!chunk.data.empty()) {
    if (Status status = transfer.writer().Write(chunk.data); !status.ok()) {
      PW_LOG_ERROR(
          "Transfer %u write of %u B chunk failed with status %u; aborting "
          "with DATA_LOSS",
          static_cast<unsigned>(chunk.transfer_id),
          static_cast<unsigned>(chunk.data.size()),
          status.code());
      FinishTransfer(transfer, Status::DataLoss());
      return;
    }
  }

  // When the client sets remaining_bytes to 0, it indicates completion of the
  // transfer. Acknowledge the completion through a status chunk and clean up.
  if (chunk.remaining_bytes == 0) {
    FinishTransfer(transfer, OkStatus());
    return;
  }

  // Update the transfer state.
  transfer.set_offset(transfer.offset() + chunk.data.size());
  transfer.set_pending_bytes(transfer.pending_bytes() - chunk.data.size());

  if (transfer.pending_bytes() > 0) {
    // Expecting more data to be sent by the client. Wait for the next chunk.
    return;
  }

  // All pending data has been received. Send a new parameters chunk to start
  // the next batch.
  SendWriteTransferParameters(transfer);
}

void TransferService::SendWriteTransferParameters(
    internal::ServerContext& transfer) {
  transfer.set_pending_bytes(
      std::min(default_max_bytes_to_receive_,
               transfer.writer().ConservativeWriteLimit()));

  const size_t max_chunk_size_bytes = MaxWriteChunkSize(
      transfer, max_chunk_size_bytes_, write_stream_.channel_id());
  const internal::Chunk parameters = {
      .transfer_id = transfer.transfer_id(),
      .pending_bytes = transfer.pending_bytes(),
      .max_chunk_size_bytes = max_chunk_size_bytes,
      .offset = transfer.offset(),
  };

  PW_LOG_DEBUG(
      "Transfer %u sending updated transfer parameters: "
      "offset=%u, pending_bytes=%u, chunk_size=%u",
      static_cast<unsigned>(transfer.transfer_id()),
      static_cast<unsigned>(transfer.offset()),
      static_cast<unsigned>(transfer.pending_bytes()),
      static_cast<unsigned>(max_chunk_size_bytes));

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
    FinishTransfer(transfer, Status::Internal());
  }
}

}  // namespace pw::transfer
