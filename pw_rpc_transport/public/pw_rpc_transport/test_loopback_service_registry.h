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

#include <queue>

#include "pw_rpc_transport/egress_ingress.h"
#include "pw_rpc_transport/service_registry.h"
#include "pw_work_queue/test_thread.h"
#include "pw_work_queue/work_queue.h"

namespace pw::rpc {

// A transport that loops back all received frames to a given ingress.
class TestLoopbackTransport : public RpcFrameSender {
 public:
  explicit TestLoopbackTransport(size_t mtu) : mtu_(mtu) {
    work_thread_ =
        thread::Thread(work_queue::test::WorkQueueThreadOptions(), work_queue_);
  }

  ~TestLoopbackTransport() override {
    work_queue_.RequestStop();
#if PW_THREAD_JOINING_ENABLED
    work_thread_.join();
#else
    work_thread_.detach();
#endif  // PW_THREAD_JOINING_ENABLED
  }

  size_t MaximumTransmissionUnit() const override { return mtu_; }

  Status Send(RpcFrame frame) override {
    buffer_queue_.emplace();
    std::vector<std::byte>& buffer = buffer_queue_.back();
    std::copy(
        frame.header.begin(), frame.header.end(), std::back_inserter(buffer));
    std::copy(
        frame.payload.begin(), frame.payload.end(), std::back_inserter(buffer));

    // Defer processing frame on ingress to avoid deadlocks.
    return work_queue_.PushWork([this]() {
      ingress_->ProcessIncomingData(buffer_queue_.front()).IgnoreError();
      buffer_queue_.pop();
    });
  }

  void SetIngress(RpcIngressHandler& ingress) { ingress_ = &ingress; }

 private:
  size_t mtu_;
  std::queue<std::vector<std::byte>> buffer_queue_;
  RpcIngressHandler* ingress_ = nullptr;
  thread::Thread work_thread_;
  work_queue::WorkQueueWithBuffer<1> work_queue_;
};

// An egress handler that passes the received RPC packet to the service
// registry.
class TestLocalEgress : public RpcEgressHandler {
 public:
  Status SendRpcPacket(ConstByteSpan packet) override {
    if (!registry_) {
      return Status::FailedPrecondition();
    }
    return registry_->ProcessRpcPacket(packet);
  }

  void SetRegistry(ServiceRegistry& registry) { registry_ = &registry; }

 private:
  ServiceRegistry* registry_ = nullptr;
};

class TestLoopbackServiceRegistry : public ServiceRegistry {
 public:
#if PW_RPC_DYNAMIC_ALLOCATION
  static constexpr int kInitTxChannelCount = 0;
#else
  static constexpr int kInitTxChannelCount = 1;
#endif
  static constexpr int kTestChannelId = 1;
  static constexpr size_t kMtu = 512;
  static constexpr size_t kMaxPacketSize = 256;

  TestLoopbackServiceRegistry() : ServiceRegistry(tx_channels_) {
    PW_ASSERT(
        client_server().client().OpenChannel(kTestChannelId, egress_).ok());
#if PW_RPC_DYNAMIC_ALLOCATION
    PW_ASSERT(
        client_server().server().OpenChannel(kTestChannelId, egress_).ok());
#endif
    transport_.SetIngress(ingress_);
    local_egress_.SetRegistry(*this);
  }

  int channel_id() const { return kTestChannelId; }

 private:
  TestLoopbackTransport transport_{kMtu};
  TestLocalEgress local_egress_;
  SimpleRpcEgress<kMaxPacketSize> egress_{"egress", transport_};
  std::array<Channel, kInitTxChannelCount> tx_channels_;
  std::array<ChannelEgress, 1> rx_channels_ = {
      rpc::ChannelEgress{kTestChannelId, local_egress_}};
  SimpleRpcIngress<kMaxPacketSize> ingress_{rx_channels_};
};

}  // namespace pw::rpc
