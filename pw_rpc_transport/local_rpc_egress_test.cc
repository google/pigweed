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

#include "pw_rpc_transport/local_rpc_egress.h"

#include "pw_chrono/system_clock.h"
#include "pw_log/log.h"
#include "pw_rpc/client_server.h"
#include "pw_rpc/packet_meta.h"
#include "pw_rpc_transport/internal/test.rpc.pwpb.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_rpc_transport/service_registry.h"
#include "pw_status/status.h"
#include "pw_sync/counting_semaphore.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

using namespace std::literals::chrono_literals;
using namespace std::literals::string_view_literals;

const auto kTestMessage = "I hope that someone gets my message in a bottle"sv;

class TestEchoService final
    : public pw_rpc_transport::testing::pw_rpc::pwpb::TestService::Service<
          TestEchoService> {
 public:
  Status Echo(
      const pw_rpc_transport::testing::pwpb::EchoMessage::Message& request,
      pw_rpc_transport::testing::pwpb::EchoMessage::Message& response) {
    response.msg = request.msg;
    return OkStatus();
  }
};

// Test service that can be controlled from the test, e.g. the test can tell the
// service when it's OK to proceed. Useful for testing packet queue exhaustion.
class ControlledTestEchoService final
    : public pw_rpc_transport::testing::pw_rpc::pwpb::TestService::Service<
          ControlledTestEchoService> {
 public:
  Status Echo(
      const pw_rpc_transport::testing::pwpb::EchoMessage::Message& request,
      pw_rpc_transport::testing::pwpb::EchoMessage::Message& response) {
    start_.release();
    process_.acquire();
    response.msg = request.msg;
    return OkStatus();
  }

  void Wait() { start_.acquire(); }
  void Proceed() { process_.release(); }

 private:
  sync::ThreadNotification start_;
  sync::ThreadNotification process_;
};

TEST(LocalRpcEgressTest, PacketsGetDeliveredToPacketProcessor) {
  constexpr size_t kMaxPacketSize = 100;
  constexpr size_t kNumRequests = 10;
  // Size the queue so we don't exhaust it (we don't want this test to flake;
  // exhaustion is tested separately).
  constexpr size_t kPacketQueueSize = 2 * kNumRequests;
  constexpr uint32_t kChannelId = 1;

  LocalRpcEgress<kPacketQueueSize, kMaxPacketSize> egress;
  std::array channels = {rpc::Channel::Create<kChannelId>(&egress)};
  ServiceRegistry registry(channels);

  TestEchoService service;
  registry.RegisterService(service);

  egress.set_packet_processor(registry);
  auto egress_thread = thread::Thread(thread::stl::Options(), egress);

  auto client =
      registry
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelId);

  std::vector<rpc::PwpbUnaryReceiver<
      pw_rpc_transport::testing::pwpb::EchoMessage::Message>>
      receivers;

  struct State {
    // Stash the receivers to keep the calls alive.
    std::atomic<uint32_t> successes = 0;
    std::atomic<uint32_t> errors = 0;
    sync::CountingSemaphore sem;
  } state;

  for (size_t i = 0; i < kNumRequests; i++) {
    receivers.push_back(client.Echo(
        {.msg = kTestMessage},
        [&state](const pw_rpc_transport::testing::pwpb::EchoMessage::Message&
                     response,
                 Status status) {
          EXPECT_EQ(status, OkStatus());
          EXPECT_EQ(response.msg, kTestMessage);
          state.successes++;
          state.sem.release();
        },
        [&state](Status) {
          state.errors++;
          state.sem.release();
        }));
  }

  for (size_t i = 0; i < kNumRequests; i++) {
    state.sem.acquire();
  }

  EXPECT_EQ(state.successes.load(), kNumRequests);
  EXPECT_EQ(state.errors.load(), 0u);

  egress.Stop();
  egress_thread.join();
}

TEST(LocalRpcEgressTest, PacketQueueExhausted) {
  constexpr size_t kMaxPacketSize = 100;
  constexpr size_t kPacketQueueSize = 1;
  constexpr uint32_t kChannelId = 1;

  LocalRpcEgress<kPacketQueueSize, kMaxPacketSize> egress;
  std::array channels = {rpc::Channel::Create<kChannelId>(&egress)};
  ServiceRegistry registry(channels);

  ControlledTestEchoService service;
  registry.RegisterService(service);

  egress.set_packet_processor(registry);
  auto egress_thread = thread::Thread(thread::stl::Options(), egress);

  auto client =
      registry
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelId);

  auto receiver = client.Echo({.msg = kTestMessage});
  service.Wait();

  // echo_call is blocked in ServiceRegistry waiting for the Proceed() call.
  // Since there is only one packet queue buffer available at a time, other
  // packets will get rejected with RESOURCE_EXHAUSTED error until the first
  // one is handled.
  EXPECT_EQ(egress.Send({}), Status::ResourceExhausted());
  service.Proceed();

  // Expecting egress to return the packet queue buffer within a reasonable
  // amount of time; currently there is no way to explicitly synchronize on
  // its availability, so we give it few seconds to recover.
  auto deadline = chrono::SystemClock::now() + 5s;
  bool egress_ok = false;
  while (chrono::SystemClock::now() <= deadline) {
    if (egress.Send({}).ok()) {
      egress_ok = true;
      break;
    }
  }

  EXPECT_TRUE(egress_ok);

  egress.Stop();
  egress_thread.join();
}

TEST(LocalRpcEgressTest, NoPacketProcessor) {
  constexpr size_t kPacketQueueSize = 10;
  constexpr size_t kMaxPacketSize = 10;
  LocalRpcEgress<kPacketQueueSize, kMaxPacketSize> egress;
  EXPECT_EQ(egress.Send({}), Status::FailedPrecondition());
}

TEST(LocalRpcEgressTest, PacketTooBig) {
  constexpr size_t kPacketQueueSize = 10;
  constexpr size_t kMaxPacketSize = 10;
  constexpr uint32_t kChannelId = 1;
  LocalRpcEgress<kPacketQueueSize, kMaxPacketSize> egress;

  std::array<std::byte, kMaxPacketSize + 1> packet{};
  std::array channels = {rpc::Channel::Create<kChannelId>(&egress)};
  ServiceRegistry registry(channels);
  egress.set_packet_processor(registry);

  EXPECT_EQ(egress.Send(packet), Status::InvalidArgument());
}

TEST(LocalRpcEgressTest, EgressStopped) {
  constexpr size_t kPacketQueueSize = 10;
  constexpr size_t kMaxPacketSize = 10;
  constexpr uint32_t kChannelId = 1;
  LocalRpcEgress<kPacketQueueSize, kMaxPacketSize> egress;

  std::array channels = {rpc::Channel::Create<kChannelId>(&egress)};
  ServiceRegistry registry(channels);
  egress.set_packet_processor(registry);

  auto egress_thread = thread::Thread(thread::stl::Options(), egress);
  EXPECT_EQ(egress.Send({}), OkStatus());
  egress.Stop();
  EXPECT_EQ(egress.Send({}), Status::FailedPrecondition());

  egress_thread.join();
}

}  // namespace
}  // namespace pw::rpc
