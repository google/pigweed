// Copyright 2022 The Pigweed Authors
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

#include <chrono>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_status/try.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer/transfer_thread.h"
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

void Context::HandleEvent(const Event& event) {
  switch (event.type) {
    case EventType::kNewClientTransfer:
    case EventType::kNewServerTransfer: {
      if (active()) {
        Finish(Status::Aborted());
      }

      if (Status status = StartTransfer(event.new_transfer); !status.ok()) {
        // TODO(frolv): Figure out a better way.
        transfer_id_ = event.new_transfer.transfer_id;
        rpc_writer_ = event.new_transfer.rpc_writer;
        thread_ = event.new_transfer.transfer_thread;
        SendStatusChunk(status);

        ClearTimeout();
        set_transfer_state(TransferState::kInactive);
      }

      return;
    }

    case EventType::kClientChunk:
    case EventType::kServerChunk:
      PW_CHECK(initialized());
      HandleChunkEvent(event.chunk);
      return;

    case EventType::kClientTimeout:
    case EventType::kServerTimeout:
      HandleTimeout();
      return;

    case EventType::kSendStatusChunk:
    case EventType::kSetTransferStream:
    case EventType::kAddTransferHandler:
    case EventType::kRemoveTransferHandler:
    case EventType::kTerminate:
      // These events are intended for the transfer thread and should never be
      // forwarded through to a context.
      PW_CRASH("Transfer context received a transfer thread event");
  }
}

Status Context::InitiateTransfer() {
  PW_DCHECK(active());

  if (type() == TransferType::kReceive) {
    // A receiver begins a new transfer with a parameters chunk telling the
    // transmitter what to send.
    PW_TRY(UpdateAndSendTransferParameters(kRetransmit));
  } else {
    PW_TRY(SendInitialTransmitChunk());
  }

  SetTimeout(chunk_timeout_);
  return OkStatus();
}

