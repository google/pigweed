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

#include "pw_rpc/internal/client_call.h"

namespace pw::rpc::internal {

void ClientCall::CloseClientCall() {
  // When a client call is closed, for bidirectional and client streaming RPCs,
  // the server may be waiting for client stream messages, so we need to
  // notify the server that the client has requested for completion and no
  // further requests should be expected from the client. For unary and server
  // streaming RPCs, since the client is not sending messages, server does not
  // need to be notified.
  if (has_client_stream() && !client_requested_completion()) {
    RequestCompletionLocked().IgnoreError();
  }
  UnregisterAndMarkClosed();
}

void ClientCall::MoveClientCallFrom(ClientCall& other)
    PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
  WaitUntilReadyForMove(*this, other);
  CloseClientCall();
  MoveFrom(other);
}

void UnaryResponseClientCall::HandleCompleted(
    ConstByteSpan response, Status status) PW_NO_LOCK_SAFETY_ANALYSIS {
  UnregisterAndMarkClosed();

  auto on_completed_local = std::move(on_completed_);
  CallbackStarted();

  // The lock is only released when calling into user code. If the callback is
  // wrapped, this on_completed is an internal function that expects the lock to
  // be held, and releases it before invoking user code.
  if (!hold_lock_while_invoking_callback_with_payload()) {
    rpc_lock().unlock();
  }

  if (on_completed_local) {
    on_completed_local(response, status);
  }

  // This mutex lock could be avoided by making callbacks_executing_ atomic.
  RpcLockGuard lock;
  CallbackFinished();
}

void StreamResponseClientCall::HandleCompleted(Status status) {
  UnregisterAndMarkClosed();
  auto on_completed_local = std::move(on_completed_);
  CallbackStarted();
  rpc_lock().unlock();

  if (on_completed_local) {
    on_completed_local(status);
  }

  // This mutex lock could be avoided by making callbacks_executing_ atomic.
  RpcLockGuard lock;
  CallbackFinished();
}

}  // namespace pw::rpc::internal
