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

#include "pw_transfer/client.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_transfer/internal/chunk.h"
#include "pw_transfer/transfer.pwpb.h"

namespace pw::transfer {

Status Client::Read(uint32_t transfer_id,
                    stream::Writer& output,
                    CompletionFunc&& on_completion,
                    chrono::SystemClock::duration timeout) {
  if (on_completion == nullptr) {
    return Status::InvalidArgument();
  }

  if (!read_stream_.active()) {
    read_stream_ =
        client_.Read([this](ConstByteSpan chunk) { OnChunk(chunk, kRead); });
  }

  return StartNewTransfer(
      transfer_id, kRead, output, std::move(on_completion), timeout);
}

Status Client::Write(uint32_t transfer_id,
                     stream::Reader& input,
                     CompletionFunc&& on_completion,
                     chrono::SystemClock::duration timeout) {
  if (on_completion == nullptr) {
    return Status::InvalidArgument();
  }

  if (!write_stream_.active()) {
    write_stream_ =
        client_.Write([this](ConstByteSpan chunk) { OnChunk(chunk, kWrite); });
  }

  return StartNewTransfer(
      transfer_id, kWrite, input, std::move(on_completion), timeout);
}

Status Client::StartNewTransfer(uint32_t transfer_id,
                                Type type,
                                stream::Stream& stream,
                                CompletionFunc&& on_completion,
                                chrono::SystemClock::duration timeout) {
  std::lock_guard lock(transfer_context_mutex_);
  ClientContext* context = nullptr;

  // Check the transfer ID is already being used. If not, find an available
  // transfer slot.
  for (ClientContext& ctx : transfer_contexts_) {
    if (ctx.active()) {
      if (ctx.transfer_id() == transfer_id) {
        return Status::AlreadyExists();
      }
    } else {
      context = &ctx;
    }
  }

  if (context == nullptr) {
    return Status::ResourceExhausted();
  }

  if (type == kWrite) {
    PW_LOG_DEBUG("Starting new write transfer %u",
                 static_cast<unsigned>(transfer_id));
    context->StartWrite(*this,
                        transfer_id,
                        work_queue_,
                        static_cast<stream::Reader&>(stream),
                        write_stream_,
                        std::move(on_completion),
                        timeout);
  } else {
    PW_LOG_DEBUG("Starting new read transfer %u",
                 static_cast<unsigned>(transfer_id));
    context->StartRead(*this,
                       transfer_id,
                       work_queue_,
                       static_cast<stream::Writer&>(stream),
                       read_stream_,
                       std::move(on_completion),
                       timeout);
  }

  return context->InitiateTransfer(max_parameters_);
}

Client::ClientContext* Client::GetActiveTransfer(uint32_t transfer_id) {
  std::lock_guard lock(transfer_context_mutex_);
  auto it =
      std::find_if(transfer_contexts_.begin(),
                   transfer_contexts_.end(),
                   [&transfer_id](ClientContext& c) {
                     return c.initialized() && c.transfer_id() == transfer_id;
                   });

  if (it == transfer_contexts_.end()) {
    return nullptr;
  }

  return it;
}

void Client::OnChunk(ConstByteSpan data, Type type) {
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

  if (type == kRead && !ctx->is_read_transfer()) {
    PW_LOG_ERROR(
        "Received a read chunk for transfer %u, but it is a write transfer",
        static_cast<unsigned>(ctx->transfer_id()));
    ctx->Finish(Status::Internal());
    return;
  }

  if (type == kWrite && !ctx->is_write_transfer()) {
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

  if (ctx->ReadChunkData(chunk_data_buffer_, max_parameters_, chunk)) {
    // TODO(frolv): This should be run from work_queue_.
    ctx->ProcessChunk(chunk_data_buffer_, max_parameters_);
  }

  // TODO(frolv): This silences the compiler. Actually use the work queue.
  static_cast<void>(work_queue_);
}

}  // namespace pw::transfer
