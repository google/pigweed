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

#include <mutex>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

Status Context::InitiateTransfer(const TransferParameters& max_parameters) {
  PW_DCHECK(active());

  if (type() == kReceive) {
    // A receiver begins a new transfer with a parameters chunk telling the
    // transmitter what to send.
    PW_TRY(UpdateAndSendTransferParameters(max_parameters, kRetransmit));
  } else {
    PW_TRY(SendInitialTransmitChunk());
  }

  timer_.InvokeAfter(chunk_timeout_);
  return OkStatus();
}

bool Context::ReadChunkData(ChunkDataBuffer& buffer,
                            const TransferParameters& max_parameters,
                            const Chunk& chunk) {
  CancelTimer();

  if (type() == kTransmit) {
    return ReadTransmitChunk(max_parameters, chunk);
  }
  return ReadReceiveChunk(buffer, max_parameters, chunk);
}

void Context::ProcessChunk(ChunkDataBuffer& buffer,
                           const TransferParameters& max_parameters) {
  if (type() == kTransmit) {
    ProcessTransmitChunk();
  } else {
    ProcessReceiveChunk(buffer, max_parameters);
  }

  if (active()) {
    timer_.InvokeAfter(chunk_timeout_);
  }
}

Status Context::SendInitialTransmitChunk() {
  // A transmitter begins a transfer by just sending its ID.
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id_;

  Result<ConstByteSpan> result =
      EncodeChunk(chunk, rpc_writer_->PayloadBuffer());
  if (!result.ok()) {
    rpc_writer_->ReleasePayloadBuffer();
    return result.status();
  }

  return rpc_writer_->Write(*result);
}

