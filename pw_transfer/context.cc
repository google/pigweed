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

#include "pw_transfer/internal/context.h"

#include "pw_assert/check.h"
#include "pw_status/try.h"

namespace pw::transfer::internal {

Status Context::Start(Type type, Handler& handler) {
  PW_DCHECK(!active());

  if (type == kRead) {
    PW_TRY(handler.PrepareRead());
  } else {
    PW_TRY(handler.PrepareWrite());
  }

  type_ = type;
  handler_ = &handler;
  offset_ = 0;
  pending_bytes_ = 0;

  return OkStatus();
}

void Context::Finish(Status status) {
  PW_DCHECK(active());

  if (type_ == kRead) {
    handler_->FinalizeRead(status);
  } else {
    handler_->FinalizeWrite(status);
  }

  handler_ = nullptr;
}

Result<Context*> ContextPool::GetOrStartTransfer(uint32_t id) {
  internal::Context* new_transfer = nullptr;

  // Check if the ID belongs to an active transfer. If not, pick an inactive
  // slot to start a new transfer.
  for (Context& transfer : transfers_) {
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