Status Context::SendInitialTransmitChunk() {
  // A transmitter begins a transfer by just sending its ID.
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id_;

  Result<ConstByteSpan> result = EncodeChunk(chunk, thread_->encode_buffer());
  if (!result.ok()) {
    return result.status();
  }

  return rpc_writer_->Write(*result);
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
      internal::EncodeChunk(parameters, thread_->encode_buffer());
  if (!data.ok()) {
    PW_LOG_ERROR("Failed to encode parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 data.status().code());
    FinishAndSendStatus(Status::Internal());
    return Status::Internal();
  }

  if (const Status status = rpc_writer_->Write(*data); !status.ok()) {
    PW_LOG_ERROR("Failed to write parameters for transfer %u: %d",
                 static_cast<unsigned>(parameters.transfer_id),
                 status.code());
    DoFinish(Status::Internal());
    return Status::Internal();
  }

  return OkStatus();
}

Status Context::UpdateAndSendTransferParameters(TransmitAction action) {
  size_t pending_bytes =
      std::min(max_parameters_->pending_bytes(),
               static_cast<uint32_t>(writer().ConservativeWriteLimit()));

  window_size_ = pending_bytes;
  window_end_offset_ = offset_ + pending_bytes;
  pending_bytes_ = pending_bytes;

  max_chunk_size_bytes_ = MaxWriteChunkSize(
      max_parameters_->max_chunk_size_bytes(), rpc_writer_->channel_id());

  return SendTransferParameters(action);
}

void Context::Initialize(const NewTransferEvent& new_transfer,
                         stream::Stream& stream) {
  PW_DCHECK(!active());

  transfer_id_ = new_transfer.transfer_id;
  flags_ = static_cast<uint8_t>(new_transfer.type);
  transfer_state_ = TransferState::kWaiting;
  retries_ = 0;
  max_retries_ = new_transfer.max_retries;

  rpc_writer_ = new_transfer.rpc_writer;
  stream_ = &stream;

  offset_ = 0;
  window_size_ = 0;
  window_end_offset_ = 0;
  pending_bytes_ = 0;
  max_chunk_size_bytes_ = new_transfer.max_parameters->max_chunk_size_bytes();

  max_parameters_ = new_transfer.max_parameters;
  thread_ = new_transfer.transfer_thread;

  last_chunk_offset_ = 0;
  chunk_timeout_ = new_transfer.timeout;
  interchunk_delay_ = chrono::SystemClock::for_at_least(
      std::chrono::microseconds(kDefaultChunkDelayMicroseconds));
  next_timeout_ = kNoTimeout;
}

void Context::HandleChunkEvent(const ChunkEvent& event) {
  PW_DCHECK(event.transfer_id == transfer_id_);

  Chunk chunk;
  if (!DecodeChunk(ConstByteSpan(event.data, event.size), chunk).ok()) {
    return;
  }

  if (chunk.status.has_value()) {
    if (active()) {
      Finish(chunk.status.value());
    } else {
      PW_LOG_DEBUG("Got final status %d for completed transfer %d",
                   static_cast<int>(chunk.status.value().code()),
                   static_cast<int>(transfer_id_));
    }
    return;
  }

  if (type() == TransferType::kTransmit) {
    HandleTransmitChunk(chunk);
  } else {
    HandleReceiveChunk(chunk);
  }
}

void Context::HandleTransmitChunk(const Chunk& chunk) {
  switch (transfer_state_) {
    case TransferState::kInactive:
    case TransferState::kRecovery:
      PW_CRASH("Never should handle chunk while inactive");
      break;

    case TransferState::kCompleted:
      // If the transfer has already completed and another chunk is received,
      // tell the other end that the transfer is over.
      //
      // TODO(frolv): Final status chunks should be ACKed by the other end. When
      // that is added, this case should be updated to check if the received
      // chunk is an ACK. If so, the transfer state can be reset to INACTIVE.
      // Otherwise, the final status should be re-sent.
      SendStatusChunk(Status::FailedPrecondition());
      return;

    case TransferState::kWaiting:
    case TransferState::kTransmitting:
      break;
  }

  if (!chunk.pending_bytes.has_value()) {
    // Malformed chunk.
    FinishAndSendStatus(Status::InvalidArgument());
    return;
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
        return;
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
                                     max_parameters_->max_chunk_size_bytes());
  }

  if (chunk.min_delay_microseconds.has_value()) {
    interchunk_delay_ = chrono::SystemClock::for_at_least(
        std::chrono::microseconds(chunk.min_delay_microseconds.value()));
  }

  PW_LOG_DEBUG(
      "Transfer %u received parameters type=%s offset=%u window_end_offset=%u",
      static_cast<unsigned>(transfer_id_),
      retransmit ? "RETRANSMIT" : "CONTINUE",
      static_cast<unsigned>(chunk.offset),
      static_cast<unsigned>(window_end_offset_));

  // Parsed all of the parameters; start sending the window.
  set_transfer_state(TransferState::kTransmitting);

  if (Status status = TransmitNextChunk(); !status.ok()) {
    FinishAndSendStatus(status);
  }
}

Status Context::TransmitNextChunk() {
  ByteSpan buffer = thread_->encode_buffer();

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

    PW_LOG_DEBUG("Transfer %u sending final chunk with remaining_bytes=0",
                 static_cast<unsigned>(transfer_id_));
  } else if (data.ok()) {
    if (offset_ == window_end_offset_) {
      PW_LOG_DEBUG(
          "Transfer %u is not finished, but the receiver cannot accept any "
          "more data (offset == window_end_offset)",
          static_cast<unsigned>(transfer_id_));

      // TODO(frolv): The transfer shouldn't end here as this may just indicate
      // that the receiver is temporarily busy. Instead, we should wait to see
      // if more data is requested.
      return Status::Internal();
    }

    PW_LOG_DEBUG("Transfer %u sending chunk offset=%u size=%u",
                 static_cast<unsigned>(transfer_id_),
                 static_cast<unsigned>(offset_),
                 static_cast<unsigned>(data.value().size()));

    encoder.WriteData(data.value()).IgnoreError();
    last_chunk_offset_ = offset_;
    offset_ += data.value().size();
    pending_bytes_ -= data.value().size();
  } else {
    PW_LOG_ERROR("Transfer %u Read() failed with status %u",
                 static_cast<unsigned>(transfer_id_),
                 data.status().code());
    return Status::DataLoss();
  }

  if (!encoder.status().ok()) {
    PW_LOG_ERROR("Transfer %u failed to encode transmit chunk",
                 static_cast<unsigned>(transfer_id_));
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
    // Sent all requested data. Must now wait for next parameters from the
    // receiver.
    set_transfer_state(TransferState::kWaiting);
    SetTimeout(chunk_timeout_);
  } else {
    // More data is to be sent. Set a timeout to send the next chunk following
    // the chunk delay.
    SetTimeout(chrono::SystemClock::for_at_least(interchunk_delay_));
  }

  return OkStatus();
}