bool Context::ReadTransmitChunk(const TransferParameters& max_parameters,
                                const Chunk& chunk) {
  {
    std::lock_guard lock(state_lock_);

    switch (transfer_state_) {
      case TransferState::kInactive:
        PW_CRASH("Never should handle chunk while in kInactive state");

      case TransferState::kRecovery:
        PW_CRASH("Transmit transfer should not enter recovery state.");

      case TransferState::kCompleted:
        // Transfer is not pending; notify the sender.
        SendStatusChunk(Status::FailedPrecondition());
        transfer_state_ = TransferState::kInactive;
        return false;

      case TransferState::kData:
        // Continue with reading the chunk.
        break;

      case TransferState::kTimedOut:
        // Drop the chunk to let the timeout handler run.
        return false;
    }
  }

  if (!chunk.pending_bytes.has_value()) {
    // Malformed chunk.
    FinishAndSendStatus(Status::InvalidArgument());
    return false;
  }

  bool retransmit = true;
  if (chunk.type.has_value()) {
    retransmit = chunk.type == Chunk::Type::kParametersRetransmit;
  }

  if (retransmit) {
    // If the offsets don't match, attempt to seek on the reader. Not all
    // readers support seeking; abort with UNIMPLEMENTED if this handler
    // doesn't.
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

    // Retransmit is the default behavior for older versions of the transfer
    // protocol. The window_end_offset field is not guaranteed to be set in
    // these versions, so it must be calculated.
    offset_ = chunk.offset;
    window_end_offset_ = offset_ + chunk.pending_bytes.value();
    pending_bytes_ = chunk.pending_bytes.value();
  } else {
    window_end_offset_ = chunk.window_end_offset;
  }

  if (chunk.max_chunk_size_bytes.has_value()) {
    max_chunk_size_bytes_ = std::min(chunk.max_chunk_size_bytes.value(),
                                     max_parameters.max_chunk_size_bytes());
  }

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
  state_lock_.lock();

  switch (transfer_state_) {
    case TransferState::kInactive:
      PW_CRASH("Never should handle chunk while in kInactive state");

    case TransferState::kTimedOut:
      // Drop the chunk to let the timeout handler run.
      state_lock_.unlock();
      return false;

    case TransferState::kCompleted: {
      // If the chunk is a repeat of the final chunk, resend the status chunk,
      // which apparently was lost. Otherwise, send FAILED_PRECONDITION since
      // this is for a non-pending transfer.
      Status response = status_;
      if (!chunk.IsFinalTransmitChunk()) {
        response = Status::FailedPrecondition();
        // Sender should only should retry with final chunk
        transfer_state_ = TransferState::kInactive;
      }
      state_lock_.unlock();
      SendStatusChunk(response);
      return false;
    }

    case TransferState::kData:
      state_lock_.unlock();
      if (!HandleDataChunk(buffer, max_parameters, chunk)) {
        return false;
      }
      break;

    case TransferState::kRecovery:
      if (chunk.offset != offset_) {
        state_lock_.unlock();

        if (last_chunk_offset_ == chunk.offset) {
          PW_LOG_DEBUG(
              "Transfer %u received repeated offset %u; retry detected, "
              "resending transfer parameters",
              static_cast<unsigned>(transfer_id_),
              static_cast<unsigned>(chunk.offset));
          if (!UpdateAndSendTransferParameters(max_parameters, kRetransmit)
                   .ok()) {
            return false;
          }
        } else {
          PW_LOG_DEBUG("Transfer %u waiting for offset %u, ignoring %u",
                       static_cast<unsigned>(transfer_id_),
                       static_cast<unsigned>(offset_),
                       static_cast<unsigned>(chunk.offset));
        }

        last_chunk_offset_ = chunk.offset;
        return false;
      }

      PW_LOG_DEBUG("Transfer %u received expected offset %u, resuming transfer",
                   static_cast<unsigned>(transfer_id_),
                   static_cast<unsigned>(offset_));

      transfer_state_ = TransferState::kData;
      state_lock_.unlock();

      if (!HandleDataChunk(buffer, max_parameters, chunk)) {
        return false;
      }
      break;
  }

  // Update the last offset seen so that retries can be detected.
  last_chunk_offset_ = chunk.offset;
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

  // Once the transmitter has sent a sufficient amount of data, try to extend
  // the window to allow it to continue sending data without blocking.
  uint32_t remaining_window_size = window_end_offset_ - offset_;
  bool extend_window = remaining_window_size <=
                       window_size_ / TransferParameters::kExtendWindowDivisor;

  if (pending_bytes_ == 0u) {
    // First chunk of a receive transfer (transfer_id only). This condition
    // should be updated to explicitly check for the first chunk.
    UpdateAndSendTransferParameters(max_parameters, kRetransmit);
  } else if (extend_window) {
    UpdateAndSendTransferParameters(max_parameters, kExtend);
  }
}

Status Context::SendNextDataChunk() {
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
  size_t max_bytes_to_send =
      std::min(window_end_offset_ - offset_, max_chunk_size_bytes_);

  if (max_bytes_to_send < data_buffer.size()) {
    data_buffer = data_buffer.first(max_bytes_to_send);
  }

  Result<ByteSpan> data = reader().Read(data_buffer);
  if (data.status().IsOutOfRange()) {
    // No more data to read.
    encoder.WriteRemainingBytes(0).IgnoreError();
    window_end_offset_ = offset_;
    pending_bytes_ = 0;
  } else if (data.ok()) {
    if (offset_ == window_end_offset_) {
      PW_LOG_DEBUG(
          "Transfer %u is not finished, but the receiver cannot accept any "
          "more data (offset == window_end_offset)",
          static_cast<unsigned>(transfer_id_));
      rpc_writer_->ReleasePayloadBuffer();
      return Status::ResourceExhausted();
    }

    encoder.WriteData(data.value()).IgnoreError();
    last_chunk_offset_ = offset_;
    offset_ += data.value().size();
    pending_bytes_ -= data.value().size();
  } else {
    PW_LOG_ERROR("Transfer %u Read() failed with status %u",
                 static_cast<unsigned>(transfer_id_),
                 data.status().code());
    rpc_writer_->ReleasePayloadBuffer();
    return Status::DataLoss();
  }

  if (!encoder.status().ok()) {
    PW_LOG_ERROR("Transfer %u failed to encode transmit chunk",
                 static_cast<unsigned>(transfer_id_));
    rpc_writer_->ReleasePayloadBuffer();
    return Status::Internal();
  }

  if (const Status status = rpc_writer_->Write(encoder); !status.ok()) {
    PW_LOG_ERROR("Transfer %u failed to send transmit chunk, status %u",
                 static_cast<unsigned>(transfer_id_),
                 status.code());
    return Status::DataLoss();
  }

  flags_ |= kFlagsDataSent;

  if (offset_ == window_end_offset_) {
    return Status::OutOfRange();
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
    UpdateAndSendTransferParameters(max_parameters, kRetransmit);
    set_transfer_state(TransferState::kRecovery);

    // Return false as there is no immediate deferred work to complete. The
    // transfer must wait for the next data chunk to be sent by the transmitter.
    return false;
  }

  // Write the chunk data to the buffer to be processed later. If the chunk has
  // no data, this will clear the buffer.
  buffer.Write(chunk.data, chunk.IsFinalTransmitChunk());
  return true;
}

