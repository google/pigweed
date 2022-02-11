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

#include "pw_transfer/transfer_thread.h"

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_transfer/internal/chunk.h"

PW_MODIFY_DIAGNOSTICS_PUSH();
PW_MODIFY_DIAGNOSTIC(ignored, "-Wmissing-field-initializers");

namespace pw::transfer::internal {

void TransferThread::Terminate() {
  next_event_ownership_.acquire();
  next_event_.type = EventType::kTerminate;
  event_notification_.release();
}

void TransferThread::SimulateTimeout(EventType type, uint32_t transfer_id) {
  next_event_ownership_.acquire();

  next_event_.type = type;
  next_event_.chunk = {};
  next_event_.chunk.transfer_id = transfer_id;

  event_notification_.release();

  WaitUntilEventIsProcessed();
}

void TransferThread::Run() {
  // Next event starts freed.
  next_event_ownership_.release();

  while (true) {
    if (event_notification_.try_acquire_until(GetNextTransferTimeout())) {
      if (next_event_.type == EventType::kTerminate) {
        return;
      }

      HandleEvent(next_event_);

      // Finished processing the event. Allow the next_event struct to be
      // overwritten.
      next_event_ownership_.release();
    }

    // Regardless of whether an event was received or not, check for any
    // transfers which have timed out and process them if so.
    for (Context& context : client_transfers_) {
      if (context.timed_out()) {
        context.HandleEvent({.type = EventType::kClientTimeout});
      }
    }
    for (Context& context : server_transfers_) {
      if (context.timed_out()) {
        context.HandleEvent({.type = EventType::kServerTimeout});
      }
    }
  }
}

chrono::SystemClock::time_point TransferThread::GetNextTransferTimeout() const {
  chrono::SystemClock::time_point timeout =
      chrono::SystemClock::TimePointAfterAtLeast(kMaxTimeout);

  for (Context& context : client_transfers_) {
    auto ctx_timeout = context.timeout();
    if (ctx_timeout.has_value() && ctx_timeout.value() < timeout) {
      timeout = ctx_timeout.value();
    }
  }
  for (Context& context : server_transfers_) {
    auto ctx_timeout = context.timeout();
    if (ctx_timeout.has_value() && ctx_timeout.value() < timeout) {
      timeout = ctx_timeout.value();
    }
  }

  return timeout;
}

void TransferThread::StartTransfer(TransferType type,
                                   uint32_t transfer_id,
                                   uint32_t handler_id,
                                   stream::Stream* stream,
                                   const TransferParameters& max_parameters,
                                   Function<void(Status)>&& on_completion,
                                   chrono::SystemClock::duration timeout,
                                   uint8_t max_retries) {
  // Block until the last event has been processed.
  next_event_ownership_.acquire();

  bool is_client_transfer = stream != nullptr;

  next_event_.type = is_client_transfer ? EventType::kNewClientTransfer
                                        : EventType::kNewServerTransfer;
  next_event_.new_transfer = {
      .type = type,
      .transfer_id = transfer_id,
      .handler_id = handler_id,
      .max_parameters = &max_parameters,
      .timeout = timeout,
      .max_retries = max_retries,
      .transfer_thread = this,
  };

  staged_on_completion_ = std::move(on_completion);

  // The transfer is initialized with either a stream (client-side) or a handler
  // (server-side). If no stream is provided, try to find a registered handler
  // with the specified ID.
  if (is_client_transfer) {
    next_event_.new_transfer.stream = stream;
    next_event_.new_transfer.rpc_writer = &static_cast<rpc::Writer&>(
        type == TransferType::kTransmit ? client_write_stream_
                                        : client_read_stream_);
  } else {
    auto handler = std::find_if(handlers_.begin(),
                                handlers_.end(),
                                [&](auto& h) { return h.id() == handler_id; });
    if (handler != handlers_.end()) {
      next_event_.new_transfer.handler = &*handler;
      next_event_.new_transfer.rpc_writer = &static_cast<rpc::Writer&>(
          type == TransferType::kTransmit ? server_read_stream_
                                          : server_write_stream_);
    } else {
      // No handler exists for the transfer: return a NOT_FOUND.
      next_event_.type = EventType::kSendStatusChunk;
      next_event_.send_status_chunk = {
          .transfer_id = transfer_id,
          .status = Status::NotFound().code(),
          .stream = type == TransferType::kTransmit
                        ? TransferStream::kServerRead
                        : TransferStream::kServerWrite,
      };
    }
  }

  event_notification_.release();
}

void TransferThread::ProcessChunk(EventType type, ConstByteSpan chunk) {
  // If this assert is hit, there is a bug in the transfer implementation.
  // Contexts' max_chunk_size_bytes fields should be set based on the size of
  // chunk_buffer_.
  PW_CHECK(chunk.size() <= chunk_buffer_.size(),
           "Transfer received a larger chunk than it can handle.");

  Result<uint32_t> transfer_id = ExtractTransferId(chunk);
  if (!transfer_id.ok()) {
    PW_LOG_ERROR("Received a malformed chunk without a transfer ID");
    return;
  }

  // Block until the last event has been processed.
  next_event_ownership_.acquire();

  std::memcpy(chunk_buffer_.data(), chunk.data(), chunk.size());

  next_event_.type = type;
  next_event_.chunk = {
      .transfer_id = *transfer_id,
      .data = chunk_buffer_.data(),
      .size = chunk.size(),
  };

  event_notification_.release();
}

void TransferThread::SetClientStream(TransferStream type,
                                     rpc::RawClientReaderWriter& stream) {
  // Block until the last event has been processed.
  next_event_ownership_.acquire();

  next_event_.type = EventType::kSetTransferStream;
  next_event_.set_transfer_stream = type;
  staged_client_stream_ = std::move(stream);

  event_notification_.release();
}

void TransferThread::SetServerStream(TransferStream type,
                                     rpc::RawServerReaderWriter& stream) {
  // Block until the last event has been processed.
  next_event_ownership_.acquire();

  next_event_.type = EventType::kSetTransferStream;
  next_event_.set_transfer_stream = type;
  staged_server_stream_ = std::move(stream);

  event_notification_.release();
}

void TransferThread::TransferHandlerEvent(EventType type,
                                          internal::Handler& handler) {
  // Block until the last event has been processed.
  next_event_ownership_.acquire();

  next_event_.type = type;
  if (type == EventType::kAddTransferHandler) {
    next_event_.add_transfer_handler = &handler;
  } else {
    next_event_.remove_transfer_handler = &handler;
  }

  event_notification_.release();
}

void TransferThread::HandleEvent(const internal::Event& event) {
  switch (event.type) {
    case EventType::kSendStatusChunk:
      SendStatusChunk(event.send_status_chunk);
      break;

    case EventType::kSetTransferStream:
      switch (event.set_transfer_stream) {
        case TransferStream::kClientRead:
          client_read_stream_ = std::move(staged_client_stream_);
          break;

        case TransferStream::kClientWrite:
          client_write_stream_ = std::move(staged_client_stream_);
          break;

        case TransferStream::kServerRead:
          server_read_stream_ = std::move(staged_server_stream_);
          break;

        case TransferStream::kServerWrite:
          server_write_stream_ = std::move(staged_server_stream_);
          break;
      }
      return;

    case EventType::kAddTransferHandler:
      handlers_.push_front(*event.add_transfer_handler);
      return;

    case EventType::kRemoveTransferHandler:
      handlers_.remove(*event.remove_transfer_handler);
      return;

    default:
      // Other events are handled by individual transfer contexts.
      break;
  }

  if (Context* ctx = FindContextForEvent(event); ctx != nullptr) {
    if (event.type == EventType::kNewClientTransfer) {
      // TODO(frolv): This is terrible.
      static_cast<ClientContext*>(ctx)->set_on_completion(
          std::move(staged_on_completion_));
    }

    ctx->HandleEvent(event);
  }
}

Context* TransferThread::FindContextForEvent(
    const internal::Event& event) const {
  switch (event.type) {
    case EventType::kNewClientTransfer:
      return FindNewTransfer(client_transfers_, event.new_transfer.transfer_id);
    case EventType::kNewServerTransfer:
      return FindNewTransfer(server_transfers_, event.new_transfer.transfer_id);
    case EventType::kClientChunk:
      return FindActiveTransfer(client_transfers_, event.chunk.transfer_id);
    case EventType::kServerChunk:
      return FindActiveTransfer(server_transfers_, event.chunk.transfer_id);
    case EventType::kClientTimeout:  // Manually triggered client timeout
      return FindActiveTransfer(client_transfers_, event.chunk.transfer_id);
    case EventType::kServerTimeout:  // Manually triggered server timeout
      return FindActiveTransfer(server_transfers_, event.chunk.transfer_id);
    default:
      return nullptr;
  }
}

void TransferThread::SendStatusChunk(
    const internal::SendStatusChunkEvent& event) {
  rpc::Writer& destination = stream_for(event.stream);

  internal::Chunk chunk = {};
  chunk.transfer_id = event.transfer_id;
  chunk.status = event.status;

  Result<ConstByteSpan> result = internal::EncodeChunk(chunk, chunk_buffer_);

  if (!result.ok()) {
    PW_LOG_ERROR("Failed to encode final chunk for transfer %u",
                 static_cast<unsigned>(event.transfer_id));
    return;
  }

  if (!destination.Write(result.value()).ok()) {
    PW_LOG_ERROR("Failed to send final chunk for transfer %u",
                 static_cast<unsigned>(event.transfer_id));
    return;
  }
}

}  // namespace pw::transfer::internal

PW_MODIFY_DIAGNOSTICS_POP();
