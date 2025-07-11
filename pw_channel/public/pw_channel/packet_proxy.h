// Copyright 2025 The Pigweed Authors
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

#include "pw_allocator/chunk_pool.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_assert/assert.h"
#include "pw_async2/dispatcher.h"
#include "pw_channel/internal/basic_proxy.h"

namespace pw::channel {

template <typename PacketTaskType>
class PacketProxy : public internal::BasicProxy {
 public:
  constexpr PacketProxy(Allocator& allocator,
                        PacketTaskType& incoming_task,
                        PacketTaskType& outgoing_task)
      : internal::BasicProxy(allocator),
        incoming_task_(incoming_task),
        outgoing_task_(outgoing_task) {
    incoming_task_.Initialize(*this, outgoing_task_);
    outgoing_task_.Initialize(*this, incoming_task_);
  }

  // Starts running the proxy's tasks on the dispatcher.
  void Run(async2::Dispatcher& dispatcher);

 private:
  PacketTaskType& incoming_task_;
  PacketTaskType& outgoing_task_;
};

// Template method implementations.

template <typename PacketTaskType>
void PacketProxy<PacketTaskType>::Run(async2::Dispatcher& dispatcher) {
  PW_DASSERT(!internal::BasicProxy::IsConnected());

  dispatcher.Post(incoming_task_);
  dispatcher.Post(outgoing_task_);
  internal::BasicProxy::Connect();
}

}  // namespace pw::channel
