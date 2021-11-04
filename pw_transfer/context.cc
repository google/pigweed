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

#include "pw_transfer/internal/context.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

void Context::UpdateParameters(const TransferParameters& max_parameters,
                               const Chunk& chunk) {
  offset_ = chunk.offset;

  if (chunk.pending_bytes.has_value()) {
    pending_bytes_ = chunk.pending_bytes.value();
  }

  if (chunk.max_chunk_size_bytes.has_value()) {
    max_chunk_size_bytes_ = std::min(chunk.max_chunk_size_bytes.value(),
                                     max_parameters.max_chunk_size_bytes());
  }
}

bool Context::ReadTransmitChunk(const TransferParameters& max_parameters,
                                const Chunk& chunk) {
  switch (state_) {
    case kInactive:
      PW_CRASH("Never should handle chunk while in kInactive state");

    case kRecovery:
      PW_CRASH("Transmit transfer should not enter recovery state.");

    case kCompleted:
      // Transfer is not pending; notify the sender.
      SendStatusChunk(Status::FailedPrecondition());
      state_ = kInactive;
      return false;

    case kData:
      // Continue with reading the chunk.
      break;
  }

  if (!chunk.pending_bytes.has_value()) {
    // Malformed chunk.
    FinishAndSendStatus(Status::InvalidArgument());
    return false;
  }

  if (chunk.pending_bytes == 0u) {
    PW_LOG_ERROR("Transfer %d receiver requested 0 bytes (invalid); aborting",
                 static_cast<unsigned>(transfer_id_));
    FinishAndSendStatus(Status::Internal());
    return false;
  }

  // If the offsets don't match, attempt to seek on the reader. Not all readers
  // support seeking; abort with UNIMPLEMENTED if this handler doesn't.
  if (offset_ != chunk.offset) {
    if (Status seek_status = reader().Seek(chunk.offset); !seek_status.ok()) {
      PW_LOG_WARN("Transfer %u seek to %u failed with status %u",
                  static_cast<unsigned>(transfer_id_),
                  static_cast<unsigned>(chunk.offset),
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

      FinishAndSendStatus(seek_status);
      return false;
    }
  }

  UpdateParameters(max_parameters, chunk);
  return true;
}

void Context::ProcessTransmitChunk() {
  Status status;

  while ((status = SendNextDataChunk()).ok()) {
    // Continue until all requested bytes are sent.
  }

  // If all bytes are successfully sent, SendNextChunk will return OUT_OF_RANGE.
  if (!status.IsOutOfRange()) {
    FinishAndSendStatus(status);
  }
}

bool Context::ReadReceiveChunk(ChunkDataBuffer& buffer,
                               const TransferParameters& max_parameters,
                               const Chunk& chunk) {
  switch (state_) {
    case kInactive:
      PW_CRASH("Never should handle chunk while in kInactive state");

    case kCompleted: {
      // If the chunk is a repeat of the final chunk, resend the status chunk,
      // which apparently was lost. Otherwise, send FAILED_PRECONDITION since
      // this is for a non-pending transfer.
      Status response = status_;
      if (!chunk.IsFinalTransmitChunk()) {
        response = Status::FailedPrecondition();
        // Sender should only should retry with final chunk
        state_ = kInactive;
      }
      SendStatusChunk(response);
      return false;
    }

    case kData:
      if (!HandleDataChunk(buffer, max_parameters, chunk)) {
        return false;
      }
      break;

    case kRecovery:
      if (chunk.offset != offset_) {
        if (last_received_offset_ == chunk.offset) {
          PW_LOG_DEBUG(
              "Transfer %u received repeated offset %u; retry detected, "
              "resending transfer parameters",
              static_cast<unsigned>(transfer_id_),
              static_cast<unsigned>(chunk.offset));
          if (!SendTransferParameters(max_parameters).ok()) {
            return false;
          }
        } else {
          PW_LOG_DEBUG("Transfer %u waiting for offset %u, ignoring %u",
                       static_cast<unsigned>(transfer_id_),
                       static_cast<unsigned>(offset_),
                       static_cast<unsigned>(chunk.offset));
        }

        last_received_offset_ = chunk.offset;
        return false;
      }

      PW_LOG_DEBUG("Transfer %u received expected offset %u, resuming transfer",
                   static_cast<unsigned>(transfer_id_),
                   static_cast<unsigned>(offset_));
      state_ = kData;

      if (!HandleDataChunk(buffer, max_parameters, chunk)) {
        return false;
      }
      break;
  }

  // Update the last offset seen so that retries can be detected.
  last_received_offset_ = chunk.offset;
  return true;
}

void Context::ProcessReceiveChunk(ChunkDataBuffer& buffer,
                                  const TransferParameters& max_parameters) {
  // Write staged data from the buffer to the stream.
  if (!buffer.empty()) {
    if (Status status = writer().Write(buffer); !status.ok()) {
      PW_LOG_ERROR(
          "Transfer %u write of %u B chunk failed with status %u; aborting "
          "with DATA_LOSS",
          static_cast<unsigned>(transfer_id_),
          static_cast<unsigned>(buffer.size()),
          status.code());
      FinishAndSendStatus(Status::DataLoss());
      return;
    }
  }

  // When the client sets remaining_bytes to 0, it indicates completion of the
  // transfer. Acknowledge the completion through a status chunk and clean up.
  if (buffer.last_chunk()) {
    FinishAndSendStatus(OkStatus());
    return;
  }

  // Update the transfer state.
  offset_ += buffer.size();
  pending_bytes_ -= buffer.size();

  // TODO(frolv): Release the buffer.

  if (pending_bytes_ == 0u) {
    // All pending data has been received. Send a new parameters chunk to start
    // the next batch.
    SendTransferParameters(max_parameters);
  }
}

Status Context::SendNextDataChunk() {
  if (pending_bytes_ == 0) {
    return Status::OutOfRange();
  }

  ByteSpan buffer = rpc_writer_->PayloadBuffer();

  // Begin by doing a partial encode of all the metadata fields, leaving the
  // buffer with usable space for the chunk data at the end.
  transfer::Chunk::MemoryEncoder encoder{buffer};
  encoder.WriteTransferId(transfer_id_).IgnoreError();
  encoder.WriteOffset(offset_).IgnoreError();

  // Reserve space for the data proto field overhead and use the remainder of
  // the buffer for the chunk data.
  size_t reserved_size = encoder.size() + 1 /* data key */ + 5 /* data size */;

  ByteSpan data_buffer = buffer.subspan(reserved_size);
  size_t max_bytes_to_send = std::min(pending_bytes_, max_chunk_size_bytes_);

  if (max_bytes_to_send < data_buffer.size()) {
    data_buffer = data_buffer.first(max_bytes_to_send);
  }

  Result<ByteSpan> data = reader().Read(data_buffer);
  if (data.status().IsOutOfRange()) {
    // No more data to read.
    encoder.WriteRemainingBytes(0).IgnoreError();
    pending_bytes_ = 0;
  } else if (data.ok()) {
    encoder.WriteData(data.value()).IgnoreError();
    offset_ += data.value().size();
    pending_bytes_ -= data.value().size();
  } else {
    PW_LOG_ERROR("Transfer %u Read() failed with status %u",
                 static_cast<unsigned>(transfer_id_),
                 data.status().code());
    rpc_writer_->ReleaseBuffer();
    return Status::DataLoss();
  }

  if (!encoder.status().ok()) {
    PW_LOG_ERROR("Transfer %u failed to encode transmit chunk",
                 static_cast<unsigned>(transfer_id_));
    rpc_writer_->ReleaseBuffer();
    return Status::Internal();
  }

  if (const Status status = rpc_writer_->Write(encoder); !status.ok()) {
    PW_LOG_ERROR("Transfer %u failed to send transmit chunk, status %u",
                 static_cast<unsigned>(transfer_id_),
                 status.code());
    return Status::DataLoss();
  }

  return data.status();
}

bool Context::HandleDataChunk(ChunkDataBuffer& buffer,
                              const TransferParameters& max_parameters,
                              const Chunk& chunk) {
  if (chunk.data.size() > pending_bytes_) {
    // End the transfer, as this indcates a bug with the client implementation
    // where it doesn't respect pending_bytes. Trying to recover from here
    // could potentially result in an infinite transfer loop.
    PW_LOG_ERROR(
        "Received more data than what was requested; terminating transfer.");
    FinishAndSendStatus(Status::Internal());
    return false;
  }

  if (chunk.offset != offset_) {
    // Bad offset; reset pending_bytes to send another parameters chunk.
    PW_LOG_DEBUG(
        "Transfer %u expected offset %u, received %u; entering recovery state",
        static_cast<unsigned>(transfer_id_),
        static_cast<unsigned>(offset_),
        static_cast<unsigned>(chunk.offset));
    SendTransferParameters(max_parameters);
    state_ = kRecovery;

    // Return false as there is no immediate deferred work to complete. The
    // transfer must wait for the next data chunk to be sent by the transmitter.
    return false;
  }

  // Write the chunk data to the buffer to be processed later. If the chunk has
  // no data, this will clear the buffer.
  buffer.Write(chunk.data, chunk.IsFinalTransmitChunk());
  return true;
}

Status Context::SendTransferParameters(
    const TransferParameters& max_parameters) {
  const size_t write_limit = writer().ConservativeWriteLimit();
  if (write_limit == 0) {
    PW_LOG_WARN(
        "Transfer %u writer returned 0 from ConservativeWriteLimit(); cannot "
        "continue, aborting with RESOURCE_EXHAUSTED",
        static_cast<unsigned>(transfer_id_));
    FinishAndSendStatus(Status::ResourceExhausted());
    return Status::ResourceExhausted();
  }

  set_pending_bytes(std::min(max_parameters.pending_bytes(),
                             static_cast<uint32_t>(write_limit)));

  const uint32_t max_chunk_size_bytes = MaxWriteChunkSize(
      max_parameters.max_chunk_size_bytes(), rpc_writer_->channel_id());

  const internal::Chunk parameters = {
      .transfer_id = transfer_id_,
      .pending_bytes = pending_bytes_,
      .max_chunk_size_bytes = max_chunk_size_bytes,
      .offset = static_cast<uint32_t>(offset_),
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
      internal::EncodeChunk(parameters, rpc_writer_->PayloadBuffer());
  if (!data.ok()) {
    PW_LOG_ERROR("Failed to encode parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 data.status().code());
    rpc_writer_->ReleaseBuffer();
    FinishAndSendStatus(Status::Internal());
    return Status::Internal();
  }

  if (const Status status = rpc_writer_->Write(*data); !status.ok()) {
    PW_LOG_ERROR("Failed to write parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 status.code());
    if (on_completion_) {
      on_completion_(*this, Status::Internal());
    }
    return Status::Internal();
  }

  return OkStatus();
}

void Context::Start(Type type,
                    uint32_t transfer_id,
                    RawWriter& rpc_writer,
                    stream::Stream& stream) {
  PW_DCHECK(!active());

  transfer_id_ = transfer_id;
  state_ = kData;
  type_ = type;

  rpc_writer_ = &rpc_writer;
  stream_ = &stream;

  offset_ = 0;
  pending_bytes_ = 0;
  max_chunk_size_bytes_ = std::numeric_limits<uint32_t>::max();

  status_ = Status::Unknown();
  last_received_offset_ = 0;
}

void Context::SendStatusChunk(Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id_;
  chunk.status = status.code();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, rpc_writer_->PayloadBuffer());

  if (!result.ok()) {
    PW_LOG_ERROR("Failed to encode final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id_));
    rpc_writer_->ReleaseBuffer();
    return;
  }

  if (!rpc_writer_->Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id_));
    return;
  }
}

void Context::FinishAndSendStatus(Status status) {
  PW_LOG_INFO("Transfer %u completed with status %u; sending final chunk",
              static_cast<unsigned>(transfer_id_),
              status.code());

  status_ = status;

  if (on_completion_ != nullptr) {
    status.Update(on_completion_(*this, status));
  }

  SendStatusChunk(status);
  state_ = kCompleted;
}

uint32_t Context::MaxWriteChunkSize(uint32_t max_chunk_size_bytes,
                                    uint32_t channel_id) const {
  // Start with the user-provided maximum chunk size, which should be the usable
  // payload length on the RPC ingress path after any transport overhead.
  ssize_t max_size = max_chunk_size_bytes;

  // Subtract the RPC overhead (pw_rpc/internal/packet.proto).
  //
  //   type:       1 byte key, 1 byte value (CLIENT_STREAM)
  //   channel_id: 1 byte key, varint value (calculate from stream)
  //   service_id: 1 byte key, 4 byte value
  //   method_id:  1 byte key, 4 byte value
  //   payload:    1 byte key, varint length (remaining space)
  //   status:     0 bytes (not set in stream packets)
  //
  //   TOTAL: 14 bytes + encoded channel_id size + encoded payload length
  //
  max_size -= 14;
  max_size -= varint::EncodedSize(channel_id);
  max_size -= varint::EncodedSize(max_size);

  // TODO(frolv): Temporarily add 5 bytes for the new call_id change. The RPC
  // overhead calculation will be moved into an RPC helper to avoid having
  // pw_transfer depend on RPC internals.
  max_size -= 5;

  // Subtract the transfer service overhead for a client write chunk
  // (pw_transfer/transfer.proto).
  //
  //   transfer_id: 1 byte key, varint value (calculate)
  //   offset:      1 byte key, varint value (calculate)
  //   data:        1 byte key, varint length (remaining space)
  //
  //   TOTAL: 3 + encoded transfer_id + encoded offset + encoded data length
  //
  size_t max_offset_in_window = offset() + pending_bytes();
  max_size -= 3;
  max_size -= varint::EncodedSize(transfer_id());
  max_size -= varint::EncodedSize(max_offset_in_window);
  max_size -= varint::EncodedSize(max_size);

  // A resulting value of zero (or less) renders write transfers unusable, as
  // there is no space to send any payload. This should be considered a
  // programmer error in the transfer service setup.
  PW_CHECK_INT_GT(
      max_size,
      0,
      "Transfer service maximum chunk size is too small to fit a payload. "
      "Increase max_chunk_size_bytes to support write transfers.");

  return max_size;
}

}  // namespace pw::transfer::internal
