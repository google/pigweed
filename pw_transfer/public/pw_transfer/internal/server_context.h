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
#include "pw_transfer/handler.h"
#include "pw_transfer/internal/client_connection.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer::internal {

struct Chunk;

// Transfer context for use within the transfer service (server-side). Stores a
// pointer to a transfer handler when active to stream the transfer data.
class ServerContext : public Context {
 public:
  constexpr ServerContext()
      : Context(),
        type_(kRead),
        state_(kInactive),
        handler_(nullptr),
        last_client_offset_(0) {}

  // True if the ServerContext has been used for a transfer (it has an ID).
  constexpr bool initialized() const { return state_ != kInactive; }

  // True if the transfer is active.
  constexpr bool active() const { return state_ >= kData; }

  // Begins a new transfer with the specified type and handler. Calls into the
  // handler's Prepare method.
  //
  // Precondition: Context is not already active.
  Status Start(TransferType type, Handler& handler);

  void HandleReadChunk(ClientConnection& client, const Chunk& chunk);

  void HandleWriteChunk(ClientConnection& client, const Chunk& chunk);

  // Ends the transfer with the given status, calling the handler's Finalize
  // method. No chunks are sent.
  //
  // Returns DATA_LOSS if the finalize call fails.
  //
  // Precondition: Transfer context is active.
  Status Finish(Status status);

 private:
  // Sends a chunk and returns status indicating what to do next:
  //
  //    OK - continue
  //    OUT_OF_RANGE - done for now
  //    other errors - abort transfer with this error
  //
  Status SendNextReadChunk(ClientConnection& client);

  void ProcessWriteDataChunk(ClientConnection& client, const Chunk& chunk);

  void SendWriteTransferParameters(ClientConnection& client);

  void FinishAndSendStatus(ClientConnection& client, Status status);

  stream::Reader& reader() const {
    PW_DASSERT(type_ == kRead);
    return handler().reader();
  }

  stream::Writer& writer() const {
    PW_DASSERT(type_ == kWrite);
    return handler().writer();
  }

  constexpr Handler& handler() {
    PW_DASSERT(active());
    return *handler_;
  }

  constexpr const Handler& handler() const {
    PW_DASSERT(active());
    return *handler_;
  }

  TransferType type_;
  enum : uint8_t {
    // This ServerContext has never been used for a transfer. It is available
    // for use for a transfer.
    kInactive,
    // A transfer completed and the final status chunk was sent. The
    // ServerContext is available for use for a new transfer. The transfer uses
    // this state to allow the client to retry its last chunk if the final
    // status chunk from the service was dropped.
    kCompleted,
    // Sending or receiving data for an active transfer.
    kData,
    // Recovering after one or more chunks was dropped in an active transfer.
    kRecovery,
  } state_;

  union {
    Handler* handler_;  // Used when state_ is kData or kRecovery
    Status status_;     // Used when state_ is kCompleted
  };

  // Track the last offset sent so that client-side retries can be detected.
  // TODO(hepler): Refactor to split send and receive transfers. This field is
  //     only needed when receiving.
  size_t last_client_offset_;
};

// A fixed-size pool of allocatable transfer contexts.
class ServerContextPool {
 public:
  constexpr ServerContextPool(TransferType type,
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
  Result<ServerContext*> StartTransfer(uint32_t transfer_id);

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
