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

#include "pw_chrono/system_clock.h"
#include "pw_rpc/client_server.h"
#include "pw_rpc/synchronous_call.h"
#include "pw_rpc_transport/egress_ingress.h"
#include "pw_rpc_transport/internal/test.rpc.pwpb.h"
#include "pw_rpc_transport/local_rpc_egress.h"
#include "pw_rpc_transport/service_registry.h"
#include "pw_rpc_transport/socket_rpc_transport.h"
#include "pw_status/status.h"
#include "pw_string/string.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"
#include "pw_thread_stl/options.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

using namespace std::chrono_literals;

constexpr size_t kMaxTestMessageSize = 1024;
constexpr uint32_t kTestChannelId = 1;

class TestService final
    : public pw_rpc_transport::testing::pw_rpc::pwpb::TestService::Service<
          TestService> {
 public:
  Status Echo(
      const pw_rpc_transport::testing::pwpb::EchoMessage::Message& request,
      pw_rpc_transport::testing::pwpb::EchoMessage::Message& response) {
    response.msg = request.msg;
    return OkStatus();
  }
};

template <size_t kMaxPacketSize, size_t kLocalEgressQueueSize>
struct SocketRpcEndpoint {
  explicit SocketRpcEndpoint(SocketRpcTransport<kMaxPacketSize>& transport)
      : transport(transport),
        rpc_egress("tx", transport),
        tx_channels({rpc::Channel::Create<kTestChannelId>(&rpc_egress)}),
        rx_channels({ChannelEgress{kTestChannelId, local_egress}}),
        rpc_ingress(rx_channels),
        service_registry(tx_channels) {
    local_egress.set_packet_processor(service_registry);
    transport.set_ingress(rpc_ingress);
  }

  LocalRpcEgress<kLocalEgressQueueSize, kMaxPacketSize> local_egress;
  SocketRpcTransport<kMaxPacketSize>& transport;
  SimpleRpcEgress<kMaxPacketSize> rpc_egress;
  std::array<rpc::Channel, 1> tx_channels;
  std::array<ChannelEgress, 1> rx_channels;
  SimpleRpcIngress<kMaxPacketSize> rpc_ingress;
  ServiceRegistry service_registry;
};

TEST(RpcIntegrationTest, SocketTransport) {
  constexpr size_t kMaxPacketSize = 512;
  constexpr size_t kLocalEgressQueueSize = 20;
  constexpr size_t kMessageSize = 50;

  SocketRpcTransport<kMaxPacketSize> a_to_b_transport(
      SocketRpcTransport<kMaxPacketSize>::kAsServer, /*port=*/0);
  auto a = SocketRpcEndpoint<kMaxPacketSize, kLocalEgressQueueSize>(
      a_to_b_transport);
  auto a_local_egress_thread =
      thread::Thread(thread::stl::Options(), a.local_egress);
  auto a_transport_thread = thread::Thread(thread::stl::Options(), a.transport);

  a_to_b_transport.WaitUntilReady();

  SocketRpcTransport<kMaxPacketSize> b_to_a_transport(
      SocketRpcTransport<kMaxPacketSize>::kAsClient,
      "localhost",
      a_to_b_transport.port());

  auto b = SocketRpcEndpoint<kMaxPacketSize, kLocalEgressQueueSize>(
      b_to_a_transport);
  auto b_local_egress_thread =
      thread::Thread(thread::stl::Options(), b.local_egress);
  auto b_transport_thread = thread::Thread(thread::stl::Options(), b.transport);

  TestService b_test_service;
  b.service_registry.RegisterService(b_test_service);
  a_to_b_transport.WaitUntilConnected();
  b_to_a_transport.WaitUntilConnected();

  for (int i = 0; i < 10; ++i) {
    InlineString<kMaxTestMessageSize> test_message;
    test_message.append(kMessageSize, '*');
    auto echo_request = pw_rpc_transport::testing::pwpb::EchoMessage::Message{
        .msg = test_message};
    const auto echo_response = rpc::SynchronousCall<
        pw_rpc_transport::testing::pw_rpc::pwpb::TestService::Echo>(
        a.service_registry.client_server().client(),
        kTestChannelId,
        echo_request);
    EXPECT_EQ(echo_response.status(), OkStatus());
    EXPECT_EQ(echo_response.response().msg, test_message);
  }

  // Shut everything down.
  a.local_egress.Stop();
  b.local_egress.Stop();
  a.transport.Stop();
  b.transport.Stop();

  a_local_egress_thread.join();
  b_local_egress_thread.join();
  a_transport_thread.join();
  b_transport_thread.join();
}

}  // namespace
}  // namespace pw::rpc
