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

#include "pw_transfer/internal/client_context.h"

#include "pw_assert/check.h"

namespace pw::transfer::internal {

void ClientContext::StartRead(Client& client,
                              uint32_t transfer_id,
                              work_queue::WorkQueue& work_queue,
                              stream::Writer& writer,
                              rpc::RawClientReaderWriter& stream,
                              Function<void(Status)>&& on_completion,
                              chrono::SystemClock::duration timeout) {
  PW_DCHECK(!active());
  PW_DCHECK(on_completion != nullptr);

  client_ = &client;
  on_completion_ = std::move(on_completion);

  InitializeForReceive(transfer_id, work_queue, stream, writer, timeout);
}

void ClientContext::StartWrite(Client& client,
                               uint32_t transfer_id,
                               work_queue::WorkQueue& work_queue,
                               stream::Reader& reader,
                               rpc::RawClientReaderWriter& stream,
                               Function<void(Status)>&& on_completion,
                               chrono::SystemClock::duration timeout) {
  PW_DCHECK(!active());
  PW_DCHECK(on_completion != nullptr);

  client_ = &client;
  on_completion_ = std::move(on_completion);

  InitializeForTransmit(transfer_id, work_queue, stream, reader, timeout);
}

}  // namespace pw::transfer::internal
