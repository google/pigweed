// Copyright 2023 The Pigweed Authors
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

#include <atomic>
#include <cstddef>

#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_rpc/channel.h"
#include "pw_rpc_transport/internal/packet_buffer_queue.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_status/status.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread_core.h"
#include "rpc_transport.h"

namespace pw::rpc {

namespace internal {
void LogNoRpcServiceRegistryError();
void LogPacketSizeTooLarge(size_t packet_size, size_t max_packet_size);
void LogEgressThreadNotRunningError();
void LogFailedToProcessPacket(Status status);
void LogFailedToAccessPacket(Status status);
void LogNoPacketAvailable(Status status);
}  // namespace internal

// Handles RPC packets destined for the local receiver.
template <size_t kPacketQueueSize, size_t kMaxPacketSize>
class LocalRpcEgress : public RpcEgressHandler,
                       public ChannelOutput,
                       public thread::ThreadCore {
  using PacketBuffer =
      typename internal::PacketBufferQueue<kMaxPacketSize>::PacketBuffer;

 public:
  LocalRpcEgress() : ChannelOutput("RPC local egress") {}
  ~LocalRpcEgress() override { Stop(); }

  // Packet processor cannot be passed as a construction dependency as it would
  // create a circular dependency in the RPC transport configuration.
  void set_packet_processor(RpcPacketProcessor& packet_processor) {
    packet_processor_ = &packet_processor;
  }

  // Adds the packet to the transmit queue. The queue is continuously processed
  // by another thread. Implements RpcEgressHandler.
  Status SendRpcPacket(ConstByteSpan rpc_packet) override;

  // Implements ChannelOutput.
  Status Send(ConstByteSpan buffer) override { return SendRpcPacket(buffer); }

  // Once stopped, LocalRpcEgress will no longer process data and
  // will report errors on SendPacket().
  void Stop() {
    if (stopped_) {
      return;
    }
    stopped_ = true;
    // Unblock the processing thread and let it finish gracefully.
    process_queue_.release();
  }

 private:
  void Run() override;

  sync::ThreadNotification process_queue_;
  RpcPacketProcessor* packet_processor_ = nullptr;
  std::array<PacketBuffer, kPacketQueueSize> packet_storage_;
  internal::PacketBufferQueue<kMaxPacketSize> packet_queue_{packet_storage_};
  internal::PacketBufferQueue<kMaxPacketSize> transmit_queue_ = {};
  std::atomic<bool> stopped_ = false;
};

template <size_t kPacketQueueSize, size_t kMaxPacketSize>
Status LocalRpcEgress<kPacketQueueSize, kMaxPacketSize>::SendRpcPacket(
    ConstByteSpan packet) {
  if (!packet_processor_) {
    internal::LogNoRpcServiceRegistryError();
    return Status::FailedPrecondition();
  }
  if (packet.size() > kMaxPacketSize) {
    internal::LogPacketSizeTooLarge(packet.size(), kMaxPacketSize);
    return Status::InvalidArgument();
  }
  if (stopped_) {
    internal::LogEgressThreadNotRunningError();
    return Status::FailedPrecondition();
  }

  // Grab a free packet from the egress' pool, copy incoming frame and
  // push it the queue for processing.
  auto packet_buffer = packet_queue_.Pop();
  if (!packet_buffer.ok()) {
    internal::LogNoPacketAvailable(packet_buffer.status());
    return packet_buffer.status();
  }

  PW_TRY(packet_buffer.value()->CopyPacket(packet));

  transmit_queue_.Push(**packet_buffer);

  process_queue_.release();

  if (stopped_) {
    internal::LogEgressThreadNotRunningError();
    return Status::DataLoss();
  }

  return OkStatus();
}

template <size_t kPacketQueueSize, size_t kMaxPacketSize>
void LocalRpcEgress<kPacketQueueSize, kMaxPacketSize>::Run() {
  while (!stopped_) {
    // Wait until a client has signaled that there is data in the packet queue.
    process_queue_.acquire();

    while (true) {
      Result<PacketBuffer*> packet_buffer = transmit_queue_.Pop();
      if (!packet_buffer.ok()) {
        break;
      }
      Result<ConstByteSpan> packet = (*packet_buffer)->GetPacket();
      if (packet.ok()) {
        if (const auto status = packet_processor_->ProcessRpcPacket(*packet);
            !status.ok()) {
          internal::LogFailedToProcessPacket(status);
        }
      } else {
        internal::LogFailedToAccessPacket(packet.status());
      }
      packet_queue_.Push(**packet_buffer);
    }
  }
}

}  // namespace pw::rpc
