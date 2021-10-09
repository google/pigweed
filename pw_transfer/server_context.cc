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

#include "pw_transfer/internal/server_context.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk.h"
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

Status ServerContext::Start(TransferType type, Handler& handler) {
  PW_DCHECK(!active());

  PW_LOG_INFO("Starting transfer %u", static_cast<unsigned>(handler.id()));

  if (const Status status = handler.Prepare(type); !status.ok()) {
    PW_LOG_WARN("Transfer %u prepare failed with status %u",
                static_cast<unsigned>(handler.id()),
                status.code());
    return status.IsPermissionDenied() ? status : Status::DataLoss();
  }

  type_ = type;
  state_ = kData;

  handler_ = &handler;
  set_transfer_id(handler.id());
  set_offset(0);
  set_pending_bytes(0);

  return OkStatus();
}

Status ServerContext::Finish(const Status status) {
  PW_DCHECK(active());

  Handler& handler = *handler_;
  state_ = kCompleted;
  status_ = status;

  if (type_ == kRead) {
    handler.FinalizeRead(status);
    return OkStatus();
  }

  if (Status finalized = handler.FinalizeWrite(status); !finalized.ok()) {
    PW_LOG_ERROR(
        "FinalizeWrite() for transfer %u failed with status %u; aborting with "
        "DATA_LOSS",
        static_cast<unsigned>(handler.id()),
        static_cast<int>(finalized.code()));
    return Status::DataLoss();
  }
  return OkStatus();
}

void ServerContext::HandleReadChunk(ClientConnection& client,
                                    const Chunk& parameters) {
  // Update local transfer fields based on the received chunk.
  if (!parameters.pending_bytes.has_value()) {
    // Malformed chunk.
    FinishAndSendStatus(client, Status::InvalidArgument());
    return;
  }

  const uint32_t pending_bytes = *parameters.pending_bytes;
  if (pending_bytes == 0u) {
    PW_LOG_ERROR("Transfer %d client requested 0 bytes (invalid); aborting",
                 static_cast<unsigned>(transfer_id()));
    FinishAndSendStatus(client, Status::Internal());
    return;
  }

  // TODO(hepler): Limit pending_bytes to max_parameters() when supported by the
  //     protocol. Clients will need to adjust their pending_bytes when set.
  // std::min(pending_bytes, client.max_parameters().pending_bytes())
  set_pending_bytes(pending_bytes);

  if (parameters.max_chunk_size_bytes.has_value()) {
    set_max_chunk_size_bytes(
        std::min(*parameters.max_chunk_size_bytes,
                 client.max_parameters().max_chunk_size_bytes()));
  }

  // If the offsets don't match, attempt to seek on the reader. Not all transfer
  // handlers support seeking; abort with UNIMPLEMENTED if this handler doesn't.
  if (offset() != parameters.offset) {
    set_offset(parameters.offset);

    if (Status seek_status = reader().Seek(offset()); !seek_status.ok()) {
      PW_LOG_WARN("Transfer %u seek to %u failed with status %u",
                  static_cast<unsigned>(transfer_id()),
                  static_cast<unsigned>(offset()),
                  seek_status.code());

      // Remap status codes to return one of the following:
      //
      //   INTERNAL: invalid seek, never should happen
      //   DATA_LOSS: the reader is in a bad state
      //   UNIMPLEMENTED: seeking is not supported
      //
      if (seek_status.IsOutOfRange()) {
        seek_status = Status::Internal();
      } else if (!seek_status.IsUnimplemented()) {
        seek_status = Status::DataLoss();
      }

      FinishAndSendStatus(client, seek_status);
      return;
    }
  }

  Status read_chunk_status;
  while ((read_chunk_status = SendNextReadChunk(client)).ok()) {
    // Continue until all requested bytes are sent.
  }

  if (!read_chunk_status.IsOutOfRange()) {
    FinishAndSendStatus(client, read_chunk_status);
  }
}

