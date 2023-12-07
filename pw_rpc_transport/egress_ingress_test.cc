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

#include "pw_rpc_transport/egress_ingress.h"

#include <random>

#include "public/pw_rpc_transport/rpc_transport.h"
#include "pw_bytes/span.h"
#include "pw_metric/metric.h"
#include "pw_rpc/client_server.h"
#include "pw_rpc/packet_meta.h"
#include "pw_rpc_transport/hdlc_framing.h"
#include "pw_rpc_transport/internal/test.rpc.pwpb.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_rpc_transport/service_registry.h"
#include "pw_rpc_transport/simple_framing.h"
#include "pw_status/status.h"
#include "pw_string/string.h"
#include "pw_sync/thread_notification.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

constexpr size_t kMaxPacketSize = 256;

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

// A transport that stores all received frames so they can be manually retrieved
// by the ingress later.
class TestTransport : public RpcFrameSender {
 public:
  explicit TestTransport(size_t mtu, bool is_faulty = false)
      : mtu_(mtu), is_faulty_(is_faulty) {}

  size_t MaximumTransmissionUnit() const override { return mtu_; }

  Status Send(RpcFrame frame) override {
    if (is_faulty_) {
      return Status::Internal();
    }
    std::copy(
        frame.header.begin(), frame.header.end(), std::back_inserter(buffer_));
    std::copy(frame.payload.begin(),
              frame.payload.end(),
              std::back_inserter(buffer_));
    return OkStatus();
  }

  ByteSpan buffer() { return buffer_; }

 private:
  size_t mtu_;
  bool is_faulty_ = false;
  std::vector<std::byte> buffer_;
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

  void set_registry(ServiceRegistry& registry) { registry_ = &registry; }

 private:
  ServiceRegistry* registry_ = nullptr;
};

TEST(RpcEgressIngressTest, SimpleFramingRoundtrip) {
  constexpr uint32_t kChannelAtoB = 1;
  constexpr size_t kMaxMessageLength = 200;
  constexpr size_t kAtoBMtu = 33;
  constexpr size_t kBtoAMtu = 72;

  TestTransport transport_a_to_b(kAtoBMtu);
  TestTransport transport_b_to_a(kBtoAMtu);

  SimpleRpcEgress<kMaxPacketSize> egress_a_to_b("a->b", transport_a_to_b);
  SimpleRpcEgress<kMaxPacketSize> egress_b_to_a("b->a", transport_b_to_a);

  std::array a_tx_channels = {
      rpc::Channel::Create<kChannelAtoB>(&egress_a_to_b)};
  std::array b_tx_channels = {
      rpc::Channel::Create<kChannelAtoB>(&egress_b_to_a)};

  ServiceRegistry registry_a(a_tx_channels);
  ServiceRegistry registry_b(b_tx_channels);

  TestService test_service;
  registry_b.RegisterService(test_service);

  TestLocalEgress local_egress_a;
  local_egress_a.set_registry(registry_a);

  TestLocalEgress local_egress_b;
  local_egress_b.set_registry(registry_b);

  std::array a_rx_channels = {
      ChannelEgress{kChannelAtoB, local_egress_a},
  };
  std::array b_rx_channels = {
      ChannelEgress{kChannelAtoB, local_egress_b},
  };

  SimpleRpcIngress<kMaxPacketSize> ingress_a(a_rx_channels);
  SimpleRpcIngress<kMaxPacketSize> ingress_b(b_rx_channels);

  auto client =
      registry_a
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelAtoB);

  sync::ThreadNotification receiver1_done;
  sync::ThreadNotification receiver2_done;

  struct ReceiverState {
    InlineString<kMaxMessageLength> message;
    sync::ThreadNotification done;
  };

  ReceiverState receiver1;
  ReceiverState receiver2;
  receiver1.message.append(2 * transport_a_to_b.MaximumTransmissionUnit(), '*');
  receiver2.message.append(2 * transport_b_to_a.MaximumTransmissionUnit(), '>');

  auto call1 = client.Echo(
      {.msg = receiver1.message},
      [&receiver1](
          const pw_rpc_transport::testing::pwpb::EchoMessage::Message& response,
          Status status) {
        EXPECT_EQ(status, OkStatus());
        EXPECT_EQ(response.msg, receiver1.message);
        receiver1.done.release();
      },
      [&receiver1](Status status) {
        EXPECT_EQ(status, OkStatus());
        receiver1.done.release();
      });

  auto call2 = client.Echo(
      {.msg = receiver2.message},
      [&receiver2](
          const pw_rpc_transport::testing::pwpb::EchoMessage::Message& response,
          Status status) {
        EXPECT_EQ(status, OkStatus());
        EXPECT_EQ(response.msg, receiver2.message);
        receiver2.done.release();
      },
      [&receiver2](Status status) {
        EXPECT_EQ(status, OkStatus());
        receiver2.done.release();
      });

  // Calling `ingress_b.ProcessIncomingData` reads all packets from the
  // transport and dispatches them according to the ingress configuration.
  // Dispatching a packet generates a reply message: we then read it back at the
  // sender by calling `ingress_a.ProcessIncomingData`.
  EXPECT_EQ(ingress_b.ProcessIncomingData(transport_a_to_b.buffer()),
            OkStatus());
  EXPECT_EQ(ingress_a.ProcessIncomingData(transport_b_to_a.buffer()),
            OkStatus());

  receiver1.done.acquire();
  receiver2.done.acquire();
}

