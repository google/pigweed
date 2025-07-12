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

#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_async2/waker.h"
#include "pw_bluetooth_proxy/comms2/direction.h"
#include "pw_bluetooth_proxy/comms2/h4_packet.h"
#include "pw_channel/packet_channel.h"
#include "pw_channel/packet_proxy_task.h"
#include "pw_containers/inline_async_queue.h"

namespace pw::bluetooth::proxy {

// Forward declaration.
class Proxy;

class L2capTask : public pw::channel::PacketProxyTask<L2capTask, H4Packet> {
 private:
  using Base = pw::channel::PacketProxyTask<L2capTask, H4Packet>;

 public:
  using Packet = H4Packet;

  L2capTask(Direction direction,
            channel::PacketReader<H4Packet>& reader,
            channel::PacketWriter<H4Packet>& writer);

  Proxy& proxy() const;
  Direction direction() const { return direction_; }

 private:
  friend Base;

  void HandlePacket(H4Packet&& packet);

  [[nodiscard]] bool MaybeHandlePacket(H4Packet& h4_packet);
  [[nodiscard]] bool HandleHciCommand(H4Packet& h4_packet);
  [[nodiscard]] bool HandleHciEvent(H4Packet& h4_packet);
  [[nodiscard]] bool HandleAclData(H4Packet& h4_packet);

  // TODO: b/430175134 - Determine how to allocate and manage packet queues.
  InlineAsyncQueue<H4Packet, 5> queue_;
  Direction direction_;
};

}  // namespace pw::bluetooth::proxy