Status Context::SendTransferParameters(TransmitAction action) {
  const internal::Chunk parameters = {
      .transfer_id = transfer_id_,
      .window_end_offset = window_end_offset_,
      .pending_bytes = pending_bytes_,
      .max_chunk_size_bytes = max_chunk_size_bytes_,
      .min_delay_microseconds = kDefaultChunkDelayMicroseconds,
      .offset = offset_,
      .type = action == kRetransmit
                  ? internal::Chunk::Type::kParametersRetransmit
                  : internal::Chunk::Type::kParametersContinue,
  };

  PW_LOG_DEBUG(
      "Transfer %u sending transfer parameters: "
      "offset=%u, window_end_offset=%u, pending_bytes=%u, chunk_size=%u",
      static_cast<unsigned>(transfer_id_),
      static_cast<unsigned>(offset_),
      static_cast<unsigned>(window_end_offset_),
      static_cast<unsigned>(pending_bytes_),
      static_cast<unsigned>(max_chunk_size_bytes_));

  // If the parameters can't be encoded or sent, it most likely indicates a
  // transport-layer issue, so there isn't much that can be done by the transfer
  // service. The client will time out and can try to restart the transfer.
  Result<ConstByteSpan> data =
      internal::EncodeChunk(parameters, rpc_writer_->PayloadBuffer());
  if (!data.ok()) {
    PW_LOG_ERROR("Failed to encode parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 data.status().code());
    rpc_writer_->ReleasePayloadBuffer();
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

Status Context::UpdateAndSendTransferParameters(
    const TransferParameters& max_parameters, TransmitAction action) {
  size_t pending_bytes =
      std::min(max_parameters.pending_bytes(),
               static_cast<uint32_t>(writer().ConservativeWriteLimit()));

  window_size_ = pending_bytes;
  window_end_offset_ = offset_ + pending_bytes;
  pending_bytes_ = pending_bytes;

  max_chunk_size_bytes_ = MaxWriteChunkSize(
      max_parameters.max_chunk_size_bytes(), rpc_writer_->channel_id());

  return SendTransferParameters(action);
}

void Context::Initialize(Type type,
                         uint32_t transfer_id,
                         work_queue::WorkQueue& work_queue,
                         rpc::Writer& rpc_writer,
                         stream::Stream& stream,
                         chrono::SystemClock::duration chunk_timeout,
                         uint8_t max_retries) {
  PW_DCHECK(!active());
  PW_CHECK(state_lock_.try_lock());

  transfer_id_ = transfer_id;
  flags_ = static_cast<uint8_t>(type);
  transfer_state_ = TransferState::kData;
  retries_ = 0;
  max_retries_ = max_retries;

  rpc_writer_ = &rpc_writer;
  stream_ = &stream;

  offset_ = 0;
  window_size_ = 0;
  window_end_offset_ = 0;
  pending_bytes_ = 0;
  max_chunk_size_bytes_ = std::numeric_limits<uint32_t>::max();

  last_chunk_offset_ = 0;
  chunk_timeout_ = chunk_timeout;
  work_queue_ = &work_queue;

  state_lock_.unlock();
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
    rpc_writer_->ReleasePayloadBuffer();
    return;
  }

  if (!rpc_writer_->Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id_));
    return;
  }
}