TEST(RpcEgressIngressTest, HdlcFramingRoundtrip) {
  constexpr uint32_t kChannelAtoB = 1;
  constexpr size_t kMaxMessageLength = 200;
  constexpr size_t kAtoBMtu = 33;
  constexpr size_t kBtoAMtu = 72;

  TestTransport transport_a_to_b(kAtoBMtu);
  TestTransport transport_b_to_a(kBtoAMtu);

  HdlcRpcEgress<kMaxPacketSize> egress_a_to_b("a->b", transport_a_to_b);
  HdlcRpcEgress<kMaxPacketSize> egress_b_to_a("b->a", transport_b_to_a);

  std::array a_tx_channels = {
      rpc::Channel::Create<kChannelAtoB>(&egress_a_to_b)};
  std::array b_tx_channels = {
      rpc::Channel::Create<kChannelAtoB>(&egress_b_to_a)};

  ServiceRegistry registry_a(a_tx_channels);
  ServiceRegistry registry_b(b_tx_channels);

  TestService test_service;
  registry_b.RegisterService(test_service);

  TestLocalEgress local_egress_a;
  local_egress_a.set_registry(registry_a);

  TestLocalEgress local_egress_b;
  local_egress_b.set_registry(registry_b);

  std::array a_rx_channels = {
      ChannelEgress{kChannelAtoB, local_egress_a},
  };
  std::array b_rx_channels = {
      ChannelEgress{kChannelAtoB, local_egress_b},
  };

  HdlcRpcIngress<kMaxPacketSize> ingress_a(a_rx_channels);
  HdlcRpcIngress<kMaxPacketSize> ingress_b(b_rx_channels);

  auto client =
      registry_a
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelAtoB);

  sync::ThreadNotification receiver1_done;
  sync::ThreadNotification receiver2_done;

  struct ReceiverState {
    InlineString<kMaxMessageLength> message;
    sync::ThreadNotification done;
  };

  ReceiverState receiver1;
  ReceiverState receiver2;
  receiver1.message.append(2 * transport_a_to_b.MaximumTransmissionUnit(), '*');
  receiver2.message.append(2 * transport_b_to_a.MaximumTransmissionUnit(), '>');

  auto call1 = client.Echo(
      {.msg = receiver1.message},
      [&receiver1](
          const pw_rpc_transport::testing::pwpb::EchoMessage::Message& response,
          Status status) {
        EXPECT_EQ(status, OkStatus());
        EXPECT_EQ(response.msg, receiver1.message);
        receiver1.done.release();
      },
      [&receiver1](Status status) {
        EXPECT_EQ(status, OkStatus());
        receiver1.done.release();
      });

  auto call2 = client.Echo(
      {.msg = receiver2.message},
      [&receiver2](
          const pw_rpc_transport::testing::pwpb::EchoMessage::Message& response,
          Status status) {
        EXPECT_EQ(status, OkStatus());
        EXPECT_EQ(response.msg, receiver2.message);
        receiver2.done.release();
      },
      [&receiver2](Status status) {
        EXPECT_EQ(status, OkStatus());
        receiver2.done.release();
      });

  // Calling `ingress_b.ProcessIncomingData` reads all packets from the
  // transport and dispatches them according to the ingress configuration.
  // Dispatching a packet generates a reply message: we then read it back at the
  // sender by calling `ingress_a.ProcessIncomingData`.
  EXPECT_EQ(ingress_b.ProcessIncomingData(transport_a_to_b.buffer()),
            OkStatus());
  EXPECT_EQ(ingress_a.ProcessIncomingData(transport_b_to_a.buffer()),
            OkStatus());

  receiver1.done.acquire();
  receiver2.done.acquire();
}