void Context::HandleReceiveChunk(const Chunk& chunk) {
  switch (transfer_state_) {
    case TransferState::kInactive:
      PW_CRASH("Never should handle chunk while inactive");

    case TransferState::kTransmitting:
      PW_CRASH("Receive transfer somehow entered TRANSMITTING state");

    case TransferState::kCompleted:
      // If the transfer has already completed and another chunk is received,
      // re-send the final status chunk.
      //
      // TODO(frolv): Final status chunks should be ACKed by the other end. When
      // that is added, this case should be updated to check if the received
      // chunk is an ACK. If so, the transfer state can be reset to INACTIVE.
      // Otherwise, the final status should be re-sent.
      SendStatusChunk(status_);
      return;

    case TransferState::kRecovery:
      if (chunk.offset != offset_) {
        if (last_chunk_offset_ == chunk.offset) {
          PW_LOG_DEBUG(
              "Transfer %u received repeated offset %u; retry detected, "
              "resending transfer parameters",
              static_cast<unsigned>(transfer_id_),
              static_cast<unsigned>(chunk.offset));

          if (!UpdateAndSendTransferParameters(kRetransmit).ok()) {
            return;
          }
          PW_LOG_DEBUG("Transfer %u waiting for offset %u, ignoring %u",
                       static_cast<unsigned>(transfer_id_),
                       static_cast<unsigned>(offset_),
                       static_cast<unsigned>(chunk.offset));
        }

        last_chunk_offset_ = chunk.offset;
        SetTimeout(chunk_timeout_);
        return;
      }

      PW_LOG_DEBUG("Transfer %u received expected offset %u, resuming transfer",
                   static_cast<unsigned>(transfer_id_),
                   static_cast<unsigned>(offset_));
      set_transfer_state(TransferState::kWaiting);
      break;

    case TransferState::kWaiting:
      break;
  }

  if (chunk.data.size() > pending_bytes_) {
    // End the transfer, as this indicates a bug with the client implementation
    // where it doesn't respect pending_bytes. Trying to recover from here
    // could potentially result in an infinite transfer loop.
    PW_LOG_ERROR(
        "Received more data than what was requested; terminating transfer.");
    FinishAndSendStatus(Status::Internal());
    return;
  }

  if (chunk.offset != offset_) {
    // Bad offset; reset pending_bytes to send another parameters chunk.
    PW_LOG_DEBUG(
        "Transfer %u expected offset %u, received %u; entering recovery state",
        static_cast<unsigned>(transfer_id_),
        static_cast<unsigned>(offset_),
        static_cast<unsigned>(chunk.offset));
    UpdateAndSendTransferParameters(kRetransmit);
    set_transfer_state(TransferState::kRecovery);

    SetTimeout(chunk_timeout_);
    return;
  }

  // Update the last offset seen so that retries can be detected.
  last_chunk_offset_ = chunk.offset;

  // Write staged data from the buffer to the stream.
  if (!chunk.data.empty()) {
    if (Status status = writer().Write(chunk.data); !status.ok()) {
      PW_LOG_ERROR(
          "Transfer %u write of %u B chunk failed with status %u; aborting "
          "with DATA_LOSS",
          static_cast<unsigned>(transfer_id_),
          static_cast<unsigned>(chunk.data.size()),
          status.code());
      FinishAndSendStatus(Status::DataLoss());
      return;
    }
  }

  // When the client sets remaining_bytes to 0, it indicates completion of the
  // transfer. Acknowledge the completion through a status chunk and clean up.
  if (chunk.IsFinalTransmitChunk()) {
    FinishAndSendStatus(OkStatus());
    return;
  }

  // Update the transfer state.
  offset_ += chunk.data.size();
  pending_bytes_ -= chunk.data.size();

  SetTimeout(chunk_timeout_);

  if (pending_bytes_ == 0u) {
    // Received all pending data. Advance the transfer parameters.
    UpdateAndSendTransferParameters(kRetransmit).IgnoreError();
    return;
  }

  // Once the transmitter has sent a sufficient amount of data, try to extend
  // the window to allow it to continue sending data without blocking.
  uint32_t remaining_window_size = window_end_offset_ - offset_;
  bool extend_window = remaining_window_size <=
                       window_size_ / TransferParameters::kExtendWindowDivisor;

  if (extend_window) {
    UpdateAndSendTransferParameters(kExtend).IgnoreError();
    return;
  }
}

