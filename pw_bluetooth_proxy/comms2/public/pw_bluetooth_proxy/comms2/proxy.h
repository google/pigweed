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

#include "pw_allocator/allocator.h"
#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_bluetooth_proxy/comms2/l2cap_task.h"
#include "pw_bytes/span.h"
#include "pw_channel/packet_channel.h"
#include "pw_channel/packet_proxy.h"

namespace pw::bluetooth::proxy {

class Proxy : public channel::PacketProxy<L2capTask> {
 private:
  using Base = channel::PacketProxy<L2capTask>;

 public:
  Proxy(Allocator& allocator, L2capTask& controller_task, L2capTask& host_task)
      : Base(allocator, controller_task, host_task) {}
};

inline Proxy& L2capTask::proxy() const {
  return static_cast<Proxy&>(Base::proxy());
}

}  // namespace pw::bluetooth::proxy
