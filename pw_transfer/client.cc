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

#include "pw_transfer/client.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_transfer_private/chunk.h"

namespace pw::transfer {

Status Client::Read(uint32_t transfer_id,
                    stream::Writer& output,
                    Function<void(Status)> on_completion) {
  if (on_completion == nullptr) {
    return Status::InvalidArgument();
  }

  if (read_stream_.active()) {
    read_stream_ =
        client_.Read([this](ConstByteSpan chunk) { OnReadChunk(chunk); });
  }

  Result<ClientContext*> ctx = NewTransfer(
      transfer_id, output, std::move(on_completion), /*write=*/false);
  if (!ctx.ok()) {
    return ctx.status();
  }

  // Restrict the amount of data that can be sent at once to the size of the
  // user-provided data buffer.
  ctx.value()->set_max_chunk_size_bytes(transfer_data_buffer_.size());

  PW_LOG_DEBUG("Starting new read transfer %u",
               static_cast<unsigned>(transfer_id));
  return OkStatus();
}

Status Client::Write(uint32_t transfer_id,
                     stream::SeekableReader& input,
                     Function<void(Status)> on_completion) {
  if (on_completion == nullptr) {
    return Status::InvalidArgument();
  }

  if (write_stream_.active()) {
    write_stream_ =
        client_.Write([this](ConstByteSpan chunk) { OnWriteChunk(chunk); });
  }

  Result<ClientContext*> ctx =
      NewTransfer(transfer_id, input, std::move(on_completion), /*write=*/true);
  if (!ctx.ok()) {
    return ctx.status();
  }

  PW_LOG_DEBUG("Starting new write transfer %u",
               static_cast<unsigned>(transfer_id));
  return StartWriteTransfer(*ctx.value());
}

Result<Client::ClientContext*> Client::NewTransfer(
    uint32_t transfer_id,
    stream::Stream& stream,
    Function<void(Status)> on_completion,
    bool write) {
  std::lock_guard lock(transfer_context_mutex_);
  ClientContext* context = nullptr;

  // Check the transfer ID is already being used. If not, find an available
  // transfer slot.
  for (ClientContext& ctx : transfer_contexts_) {
    if (ctx.transfer_id() == transfer_id) {
      return Status::FailedPrecondition();
    }

    if (!ctx.active()) {
      context = &ctx;
    }
  }

  if (context == nullptr) {
    return Status::ResourceExhausted();
  }

  if (write) {
    context->StartWrite(*this,
                        transfer_id,
                        static_cast<stream::Reader&>(stream),
                        std::move(on_completion));
  } else {
    context->StartRead(*this,
                       transfer_id,
                       static_cast<stream::Writer&>(stream),
                       std::move(on_completion));
  }

  return context;
}

Client::ClientContext* Client::GetActiveTransfer(uint32_t transfer_id) {
  std::lock_guard lock(transfer_context_mutex_);
  auto it = std::find_if(transfer_contexts_.begin(),
                         transfer_contexts_.end(),
                         [&transfer_id](ClientContext& c) {
                           return c.active() && c.transfer_id() == transfer_id;
                         });

  if (it == transfer_contexts_.end()) {
    return nullptr;
  }

  return it;
}

Status Client::StartReadTransfer(ClientContext& ctx) {
  return SendTransferParameters(ctx);
}

Status Client::StartWriteTransfer(ClientContext& ctx) {
  // Send the initial chunk containing just the transfer ID, indicating that we
  // want to write data.
  internal::Chunk chunk = {};
  chunk.transfer_id = ctx.transfer_id();

  Result<ConstByteSpan> result =
      EncodeChunk(chunk, write_stream_.PayloadBuffer());
  if (!result.ok()) {
    return result.status();
  }

  return write_stream_.Write(result.value());
}

void Client::OnReadChunk(ConstByteSpan data) {
  // Process a read transfer chunk arriving from the server.
  // Server -> client read chunks contain the transfer data.

  internal::Chunk chunk;
  if (Status status = DecodeChunk(data, chunk); !status.ok()) {
    // TODO(frolv): Handle this error case.
    return;
  }

  ClientContext* ctx = GetActiveTransfer(chunk.transfer_id);
  if (ctx == nullptr) {
    // TODO(frolv): Handle this error case.
    return;
  }

  if (!ctx->is_read_transfer()) {
    PW_LOG_ERROR(
        "Received a read chunk for transfer %u, but it is a write transfer",
        static_cast<unsigned>(ctx->transfer_id()));
    ctx->Finish(Status::Internal());
    return;
  }

  if (chunk.status.has_value()) {
    // A status field indicates that the transfer has finished.
    // TODO(frolv): This is invoked from the RPC client thread -- should it be
    // run in the work queue instead?
    ctx->Finish(chunk.status.value());
    return;
  }

  if (chunk.offset != ctx->offset()) {
    ctx->set_pending_bytes(0);
    transfer_data_size_ = 0;
  } else {
    if (chunk.data.size() > transfer_data_buffer_.size()) {
      // This case shouldn't happen, as it would indicate that the server was
      // not compliant with the transfer parameters sent.
      SendStatusChunk(read_stream_, ctx->transfer_id(), Status::Internal());
      ctx->Finish(Status::Internal());
      return;
    }

    // TODO(frolv): This needs to be locked.
    std::memcpy(
        transfer_data_buffer_.data(), chunk.data.data(), chunk.data.size());
    transfer_data_size_ = chunk.data.size();

    if (chunk.remaining_bytes.has_value() && chunk.remaining_bytes == 0) {
      ctx->set_is_last_chunk(true);
    }
  }

  work_queue_.PushWork([&ctx]() { InvokeHandleReadChunk(*ctx); });
}

void Client::HandleReadChunk(ClientContext& ctx) {
  if (transfer_data_size_ > 0) {
    if (Status status = ctx.writer().Write(
            transfer_data_buffer_.first(transfer_data_size_));
        !status.ok()) {
      ctx.Finish(status);
      return;
    }

    ctx.set_pending_bytes(ctx.pending_bytes() - transfer_data_size_);
  }

  if (ctx.is_last_chunk()) {
    // Transfer is complete; send a status acknowledging this.
    SendStatusChunk(read_stream_, ctx.transfer_id(), OkStatus());
    return;
  }

  if (ctx.pending_bytes() == 0) {
    if (Status status = SendTransferParameters(ctx); !status.ok()) {
      (void)status;
      // TODO(frolv): Handle this error case.
    }
    return;
  }
}

void Client::OnWriteChunk(ConstByteSpan data) {
  // Process a write transfer chunk arriving from the server.
  //
  // Server -> client write chunks only contain transfer parameters. Update the
  // parameters on the local transfer context to match the server's, then queue
  // the first data chunk to be sent to the server.

  internal::Chunk chunk;
  if (Status status = DecodeChunk(data, chunk); !status.ok()) {
    // TODO(frolv): Handle this error case.
    return;
  }

  ClientContext* ctx = GetActiveTransfer(chunk.transfer_id);
  if (ctx == nullptr) {
    // TODO(frolv): Handle this error case.
    return;
  }

  if (!ctx->is_write_transfer()) {
    PW_LOG_ERROR(
        "Received a write chunk for transfer %u, but it is a read transfer",
        static_cast<unsigned>(ctx->transfer_id()));
    ctx->Finish(Status::Internal());
    return;
  }

  if (chunk.status.has_value()) {
    // A status field indicates that the transfer has finished.
    // TODO(frolv): This is invoked from the RPC client thread -- should it be
    // run in the work queue instead?
    ctx->Finish(chunk.status.value());
    return;
  }

  // Store the transfer parameters from the chunk into the transfer context.
  // The chunk itself will no longer be available when the handler function is
  // invoked.
  if (chunk.pending_bytes.has_value()) {
    ctx->set_pending_bytes(chunk.pending_bytes.value());
  }

  if (chunk.max_chunk_size_bytes.has_value()) {
    ctx->set_max_chunk_size_bytes(chunk.max_chunk_size_bytes.value());
  }

  if (chunk.offset != ctx->offset()) {
    PW_LOG_DEBUG("Rewinding write transfer %u from offset %u to %u",
                 static_cast<unsigned>(chunk.transfer_id),
                 static_cast<unsigned>(ctx->offset()),
                 static_cast<unsigned>(chunk.offset));
    ctx->set_offset(chunk.offset);
  }

  work_queue_.PushWork([&ctx]() { InvokeHandleWriteChunk(*ctx); });
}

void Client::HandleWriteChunk(ClientContext& ctx) {
  if (ctx.pending_bytes() == 0) {
    // Wait for the next transfer parameters from the server.
    return;
  }

  // TODO(frolv): Some of this code is duplicated from the service and could
  // possibly be shared.
  ByteSpan buffer = write_stream_.PayloadBuffer();

  Chunk::MemoryEncoder encoder(buffer);
  encoder.WriteTransferId(ctx.transfer_id()).IgnoreError();
  encoder.WriteOffset(ctx.offset()).IgnoreError();

  // Reserve space for the data proto field overhead and use the remainder of
  // the buffer for the chunk data.
  size_t reserved_size = encoder.size() + 1 /* data key */ + 5 /* data size */;

  ByteSpan data_buffer = buffer.subspan(reserved_size);
  size_t max_bytes_to_send =
      std::min(ctx.pending_bytes(), ctx.max_chunk_size_bytes());

  if (max_bytes_to_send < data_buffer.size()) {
    data_buffer = data_buffer.first(max_bytes_to_send);
  }

  Result<ByteSpan> data = ctx.reader().Read(data_buffer);
  if (data.status().IsOutOfRange()) {
    // There is no more data to send; the transfer is complete. Set
    // remaining_bytes=0 to convey this to the server.
    encoder.WriteRemainingBytes(0).IgnoreError();
    ctx.set_pending_bytes(0);
  } else if (data.ok()) {
    encoder.WriteData(data.value()).IgnoreError();
    ctx.set_offset(ctx.offset() + data.value().size());
    ctx.set_pending_bytes(ctx.pending_bytes() - data.value().size());
  } else {
    PW_LOG_ERROR("Failed to read from data for write transfer %u: %d",
                 static_cast<unsigned>(ctx.transfer_id()),
                 data.status().code());
    write_stream_.ReleaseBuffer();
    SendStatusChunk(write_stream_, ctx.transfer_id(), data.status());
    ctx.Finish(data.status());
    return;
  }

  if (Status status = write_stream_.Write(encoder); !status.ok()) {
    SendStatusChunk(write_stream_, ctx.transfer_id(), status);
    ctx.Finish(status);
    return;
  }

  if (ctx.pending_bytes() > 0) {
    // Queue the next chunk to be sent.
    work_queue_.PushWork([&ctx]() { InvokeHandleWriteChunk(ctx); });
  }
}

void Client::SendStatusChunk(fake::ClientReaderWriter& stream,
                             uint32_t transfer_id,
                             Status status) {
  internal::Chunk chunk = {};
  chunk.transfer_id = transfer_id;
  chunk.status = status.code();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(chunk, stream.PayloadBuffer());
  if (result.ok()) {
    stream.Write(result.value()).IgnoreError();
  }
}

Status Client::SendTransferParameters(ClientContext& ctx) {
  PW_DCHECK(ctx.active());
  PW_DCHECK(ctx.is_read_transfer());

  // TODO(frolv): For now, receive one chunk at a time. This should be made
  // configurable through a constructor argument (or maybe even per transfer).
  ctx.set_max_chunk_size_bytes(
      ctx.MaxWriteChunkSize(max_chunk_size_bytes_, read_stream_.channel_id()));
  ctx.set_pending_bytes(ctx.max_chunk_size_bytes());

  internal::Chunk parameters = {};
  parameters.transfer_id = ctx.transfer_id();
  parameters.offset = ctx.offset();
  parameters.pending_bytes = ctx.pending_bytes();
  parameters.max_chunk_size_bytes = ctx.max_chunk_size_bytes();

  Result<ConstByteSpan> result =
      internal::EncodeChunk(parameters, read_stream_.PayloadBuffer());
  if (!result.ok()) {
    return result.status();
  }

  return read_stream_.Write(result.value());
}

}  // namespace pw::transfer