void Context::SendStatusChunk(Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id_;
  chunk.status = status.code();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, thread_->encode_buffer());

  if (!result.ok()) {
    PW_LOG_ERROR("Failed to encode final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id_));
    return;
  }

  if (!rpc_writer_->Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(transfer_id_));
    return;
  }
}

Status Context::Finish(Status status) {
  ClearTimeout();

  PW_LOG_INFO("Transfer %u completed with status %u",
              static_cast<unsigned>(transfer_id_),
              status.code());

  status.Update(DoFinish(status));

  set_transfer_state(TransferState::kCompleted);
  SetTimeout(kFinalChunkAckTimeout);
  status_ = status;

  return status;
}

void Context::SetTimeout(chrono::SystemClock::duration timeout) {
  next_timeout_ = chrono::SystemClock::TimePointAfterAtLeast(timeout);
}

void Context::HandleTimeout() {
  ClearTimeout();

  Status status;

  switch (transfer_state_) {
    case TransferState::kCompleted:
      // A timeout occurring in a completed state indicates that the other side
      // never ACKed the final status packet. Reset the context to inactive.
      set_transfer_state(TransferState::kInactive);
      break;

    case TransferState::kTransmitting:
      // A timeout occurring in a TRANSMITTING state indicates that the transfer
      // has waited for its inter-chunk delay and should transmit its next
      // chunk.
      status = TransmitNextChunk();
      break;

    case TransferState::kWaiting:
      // A timeout occurring in a WAITING state indicates that no chunk has been
      // received from the other side. The transfer should retry its previous
      // operation.
      status = Retry();
      if (status.ok()) {
        SetTimeout(chunk_timeout_);
      }
      break;

    case TransferState::kInactive:
    case TransferState::kRecovery:
      PW_LOG_ERROR("Timeout occurred in bad state %d",
                   static_cast<int>(transfer_state_));
      return;
  }

  if (!status.ok()) {
    FinishAndSendStatus(status);
  }
}

Status Context::Retry() {
  if (retries_ == max_retries_) {
    PW_LOG_ERROR("Transfer %u failed to receive a chunk after %u retries.",
                 static_cast<unsigned>(transfer_id_),
                 static_cast<unsigned>(retries_));
    PW_LOG_ERROR("Canceling transfer.");
    return Status::DeadlineExceeded();
  }

  ++retries_;

  if (type() == TransferType::kReceive) {
    // Resend the most recent transfer parameters.
    PW_LOG_DEBUG(
        "Receive transfer %u timed out waiting for chunk; resending parameters",
        static_cast<unsigned>(transfer_id_));

    return SendTransferParameters(kRetransmit);
  }

  // In a transmit, if a data chunk has not yet been sent, the initial transfer
  // parameters did not arrive from the receiver. Resend the initial chunk.
  if ((flags_ & kFlagsDataSent) != kFlagsDataSent) {
    PW_LOG_DEBUG(
        "Transmit transfer %u timed out waiting for initial parameters",
        static_cast<unsigned>(transfer_id_));
    return SendInitialTransmitChunk();
  }

  // Otherwise, resend the most recent chunk. If the reader doesn't support
  // seeking, this isn't possible, so just terminate the transfer immediately.
  if (!reader().Seek(last_chunk_offset_).ok()) {
    PW_LOG_ERROR("Transmit transfer %d timed out waiting for new parameters.",
                 static_cast<unsigned>(transfer_id_));
    PW_LOG_ERROR("Retrying requires a seekable reader. Alas, ours is not.");
    return Status::DeadlineExceeded();
  }

  // Rewind the transfer position and resend the chunk.
  size_t last_size_sent = offset_ - last_chunk_offset_;
  offset_ = last_chunk_offset_;
  pending_bytes_ += last_size_sent;

  return TransmitNextChunk();
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
