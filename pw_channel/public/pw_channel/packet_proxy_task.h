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

#include <utility>

#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/task.h"
#include "pw_async2/waker.h"
#include "pw_channel/packet_channel.h"
#include "pw_channel/packet_proxy.h"
#include "pw_containers/inline_async_queue.h"

namespace pw::channel {
namespace internal {

class BasicPacketProxyTask : public async2::Task {
 public:
  ~BasicPacketProxyTask() override { DisconnectFromProxy(); }

 protected:
  BasicPacketProxyTask()
      : async2::Task(PW_ASYNC_TASK_NAME("pw::channel::PacketProxyTask")),
        proxy_(nullptr) {}

  bool IsConnected() const { return proxy().IsConnected(); }

  async2::Poll<> PendProxyCompleted(async2::Context& context) const {
    return proxy().PendCompleted(context);
  }

  void DisconnectFromProxy() { proxy().DisconnectTask(); }

  internal::BasicProxy& proxy() const { return *proxy_; }

 protected:
  constexpr void set_proxy(internal::BasicProxy& proxy) { proxy_ = &proxy; }

 private:
  internal::BasicProxy* proxy_;
};

}  // namespace internal

template <typename Derived>
class PacketProxy;

/// A task that handles packets being read from a reader and written to a
/// writer.
///
/// @tparam Derived concrete task type; must provide a
///    `void HandlePacket(Packet&&)` for processing packets as they're read.
/// @tparam PacketType the type of packet that is read/written
template <typename Derived, typename PacketType>
class PacketProxyTask : public internal::BasicPacketProxyTask {
 public:
  using Packet = PacketType;

  constexpr PacketProxyTask(PacketReader<Packet>& reader,
                            PacketWriter<Packet>& writer,
                            InlineAsyncQueue<Packet>& queue)
      : reader_(reader), writer_(writer), write_queue_(queue) {}

  // Must be called from dispatcher thread!
  constexpr void RequestReset() { reset_requested_ = true; }

  Derived& peer() const { return *peer_; }

 protected:
  void ForwardPacket(Packet&& packet) { write_queue_.push(std::move(packet)); }

 private:
  friend PacketProxy<Derived>;

  using internal::BasicPacketProxyTask::set_proxy;

  constexpr void Initialize(PacketProxy<Derived>& proxy, Derived& peer) {
    set_proxy(proxy);
    peer_ = &peer;
  }

  async2::Poll<> DoPend(async2::Context& context) override;
  async2::Poll<Status> PendWrite(async2::Context& context);
  async2::Poll<Status> PendRead(async2::Context& context);

  PacketReader<Packet>& reader_;
  PacketWriter<Packet>& writer_;
  InlineAsyncQueue<Packet>& write_queue_;
  Derived* peer_ = nullptr;
  async2::Waker waker_;
  bool reset_requested_ = false;
};

// Template method implementations.

template <typename Derived, typename Packet>
async2::Poll<> PacketProxyTask<Derived, Packet>::DoPend(
    async2::Context& context) {
  while (PendProxyCompleted(context).IsPending()) {
    auto poll_write = PendWrite(context);
    if (poll_write.IsReady() && !poll_write->ok()) {
      break;
    }

    auto poll_read = PendRead(context);
    if (poll_read.IsReady() && !poll_read->ok()) {
      break;
    }

    if (poll_write.IsPending() && poll_read.IsPending()) {
      return async2::Pending();
    }
  }
  DisconnectFromProxy();
  return async2::Ready();
}

template <typename Derived, typename Packet>
async2::Poll<Status> PacketProxyTask<Derived, Packet>::PendWrite(
    async2::Context& context) {
  while (true) {
    // Check if the task is still working on the previous write. If this returns
    // `Pending`, it is the writer's responsibility to wake this task.
    PW_TRY_READY(writer_.PendWrite(context));

    // Look for the next packet to write.
    PW_TRY_READY(write_queue_.PendNotEmpty(context));

    // Check if the task can stage the next write. If this returns `Pending`, it
    // is the writer's responsibility to wake this task.
    PW_TRY_READY_ASSIGN(auto ready, writer_.PendReadyToWrite(context, 1));

    if (ready.ok()) {
      ready->Stage(std::move(write_queue_.front()));
      write_queue_.pop();
      // Loop to PendWrite again
    } else {
      return ready.status();
    }
  }
  PW_UNREACHABLE;
}

template <typename Derived, typename Packet>
async2::Poll<Status> PacketProxyTask<Derived, Packet>::PendRead(
    async2::Context& context) {
  // Check if the task has a new packet to read. If this returns `Pending`, it
  // is the reader's responsibility to wake this task.
  PW_TRY_READY_ASSIGN(Result<Packet> result, reader_.PendRead(context));
  if (!result.ok()) {
    return async2::Ready(result.status());
  }

  static_cast<Derived&>(*this).HandlePacket(*std::move(result));

  if (reset_requested_) {
    reset_requested_ = false;
    return async2::Ready(Status::Aborted());
  }
  return async2::Ready(OkStatus());
}

}  // namespace pw::channel
