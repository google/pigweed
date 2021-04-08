// Copyright 2020 The Pigweed Authors
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

#include <algorithm>

#include "pw_rpc/channel.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::rpc {

// Wraps an RPC ChannelOutput implementation with a mutex to synchronize its
// acquire and release buffer operations. This can be used to allow a simple
// ChannelOutput implementation to run in multi-threaded contexts. More complex
// implementations may want to roll their own synchronization.
template <typename BaseChannelOutput>
class PW_LOCKABLE("pw::rpc::SynchronizedChannelOutput")
    SynchronizedChannelOutput final : public BaseChannelOutput {
 public:
  template <typename... Args>
  constexpr SynchronizedChannelOutput(sync::Mutex& mutex, Args&&... args)
      : BaseChannelOutput(std::forward<Args>(args)...), mutex_(mutex) {}

  std::span<std::byte> AcquireBuffer() final PW_EXCLUSIVE_LOCK_FUNCTION() {
    mutex_.lock();
    return BaseChannelOutput::AcquireBuffer();
  }

  Status SendAndReleaseBuffer(std::span<const std::byte> buffer) final
      PW_UNLOCK_FUNCTION() {
    Status status = BaseChannelOutput::SendAndReleaseBuffer(buffer);
    mutex_.unlock();
    return status;
  }

 private:
  sync::Mutex& mutex_;
};

}  // namespace pw::rpc
