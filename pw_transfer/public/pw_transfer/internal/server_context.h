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
#pragma once

#include "pw_assert/assert.h"
#include "pw_containers/intrusive_list.h"
#include "pw_result/result.h"
#include "pw_rpc/raw/server_reader_writer.h"
#include "pw_transfer/handler.h"
#include "pw_transfer/internal/client_connection.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer::internal {

// Transfer context for use within the transfer service (server-side). Stores a
// pointer to a transfer handler when active to stream the transfer data.
class ServerContext : public Context {
 public:
  ServerContext() : Context(OnCompletion), type_(kRead), handler_(nullptr) {}

  // Begins a new transfer with the specified type and handler. Calls into the
  // handler's Prepare method.
  //
  // Precondition: Context is not already active.
  Status Start(TransferType type,
               Handler& handler,
               work_queue::WorkQueue& work_queue,
               rpc::RawServerReaderWriter& stream,
               chrono::SystemClock::duration timeout,
               uint8_t max_retries);

  // Ends the transfer with the given status, calling the handler's Finalize
  // method. No chunks are sent.
  //
  // Returns DATA_LOSS if the finalize call fails.
  //
  // Precondition: Transfer context is active.
  Status Finish(Status status);

 private:
  static Status OnCompletion(Context& ctx, Status status) {
    return static_cast<ServerContext&>(ctx).Finish(status);
  }

  TransferType type_;
  Handler* handler_;
};

// A fixed-size pool of allocatable transfer contexts.
class ServerContextPool {
 public:
  ServerContextPool(TransferType type,
                    IntrusiveList<internal::Handler>& handlers)
      : type_(type), handlers_(handlers) {}

  // Looks up an active context by ID. If none exists, tries to allocate and
  // start a new context.
  //
  // Errors:
  //
  //   NOT_FOUND - No handler exists for the specified transfer ID.
  //   RESOURCE_EXHAUSTED - Out of transfer context slots.
  //
  Result<ServerContext*> StartTransfer(uint32_t transfer_id,
                                       work_queue::WorkQueue& work_queue,
                                       rpc::RawServerReaderWriter& stream,
                                       chrono::SystemClock::duration timeout,
                                       uint8_t max_retries);

  Result<ServerContext*> GetPendingTransfer(uint32_t transfer_id);

 private:
  // TODO(frolv): Initially, only one transfer at a time is supported. Once that
  // is updated, this should be made configurable.
  static constexpr int kMaxConcurrentTransfers = 1;

  TransferType type_;
  std::array<ServerContext, kMaxConcurrentTransfers> transfers_;
  IntrusiveList<internal::Handler>& handlers_;
};

}  // namespace pw::transfer::internal