Status ServerContext::SendNextReadChunk(ClientConnection& client) {
  if (pending_bytes() == 0) {
    return Status::OutOfRange();
  }

  ByteSpan buffer = client.read_stream().PayloadBuffer();

  // Begin by doing a partial encode of all the metadata fields, leaving the
  // buffer with usable space for the chunk data at the end.
  transfer::Chunk::MemoryEncoder encoder{buffer};
  encoder.WriteTransferId(transfer_id()).IgnoreError();
  encoder.WriteOffset(offset()).IgnoreError();

  // Reserve space for the data proto field overhead and use the remainder of
  // the buffer for the chunk data.
  size_t reserved_size = encoder.size() + 1 /* data key */ + 5 /* data size */;

  ByteSpan data_buffer = buffer.subspan(reserved_size);
  size_t max_bytes_to_send = std::min(pending_bytes(), max_chunk_size_bytes());

  if (max_bytes_to_send < data_buffer.size()) {
    data_buffer = data_buffer.first(max_bytes_to_send);
  }

  Result<ByteSpan> data = reader().Read(data_buffer);
  if (data.status().IsOutOfRange()) {
    // No more data to read.
    encoder.WriteRemainingBytes(0).IgnoreError();
    set_pending_bytes(0);
  } else if (data.ok()) {
    encoder.WriteData(data.value()).IgnoreError();
    advance_offset(data.value().size());
    set_pending_bytes(pending_bytes() - data.value().size());
  } else {
    PW_LOG_ERROR("Transfer %u read failed with status %u",
                 static_cast<unsigned>(transfer_id()),
                 data.status().code());
    client.read_stream().ReleaseBuffer();
    return Status::DataLoss();
  }

  if (!encoder.status().ok()) {
    PW_LOG_ERROR("Transfer %u failed to encode read chunk",
                 static_cast<unsigned>(transfer_id()));
    client.read_stream().ReleaseBuffer();
    return Status::Internal();
  }

  if (const Status status = client.read_stream().Write(encoder); !status.ok()) {
    PW_LOG_ERROR("Transfer %u failed to send chunk, status %u",
                 static_cast<unsigned>(transfer_id()),
                 status.code());
    return Status::DataLoss();
  }

  return data.status();
}

void ServerContext::HandleWriteChunk(ClientConnection& client,
                                     const Chunk& chunk) {
  switch (state_) {
    case kInactive:
      PW_DCHECK("Never should handle chunk while in kInactive state");
      return;
    case kCompleted: {
      // If the chunk is a repeat of the final chunk, resend the status chunk,
      // which apparently was lost. Otherwise, send FAILED_PRECONDITION since
      // this is for a non-pending transfer.
      Status response = status_;
      if (!chunk.IsFinalSendChunk()) {
        response = Status::FailedPrecondition();
        state_ = kInactive;  // Sender should only should retry with final chunk
      }
      client.SendStatusChunk(type_, transfer_id(), response);
      return;
    }
    case kData:
      ProcessWriteDataChunk(client, chunk);
      break;
    case kRecovery:
      if (chunk.offset != offset()) {
        if (last_client_offset_ == chunk.offset) {
          PW_LOG_DEBUG(
              "Transfer %u received repeated offset %u; retry detected, "
              "resending write transfer parameters",
              static_cast<unsigned>(transfer_id()),
              static_cast<unsigned>(chunk.offset));
          SendWriteTransferParameters(client);
        } else {
          PW_LOG_DEBUG("Transfer %u waiting for offset %u, ignoring %u",
                       static_cast<unsigned>(transfer_id()),
                       static_cast<unsigned>(offset()),
                       static_cast<unsigned>(chunk.offset));
        }
        break;
      }

      PW_LOG_DEBUG("Transfer %u received expected offset %u, resuming transfer",
                   static_cast<unsigned>(transfer_id()),
                   static_cast<unsigned>(offset()));
      state_ = kData;

      ProcessWriteDataChunk(client, chunk);
      break;
  }
  // Update the last offset seen from the client so retries can be detected.
  last_client_offset_ = chunk.offset;
}

void ServerContext::ProcessWriteDataChunk(ClientConnection& client,
                                          const Chunk& chunk) {
  if (chunk.data.size() > pending_bytes()) {
    // End the transfer, as this indcates a bug with the client implementation
    // where it doesn't respect pending_bytes. Trying to recover from here
    // could potentially result in an infinite transfer loop.
    PW_LOG_ERROR(
        "Received more data than what was requested; terminating transfer.");
    FinishAndSendStatus(client, Status::Internal());
    return;
  }

  if (chunk.offset != offset()) {
    // Bad offset; reset pending_bytes to send another parameters chunk.
    PW_LOG_DEBUG(
        "Transfer %u expected offset %u, received %u; entering recovery state",
        static_cast<unsigned>(transfer_id()),
        static_cast<unsigned>(offset()),
        static_cast<unsigned>(chunk.offset));
    SendWriteTransferParameters(client);
    state_ = kRecovery;
    return;
  }

  // Write the received data to the writer.
  if (!chunk.data.empty()) {
    if (Status status = writer().Write(chunk.data); !status.ok()) {
      PW_LOG_ERROR(
          "Transfer %u write of %u B chunk failed with status %u; aborting "
          "with DATA_LOSS",
          static_cast<unsigned>(chunk.transfer_id),
          static_cast<unsigned>(chunk.data.size()),
          status.code());
      FinishAndSendStatus(client, Status::DataLoss());
      return;
    }
  }

  // When the client sets remaining_bytes to 0, it indicates completion of the
  // transfer. Acknowledge the completion through a status chunk and clean up.
  if (chunk.IsFinalSendChunk()) {
    FinishAndSendStatus(client, OkStatus());
    return;
  }

  // Update the transfer state.
  advance_offset(chunk.data.size());
  set_pending_bytes(pending_bytes() - chunk.data.size());

  if (pending_bytes() == 0u) {
    // All pending data has been received. Send a new parameters chunk to start
    // the next batch.
    SendWriteTransferParameters(client);
  }

  // Expecting more chunks to be sent by the client.
}

