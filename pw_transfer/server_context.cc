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
#include "pw_transfer/internal/chunk.h"
#include "pw_transfer/transfer.pwpb.h"
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

Status ServerContext::Start(TransferType type,
                            Handler& handler,
                            work_queue::WorkQueue& work_queue,
                            rpc::RawServerReaderWriter& stream,
                            chrono::SystemClock::duration timeout,
                            uint8_t max_retries) {
  PW_DCHECK(!active());

  PW_LOG_INFO("Starting transfer %u", static_cast<unsigned>(handler.id()));

  if (const Status status = handler.Prepare(type); !status.ok()) {
    PW_LOG_WARN("Transfer %u prepare failed with status %u",
                static_cast<unsigned>(handler.id()),
                status.code());
    return status.IsPermissionDenied() ? status : Status::DataLoss();
  }

  type_ = type;
  handler_ = &handler;

  if (type == kRead) {
    InitializeForTransmit(handler.id(),
                          work_queue,
                          stream,
                          handler.reader(),
                          timeout,
                          max_retries);
  } else {
    InitializeForReceive(handler.id(),
                         work_queue,
                         stream,
                         handler.writer(),
                         timeout,
                         max_retries);
  }

  return OkStatus();
}

Status ServerContext::Finish(const Status status) {
  PW_DCHECK(active());

  Handler& handler = *handler_;
  set_transfer_state(TransferState::kCompleted);

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

Result<ServerContext*> ServerContextPool::StartTransfer(
    uint32_t transfer_id,
    work_queue::WorkQueue& work_queue,
    rpc::RawServerReaderWriter& stream,
    chrono::SystemClock::duration timeout,
    uint8_t max_retries) {
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

  PW_TRY(new_transfer->Start(
      type_, *handler, work_queue, stream, timeout, max_retries));
  return new_transfer;
}

Result<ServerContext*> ServerContextPool::GetPendingTransfer(
    uint32_t transfer_id) {
  auto transfer =
      std::find_if(transfers_.begin(), transfers_.end(), [=](auto& t) {
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