TEST(RpcEgressIngressTest, MalformedRpcPacket) {
  constexpr uint32_t kTestChannel = 1;
  constexpr size_t kMtu = 33;
  std::vector<std::byte> kMalformedPacket = {std::byte{0x42}, std::byte{0x74}};

  TestTransport transport(kMtu);
  SimpleRpcEgress<kMaxPacketSize> egress("test", transport);

  TestLocalEgress local_egress;
  std::array rx_channels = {
      ChannelEgress{kTestChannel, local_egress},
  };

  SimpleRpcIngress<kMaxPacketSize> ingress(rx_channels);

  EXPECT_EQ(egress.Send(kMalformedPacket), OkStatus());
  EXPECT_EQ(ingress.ProcessIncomingData(transport.buffer()), OkStatus());

  EXPECT_EQ(ingress.num_bad_packets(), 1u);
  EXPECT_EQ(ingress.num_overflow_channel_ids(), 0u);
  EXPECT_EQ(ingress.num_missing_egresses(), 0u);
  EXPECT_EQ(ingress.num_egress_errors(), 0u);
}

TEST(RpcEgressIngressTest, ChannelIdOverflow) {
  constexpr uint32_t kInvalidChannelId = 65;
  constexpr size_t kMtu = 128;

  TestTransport transport(kMtu);
  SimpleRpcEgress<kMaxPacketSize> egress("test", transport);

  std::array sender_tx_channels = {
      rpc::Channel::Create<kInvalidChannelId>(&egress)};

  ServiceRegistry registry(sender_tx_channels);
  auto client =
      registry
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kInvalidChannelId);

  SimpleRpcIngress<kMaxPacketSize> ingress;

  auto receiver = client.Echo({.msg = "test"});

  EXPECT_EQ(ingress.ProcessIncomingData(transport.buffer()), OkStatus());

  EXPECT_EQ(ingress.num_bad_packets(), 0u);
  EXPECT_EQ(ingress.num_overflow_channel_ids(), 1u);
  EXPECT_EQ(ingress.num_missing_egresses(), 0u);
  EXPECT_EQ(ingress.num_egress_errors(), 0u);
}

TEST(RpcEgressIngressTest, MissingEgressForIncomingPacket) {
  constexpr uint32_t kChannelA = 22;
  constexpr uint32_t kChannelB = 33;
  constexpr size_t kMtu = 128;

  TestTransport transport(kMtu);
  SimpleRpcEgress<kMaxPacketSize> egress("test", transport);

  std::array sender_tx_channels = {rpc::Channel::Create<kChannelA>(&egress)};

  ServiceRegistry registry(sender_tx_channels);
  auto client =
      registry
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelA);

  std::array ingress_channels = {ChannelEgress(kChannelB, egress)};
  SimpleRpcIngress<kMaxPacketSize> ingress(ingress_channels);

  auto receiver = client.Echo({.msg = "test"});

  EXPECT_EQ(ingress.ProcessIncomingData(transport.buffer()), OkStatus());

  EXPECT_EQ(ingress.num_bad_packets(), 0u);
  EXPECT_EQ(ingress.num_overflow_channel_ids(), 0u);
  EXPECT_EQ(ingress.num_missing_egresses(), 1u);
  EXPECT_EQ(ingress.num_egress_errors(), 0u);
}

TEST(RpcEgressIngressTest, EgressSendFailureForIncomingPacket) {
  constexpr uint32_t kChannelId = 22;
  constexpr size_t kMtu = 128;

  TestTransport good_transport(kMtu, /*is_faulty=*/false);
  TestTransport bad_transport(kMtu, /*is_faulty=*/true);
  SimpleRpcEgress<kMaxPacketSize> good_egress("test", good_transport);
  SimpleRpcEgress<kMaxPacketSize> bad_egress("test", bad_transport);

  std::array sender_tx_channels = {
      rpc::Channel::Create<kChannelId>(&good_egress)};

  ServiceRegistry registry(sender_tx_channels);
  auto client =
      registry
          .CreateClient<pw_rpc_transport::testing::pw_rpc::pwpb::TestService>(
              kChannelId);

  std::array ingress_channels = {ChannelEgress(kChannelId, bad_egress)};
  SimpleRpcIngress<kMaxPacketSize> ingress(ingress_channels);

  auto receiver = client.Echo({.msg = "test"});

  EXPECT_EQ(ingress.ProcessIncomingData(good_transport.buffer()), OkStatus());

  EXPECT_EQ(ingress.num_bad_packets(), 0u);
  EXPECT_EQ(ingress.num_overflow_channel_ids(), 0u);
  EXPECT_EQ(ingress.num_missing_egresses(), 0u);
  EXPECT_EQ(ingress.num_egress_errors(), 1u);
}

}  // namespace
}  // namespace pw::rpc