void ServerContext::SendWriteTransferParameters(ClientConnection& client) {
  const size_t write_limit = writer().ConservativeWriteLimit();
  if (write_limit == 0) {
    PW_LOG_WARN(
        "Transfer %u writer returned 0 from ConservativeWriteLimit(); cannot "
        "continue, aborting with RESOURCE_EXHAUSTED",
        static_cast<unsigned>(transfer_id()));
    FinishAndSendStatus(client, Status::ResourceExhausted());
    return;
  }

  set_pending_bytes(std::min(client.max_parameters().pending_bytes(),
                             static_cast<uint32_t>(write_limit)));

  const uint32_t max_chunk_size_bytes =
      MaxWriteChunkSize(client.max_parameters().max_chunk_size_bytes(),
                        client.write_stream().channel_id());
  const internal::Chunk parameters = {
      .transfer_id = transfer_id(),
      .pending_bytes = pending_bytes(),
      .max_chunk_size_bytes = max_chunk_size_bytes,
      .offset = offset(),
  };

  PW_LOG_DEBUG(
      "Transfer %u sending transfer parameters: "
      "offset=%u, pending_bytes=%u, chunk_size=%u",
      static_cast<unsigned>(transfer_id()),
      static_cast<unsigned>(offset()),
      static_cast<unsigned>(pending_bytes()),
      static_cast<unsigned>(max_chunk_size_bytes));

  // If the parameters can't be encoded or sent, it most likely indicates a
  // transport-layer issue, so there isn't much that can be done by the transfer
  // service. The client will time out and can try to restart the transfer.
  Result<ConstByteSpan> data =
      internal::EncodeChunk(parameters, client.write_stream().PayloadBuffer());
  if (!data.ok()) {
    PW_LOG_ERROR("Failed to encode parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 data.status().code());
    client.write_stream().ReleaseBuffer();
    FinishAndSendStatus(client, Status::Internal());
    return;
  }

  if (const Status status = client.write_stream().Write(*data); !status.ok()) {
    PW_LOG_ERROR("Failed to write parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 status.code());
    Finish(Status::Internal());
    return;
  }
}

void ServerContext::FinishAndSendStatus(ClientConnection& client,
                                        Status status) {
  const uint32_t id = transfer_id();
  PW_LOG_INFO("Transfer %u completed with status %u; sending final chunk",
              static_cast<unsigned>(id),
              status.code());
  status.Update(Finish(status));

  client.SendStatusChunk(type_, id, status);
}

Result<ServerContext*> ServerContextPool::StartTransfer(uint32_t transfer_id) {
  ServerContext* new_transfer = nullptr;

  // Check if the ID belongs to an active transfer. If not, pick an inactive
  // slot to start a new transfer.
  for (ServerContext& transfer : transfers_) {
    if (transfer.active()) {
      // Check if restarting a currently pending transfer.
      if (transfer.transfer_id() == transfer_id) {
        PW_LOG_DEBUG(
            "Received initial chunk for transfer %u which was already in "
            "progress; aborting and restarting",
            static_cast<unsigned>(transfer_id));
        transfer.Finish(Status::Aborted());
        new_transfer = &transfer;
        break;
      }
    } else {
      // Remember this but keep searching for an active transfer with this ID.
      new_transfer = &transfer;
    }
  }

  if (new_transfer == nullptr) {
    return Status::Unavailable();
  }

  // Try to start the new transfer by checking if a handler for it exists.
  auto handler = std::find_if(handlers_.begin(), handlers_.end(), [&](auto& h) {
    return h.id() == transfer_id;
  });

  if (handler == handlers_.end()) {
    return Status::NotFound();
  }

  PW_TRY(new_transfer->Start(type_, *handler));
  return new_transfer;
}

Result<ServerContext*> ServerContextPool::GetPendingTransfer(
    uint32_t transfer_id) {
  auto transfer =
      std::find_if(transfers_.begin(), transfers_.end(), [=](const auto& t) {
        return t.initialized() && t.transfer_id() == transfer_id;
      });

  if (transfer == transfers_.end()) {
    PW_LOG_DEBUG("Ignoring chunk for transfer %u, which is not pending",
                 static_cast<unsigned>(transfer_id));
    return Status::FailedPrecondition();
  }

  return &(*transfer);
}

}  // namespace pw::transfer::internal
