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

#include <variant>

#include "pw_assert/assert.h"
#include "pw_function/function.h"
#include "pw_rpc/raw/client_reader_writer.h"
#include "pw_stream/stream.h"
#include "pw_transfer/internal/context.h"

namespace pw::transfer {

class Client;

namespace internal {

class ClientContext : public Context {
 public:
  ClientContext()
      : internal::Context(OnCompletion),
        client_(nullptr),
        on_completion_(nullptr) {}

  constexpr bool is_read_transfer() const { return type() == kReceive; }
  constexpr bool is_write_transfer() const { return type() == kTransmit; }

  Client& client() {
    PW_DASSERT(active());
    return *client_;
  }

  void StartRead(Client& client,
                 uint32_t transfer_id,
                 work_queue::WorkQueue& work_queue,
                 stream::Writer& writer,
                 rpc::RawClientReaderWriter& stream,
                 Function<void(Status)>&& on_completion,
                 chrono::SystemClock::duration chunk_timeout);

  void StartWrite(Client& client,
                  uint32_t transfer_id,
                  work_queue::WorkQueue& work_queue,
                  stream::Reader& reader,
                  rpc::RawClientReaderWriter& stream,
                  Function<void(Status)>&& on_completion,
                  chrono::SystemClock::duration chunk_timeout);

  void Finish(Status status) {
    PW_DASSERT(active());
    set_transfer_state(TransferState::kCompleted);
    if (on_completion_ != nullptr) {
      on_completion_(status);
    }
    client_ = nullptr;
  }

 private:
  static Status OnCompletion(Context& ctx, Status status) {
    static_cast<ClientContext&>(ctx).Finish(status);
    return OkStatus();
  }

  Client* client_;
  Function<void(Status)> on_completion_;
};

}  // namespace internal
}  // namespace pw::transfer