void Context::FinishAndSendStatus(Status status) {
  CancelTimer();

  PW_LOG_INFO("Transfer %u completed with status %u; sending final chunk",
              static_cast<unsigned>(transfer_id_),
              status.code());

  status_ = status;

  if (on_completion_ != nullptr) {
    status.Update(on_completion_(*this, status));
  }

  SendStatusChunk(status);
  set_transfer_state(TransferState::kCompleted);
}

void Context::OnTimeout() {
  std::lock_guard lock(state_lock_);
  transfer_state_ = TransferState::kTimedOut;

  const Status status = work_queue_->PushWork([this]() {
    HandleTimeout();
    if (active()) {
      timer_.InvokeAfter(chunk_timeout_);
    }
  });

  if (!status.ok()) {
    PW_LOG_ERROR("Transfer %u failed to push timeout handler to work queue",
                 static_cast<unsigned>(transfer_id_));

    // If the work queue is full, there is no way to keep the transfer alive or
    // notify the other end of the failure. Simply end the transfer; if any more
    // chunks are received, an error will be sent then.
    status_ = Status::DeadlineExceeded();
    transfer_state_ = TransferState::kCompleted;
  }
}

void Context::HandleTimeout() {
  state_lock_.lock();
  PW_DCHECK(transfer_state_ == TransferState::kTimedOut);

  if (retries_ == max_retries_) {
    PW_LOG_ERROR("Transfer %u failed to receive a chunk after %u retries.",
                 static_cast<unsigned>(transfer_id_),
                 static_cast<unsigned>(retries_));
    PW_LOG_ERROR("Canceling transfer.");
    state_lock_.unlock();
    FinishAndSendStatus(Status::DeadlineExceeded());
    return;
  }

  ++retries_;
  transfer_state_ = TransferState::kData;
  state_lock_.unlock();

  if (type() == kReceive) {
    // Resend the most recent transfer parameters. SendTransferParameters()
    // internally handles failures, so its status can be ignored.
    PW_LOG_DEBUG(
        "Receive transfer %u timed out waiting for chunk; resending parameters",
        static_cast<unsigned>(transfer_id_));
    SendTransferParameters(kRetransmit).IgnoreError();
    return;
  }

  // In a transmit, if a data chunk has not yet been sent, the initial transfer
  // parameters did not arrive from the receiver. Resend the initial chunk.
  if ((flags_ & kFlagsDataSent) != kFlagsDataSent) {
    PW_LOG_DEBUG(
        "Transmit transfer %u timed out waiting for initial parameters",
        static_cast<unsigned>(transfer_id_));
    SendInitialTransmitChunk();
    return;
  }

  // Otherwise, resend the most recent chunk. If the reader doesn't support
  // seeking, this isn't possible, so just terminate the transfer immediately.
  if (!reader().Seek(last_chunk_offset_).ok()) {
    PW_LOG_ERROR("Transmit transfer %d timed out waiting for new parameters.",
                 static_cast<unsigned>(transfer_id_));
    PW_LOG_ERROR("Retrying requires a seekable reader. Alas, ours is not.");
    FinishAndSendStatus(Status::DeadlineExceeded());
    return;
  }

  // Rewind the transfer position and resend the chunk.
  size_t last_size_sent = offset_ - last_chunk_offset_;
  offset_ = last_chunk_offset_;
  pending_bytes_ += last_size_sent;

  ProcessTransmitChunk();
}

uint32_t Context::MaxWriteChunkSize(uint32_t max_chunk_size_bytes,
                                    uint32_t channel_id) const {
  // Start with the user-provided maximum chunk size, which should be the usable
  // payload length on the RPC ingress path after any transport overhead.
  ptrdiff_t max_size = max_chunk_size_bytes;

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
  max_size -= 3;
  max_size -= varint::EncodedSize(transfer_id_);
  max_size -= varint::EncodedSize(window_end_offset_);
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
