// Copyright 2022 The Pigweed Authors
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

#include <cstddef>
#include <span>

#include "pw_rpc/channel.h"
#include "pw_rpc/client_server.h"
#include "pw_rpc/nanopb/fake_channel_output.h"
#include "pw_rpc/payloads_view.h"
#include "pw_status/status.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/mutex.h"
#include "pw_thread/thread_core.h"

namespace pw::rpc {
namespace internal {

// A channel output implementation that stores and forwards outgoing packets
template <size_t kOutputSize,
          size_t kMaxPackets,
          size_t kPayloadsBufferSizeBytes>
class WatchableChannelOutput final : public ChannelOutput {
 private:
  using Output = NanopbFakeChannelOutput<kMaxPackets, kPayloadsBufferSizeBytes>;
  template <auto kMethod>
  using MethodInfo = internal::MethodInfo<kMethod>;
  template <auto kMethod>
  using Response = typename MethodInfo<kMethod>::Response;
  template <auto kMethod>
  using Request = typename MethodInfo<kMethod>::Request;

 public:
  constexpr WatchableChannelOutput()
      : ChannelOutput("testing::FakeChannelOutput") {}

  size_t MaximumTransmissionUnit() PW_LOCKS_EXCLUDED(mutex_) override {
    std::lock_guard lock(mutex_);
    return output_.MaximumTransmissionUnit();
  }

  Status Send(std::span<const std::byte> buffer)
      PW_LOCKS_EXCLUDED(mutex_) override {
    Status status;
    mutex_.lock();
    status = output_.Send(buffer);
    mutex_.unlock();
    output_semaphore_.release();
    return status;
  }

  // Returns true if should continue waiting for additional output
  bool WaitForOutput() PW_LOCKS_EXCLUDED(mutex_) {
    output_semaphore_.acquire();
    std::lock_guard lock(mutex_);
    return should_wait_;
  }

  void StopWaitingForOutput() PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    should_wait_ = false;
    output_semaphore_.release();
  }

  // Returns true if new packets were available to forward
  bool ForwardNextPacket(ClientServer& client_server)
      PW_LOCKS_EXCLUDED(mutex_) {
    std::array<std::byte, kOutputSize> packet_buffer;
    Result<ConstByteSpan> result = EncodeNextUnsentPacket(packet_buffer);
    if (!result.ok()) {
      return false;
    }
    ++sent_packets_;
    const auto process_result = client_server.ProcessPacket(*result);
    PW_ASSERT(process_result.ok());
    return true;
  }

  template <auto kMethod>
  Response<kMethod> response(uint32_t channel_id, uint32_t index)
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    PW_ASSERT(output_.packets().size() >= index);
    return output_.template responses<kMethod>(channel_id)[index];
  }

  template <auto kMethod>
  Request<kMethod> request(uint32_t channel_id, uint32_t index)
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    PW_ASSERT(output_.packets().size() >= index);
    return output_.template requests<kMethod>(channel_id)[index];
  }

 private:
  Result<ConstByteSpan> EncodeNextUnsentPacket(
      std::array<std::byte, kPayloadsBufferSizeBytes>& packet_buffer)
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    if (output_.packets().size() <= sent_packets_) {
      return Status::NotFound();
    }
    return output_.packets()[sent_packets_].Encode(packet_buffer);
  }
  NanopbFakeChannelOutput<kMaxPackets, kPayloadsBufferSizeBytes> output_
      PW_GUARDED_BY(mutex_);
  sync::BinarySemaphore output_semaphore_;
  sync::Mutex mutex_;
  bool should_wait_ PW_GUARDED_BY(mutex_) = true;
  uint16_t sent_packets_ = 0;
};

}  // namespace internal

// Provides a testing context with a real client and server
// Allow for asynchronous function when used as a ThreadCore, or synchronous
// otherwise.
template <size_t kOutputSize = 128,
          size_t kMaxPackets = 16,
          size_t kPayloadsBufferSizeBytes = 128>
class NanopbClientServerTestContext : public thread::ThreadCore {
 private:
  template <auto kMethod>
  using MethodInfo = internal::MethodInfo<kMethod>;
  template <auto kMethod>
  using Response = typename MethodInfo<kMethod>::Response;
  template <auto kMethod>
  using Request = typename MethodInfo<kMethod>::Request;

 public:
  explicit NanopbClientServerTestContext()
      : channel_(Channel::Create<1>(&channel_output_)),
        client_server_({&channel_, 1}) {
    // Semaphore starts released to allow for termination
    exit_semaphore_.release();
  }

  ~NanopbClientServerTestContext() {
    channel_output_.StopWaitingForOutput();
    exit_semaphore_.acquire();
  }

  const Channel& channel() { return channel_; }
  Client& client() { return client_server_.client(); }
  Server& server() { return client_server_.server(); }

  // Retrieve copy of request indexed by order of occurance
  template <auto kMethod>
  Request<kMethod> request(uint32_t index) {
    return channel_output_.template request<kMethod>(channel_.id(), index);
  }

  // Retrieve copy of resonse indexed by order of occurance
  template <auto kMethod>
  Response<kMethod> response(uint32_t index) {
    return channel_output_.template response<kMethod>(channel_.id(), index);
  }

  // If this class is NOT used as a ThreadCore, this should be called after each
  // rpc call to synchronously forward all queued messages. Otherwise this
  // function can be ignored.
  void ForwardNewPackets() {
    while (channel_output_.ForwardNextPacket(client_server_)) {
    }
  }

 private:
  void Run() override {
    // Acquire semaphore to block exit until released
    exit_semaphore_.acquire();
    while (channel_output_.WaitForOutput()) {
      ForwardNewPackets();
    }
    exit_semaphore_.release();
  }
  internal::
      WatchableChannelOutput<kOutputSize, kMaxPackets, kPayloadsBufferSizeBytes>
          channel_output_;
  Channel channel_;
  ClientServer client_server_;
  sync::BinarySemaphore exit_semaphore_;
};

}  // namespace pw::rpc
