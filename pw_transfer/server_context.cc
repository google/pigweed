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
#include "pw_varint/varint.h"

namespace pw::transfer::internal {

Status ServerContext::Start(Type type, Handler& handler) {
  PW_DCHECK(!active());

  if (type == kRead) {
    PW_TRY(handler.PrepareRead());
  } else {
    PW_TRY(handler.PrepareWrite());
  }

  type_ = type;
  handler_ = &handler;
  set_transfer_id(handler.id());
  set_offset(0);
  set_pending_bytes(0);

  return OkStatus();
}

Status ServerContext::Finish(const Status status) {
  PW_DCHECK(active());

  Handler& handler = *handler_;
  handler_ = nullptr;

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

Result<ServerContext*> ServerContextPool::GetOrStartTransfer(uint32_t id) {
  internal::ServerContext* new_transfer = nullptr;

  // Check if the ID belongs to an active transfer. If not, pick an inactive
  // slot to start a new transfer.
  for (ServerContext& transfer : transfers_) {
    if (transfer.active()) {
      if (transfer.transfer_id() == id) {
        return &transfer;
      }
    } else {
      new_transfer = &transfer;
    }
  }

  if (!new_transfer) {
    return Status::ResourceExhausted();
  }

  // Try to start the new transfer by checking if a handler for it exists.
  auto handler = std::find_if(handlers_.begin(), handlers_.end(), [&](auto& h) {
    return h.id() == id;
  });

  if (handler == handlers_.end()) {
    return Status::NotFound();
  }

  PW_TRY(new_transfer->Start(type_, *handler));
  return new_transfer;
}

}  // namespace pw::transfer::internal
