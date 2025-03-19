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

#include <mutex>

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

// Wrapper for locked access to L2capChannel. Lock must be held at
// construction already, and will be released on destruct.
class LockedL2capChannel {
 public:
  LockedL2capChannel(L2capChannel& channel,
                     std::unique_lock<sync::Mutex>&& lock)
      : channel_(&channel), lock_(std::move(lock)) {}

  LockedL2capChannel(LockedL2capChannel&& other)
      : channel_(other.channel_), lock_(std::move(other.lock_)) {
    other.channel_ = nullptr;
  }

  LockedL2capChannel& operator=(LockedL2capChannel&& other) {
    lock_ = std::move(other.lock_);
    channel_ = other.channel_;
    other.channel_ = nullptr;
    return *this;
  }
  LockedL2capChannel(const LockedL2capChannel&) = delete;
  LockedL2capChannel& operator=(const LockedL2capChannel&) = delete;

  // Will assert if accessed on moved-from object.
  L2capChannel& channel() {
    PW_ASSERT(channel_);
    return *channel_;
  }

 private:
  L2capChannel* channel_;
  std::unique_lock<sync::Mutex> lock_;
};

}  // namespace pw::bluetooth::proxy
