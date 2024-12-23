// Copyright 2024 The Pigweed Authors
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

#include <cstdint>

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_containers/flat_map.h"

namespace pw::bluetooth::proxy {
namespace {

using containers::FlatMap;

// ########## L2capCocTest

class L2capCocTest : public ProxyHostTest {};

TEST_F(L2capCocTest, CannotCreateChannelWithInvalidArgs) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  // Connection handle too large by 1.
  EXPECT_EQ(BuildCocWithResult(proxy, CocParameters{.handle = 0x0FFF}).status(),
            PW_STATUS_INVALID_ARGUMENT);

  // Local CID invalid (0).
  EXPECT_EQ(BuildCocWithResult(proxy, CocParameters{.local_cid = 0}).status(),
            PW_STATUS_INVALID_ARGUMENT);

  // Remote CID invalid (0).
  EXPECT_EQ(BuildCocWithResult(proxy, CocParameters{.remote_cid = 0}).status(),
            PW_STATUS_INVALID_ARGUMENT);
}

// ########## L2capCocWriteTest

class L2capCocWriteTest : public ProxyHostTest {};

// Size of sdu_length field in first K-frames.
constexpr uint8_t kSduLengthFieldSize = 2;
// Size of a K-Frame over Acl packet with no payload.
constexpr uint8_t kFirstKFrameOverAclMinSize =
    emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    emboss::FirstKFrame::MinSizeInBytes();

TEST_F(L2capCocWriteTest, BasicWrite) {
  struct {
    int sends_called = 0;
    // First four bits 0x0 encode PB & BC flags
    uint16_t handle = 0x0ACB;
    // Length of L2CAP PDU
    uint16_t acl_data_total_length = 0x0009;
    // L2CAP header PDU length field
    uint16_t pdu_length = 0x0005;
    // Random CID
    uint16_t channel_id = 0x1234;
    // Length of L2CAP SDU
    uint16_t sdu_length = 0x0003;
    // L2CAP information payload
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};

    // Built from the preceding values in little endian order (except payload in
    // big endian).
    std::array<uint8_t, 13> expected_hci_packet = {0xCB,
                                                   0x0A,
                                                   0x09,
                                                   0x00,
                                                   0x05,
                                                   0x00,
                                                   0x34,
                                                   0x12,
                                                   0x03,
                                                   0x00,
                                                   0xAB,
                                                   0xCD,
                                                   0xEF};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        ++capture.sends_called;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::ACL_DATA);
        EXPECT_EQ(packet.GetHciSpan().size(),
                  capture.expected_hci_packet.size());
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               capture.expected_hci_packet.begin(),
                               capture.expected_hci_packet.end()));
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto acl,
            MakeEmbossView<emboss::AclDataFrameView>(packet.GetHciSpan()));
        EXPECT_EQ(acl.header().handle().Read(), capture.handle);
        EXPECT_EQ(acl.header().packet_boundary_flag().Read(),
                  emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
        EXPECT_EQ(acl.header().broadcast_flag().Read(),
                  emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
        EXPECT_EQ(acl.data_total_length().Read(),
                  capture.acl_data_total_length);
        emboss::FirstKFrameView kframe = emboss::MakeFirstKFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        EXPECT_EQ(kframe.pdu_length().Read(), capture.pdu_length);
        EXPECT_EQ(kframe.channel_id().Read(), capture.channel_id);
        EXPECT_EQ(kframe.sdu_length().Read(), capture.sdu_length);
        for (size_t i = 0; i < 3; ++i) {
          EXPECT_EQ(kframe.payload()[i].Read(), capture.payload[i]);
        }
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/1,
                              /*br_edr_acl_credits_to_reserve=*/0);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  L2capCoc channel = BuildCoc(proxy,
                              CocParameters{.handle = capture.handle,
                                            .remote_cid = capture.channel_id});
  PW_TEST_EXPECT_OK(
      channel.Write(MultiBufFromSpan(span(capture.payload))).status);
  EXPECT_EQ(capture.sends_called, 1);
}

TEST_F(L2capCocWriteTest, ErrorOnWriteToStoppedChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/1,
                              /*br_edr_acl_credits_to_reserve=*/0);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = 123,
                    .tx_credits = 1,
                    .event_fn = []([[maybe_unused]] L2capChannelEvent event) {
                      FAIL();
                    }});

  channel.Stop();
  EXPECT_EQ(channel.Write(multibuf::MultiBuf{}).status,
            Status::FailedPrecondition());
}

TEST_F(L2capCocWriteTest, WriteExceedingMtuFails) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) { FAIL(); });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/1,
                              /*br_edr_acl_credits_to_reserve=*/0);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  // Payload size exceeds MTU.
  L2capCoc small_mtu_channel = BuildCoc(proxy, CocParameters{.tx_mtu = 1});
  std::array<uint8_t, 24> payload;
  EXPECT_EQ(small_mtu_channel.Write(MultiBufFromSpan(span(payload))).status,
            Status::InvalidArgument());
}

TEST_F(L2capCocWriteTest, MultipleWritesSameChannel) {
  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        ++capture.sends_called;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto acl,
            MakeEmbossView<emboss::AclDataFrameView>(packet.GetHciSpan()));
        emboss::FirstKFrameView kframe = emboss::MakeFirstKFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        for (size_t i = 0; i < 3; ++i) {
          EXPECT_EQ(kframe.payload()[i].Read(), capture.payload[i]);
        }
      });

  uint16_t num_writes = 5;
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/num_writes,
                              /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(
      proxy,
      /*num_credits_to_reserve=*/num_writes));

  L2capCoc channel = BuildCoc(proxy, CocParameters{.tx_credits = num_writes});
  for (int i = 0; i < num_writes; ++i) {
    PW_TEST_EXPECT_OK(
        channel.Write(MultiBufFromSpan(span(capture.payload))).status);
    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, num_writes);
}

TEST_F(L2capCocWriteTest, MultipleWritesMultipleChannels) {
  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        ++capture.sends_called;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto acl,
            MakeEmbossView<emboss::AclDataFrameView>(packet.GetHciSpan()));
        emboss::FirstKFrameView kframe = emboss::MakeFirstKFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        for (size_t i = 0; i < 3; ++i) {
          EXPECT_EQ(kframe.payload()[i].Read(), capture.payload[i]);
        }
      });

  constexpr uint16_t kNumChannels = 5;
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/kNumChannels,
                              /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(
      proxy,
      /*num_credits_to_reserve=*/kNumChannels));

  uint16_t remote_cid = 123;
  std::array<L2capCoc, kNumChannels> channels{
      BuildCoc(proxy, CocParameters{.remote_cid = remote_cid}),
      BuildCoc(
          proxy,
          CocParameters{.remote_cid = static_cast<uint16_t>(remote_cid + 1)}),
      BuildCoc(
          proxy,
          CocParameters{.remote_cid = static_cast<uint16_t>(remote_cid + 2)}),
      BuildCoc(
          proxy,
          CocParameters{.remote_cid = static_cast<uint16_t>(remote_cid + 3)}),
      BuildCoc(
          proxy,
          CocParameters{.remote_cid = static_cast<uint16_t>(remote_cid + 4)}),
  };

  for (int i = 0; i < kNumChannels; ++i) {
    PW_TEST_EXPECT_OK(
        channels[i].Write(MultiBufFromSpan(span(capture.payload))).status);
    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, kNumChannels);
}

// ########## L2capCocReadTest

class L2capCocReadTest : public ProxyHostTest {};

TEST_F(L2capCocReadTest, BasicRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .receive_fn = [&capture](pw::span<uint8_t> payload) {
                      ++capture.sends_called;
                      EXPECT_TRUE(std::equal(payload.begin(),
                                             payload.end(),
                                             capture.expected_payload.begin(),
                                             capture.expected_payload.end()));
                    }});

  std::array<uint8_t,
             kFirstKFrameOverAclMinSize + capture.expected_payload.size()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 capture.expected_payload.size());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize +
                            capture.expected_payload.size());
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(capture.expected_payload.size());
  std::copy(capture.expected_payload.begin(),
            capture.expected_payload.end(),
            hci_arr.begin() + kFirstKFrameOverAclMinSize);

  // Send ACL data packet destined for the CoC we registered.
  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(capture.sends_called, 1);
}

TEST_F(L2capCocReadTest, ChannelHandlesReadWithNullReceiveFn) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) { FAIL(); });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .rx_credits = 1,
                    .event_fn = []([[maybe_unused]] L2capChannelEvent event) {
                      FAIL();
                    }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->payload().SizeInBytes());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(0);

  proxy.HandleH4HciFromController(std::move(h4_packet));
}

TEST_F(L2capCocReadTest, ErrorOnRxToStoppedChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve*/ 0);

  int events_received = 0;
  uint16_t num_invalid_rx = 3;
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .rx_credits = num_invalid_rx,
          .receive_fn =
              []([[maybe_unused]] pw::span<uint8_t> payload) { FAIL(); },
          .event_fn =
              [&events_received](L2capChannelEvent event) {
                ++events_received;
                EXPECT_EQ(event, L2capChannelEvent::kRxWhileStopped);
              }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->payload().SizeInBytes());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(0);

  channel.Stop();
  for (int i = 0; i < num_invalid_rx; ++i) {
    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    proxy.HandleH4HciFromController(std::move(h4_packet));
  }
  EXPECT_EQ(events_received, num_invalid_rx);
}

TEST_F(L2capCocReadTest, TooShortAclPassedToHost) {
  int sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithHci&& packet) {
        ++sends_called;
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .receive_fn = []([[maybe_unused]] pw::span<uint8_t> payload) {
            FAIL();
          }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  // Write size larger than buffer size.
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() + 5);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(sends_called, 1);
}

TEST_F(L2capCocReadTest, ChannelClosedWithErrorIfMtuExceeded) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  constexpr uint16_t kRxMtu = 5;
  int events_received = 0;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .rx_mtu = kRxMtu,
          .receive_fn =
              []([[maybe_unused]] pw::span<uint8_t> payload) { FAIL(); },
          .event_fn =
              [&events_received](L2capChannelEvent event) {
                ++events_received;
                EXPECT_EQ(event, L2capChannelEvent::kRxInvalid);
              }});

  constexpr uint16_t kPayloadSize = kRxMtu + 1;
  std::array<uint8_t, kFirstKFrameOverAclMinSize + kPayloadSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 kPayloadSize);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + kPayloadSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kPayloadSize);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(events_received, 1);
}

TEST_F(L2capCocReadTest, ChannelClosedWithErrorIfMpsExceeded) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  constexpr uint16_t kRxMps = 5;
  int events_received = 0;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .rx_mps = kRxMps,
          .receive_fn =
              []([[maybe_unused]] pw::span<uint8_t> payload) { FAIL(); },
          .event_fn =
              [&events_received](L2capChannelEvent event) {
                ++events_received;
                EXPECT_EQ(event, L2capChannelEvent::kRxInvalid);
              }});

  constexpr uint16_t kPayloadSize = kRxMps + 1;
  std::array<uint8_t, kFirstKFrameOverAclMinSize + kPayloadSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 kPayloadSize);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + kPayloadSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kPayloadSize);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(events_received, 1);
}

TEST_F(L2capCocReadTest, ChannelClosedWithErrorIfPayloadsExceedSduLength) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  int events_received = 0;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .receive_fn =
              []([[maybe_unused]] pw::span<uint8_t> payload) { FAIL(); },
          .event_fn =
              [&events_received](L2capChannelEvent event) {
                ++events_received;
                EXPECT_EQ(event, L2capChannelEvent::kRxInvalid);
              }});

  constexpr uint16_t k1stPayloadSize = 1;
  constexpr uint16_t k2ndPayloadSize = 3;
  ASSERT_GT(k2ndPayloadSize, k1stPayloadSize + 1);
  // Indicate SDU length that does not account for the 2nd payload size.
  constexpr uint16_t kSduLength = k1stPayloadSize + 1;

  std::array<uint8_t,
             kFirstKFrameOverAclMinSize +
                 std::max(k1stPayloadSize, k2ndPayloadSize)>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_1st_segment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(),
                        kFirstKFrameOverAclMinSize + k1stPayloadSize)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 k1stPayloadSize);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + k1stPayloadSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kSduLength);

  proxy.HandleH4HciFromController(std::move(h4_1st_segment));

  // Send 2nd segment.
  acl->data_total_length().Write(emboss::SubsequentKFrame::MinSizeInBytes() +
                                 k2ndPayloadSize);
  kframe.pdu_length().Write(k2ndPayloadSize);
  H4PacketWithHci h4_2nd_segment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(),
                        emboss::AclDataFrame::MinSizeInBytes() +
                            emboss::SubsequentKFrame::MinSizeInBytes() +
                            k2ndPayloadSize)};

  proxy.HandleH4HciFromController(std::move(h4_2nd_segment));

  EXPECT_EQ(events_received, 1);
}

TEST_F(L2capCocReadTest, NoReadOnStoppedChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .receive_fn = []([[maybe_unused]] pw::span<uint8_t> payload) {
            FAIL();
          }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);

  channel.Stop();
  proxy.HandleH4HciFromController(std::move(h4_packet));
}

TEST_F(L2capCocReadTest, NoReadOnSameCidDifferentConnectionHandle) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .local_cid = local_cid,
          .receive_fn = []([[maybe_unused]] pw::span<uint8_t> payload) {
            FAIL();
          }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(444);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);

  proxy.HandleH4HciFromController(std::move(h4_packet));
}

TEST_F(L2capCocReadTest, MultipleReadsSameChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .receive_fn = [&capture](pw::span<uint8_t> payload) {
                      ++capture.sends_called;
                      EXPECT_TRUE(std::equal(payload.begin(),
                                             payload.end(),
                                             capture.payload.begin(),
                                             capture.payload.end()));
                    }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize + capture.payload.size()>
      hci_arr;
  hci_arr.fill(0);

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 capture.payload.size());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(capture.payload.size() + kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(capture.payload.size());

  int num_reads = 10;
  for (int i = 0; i < num_reads; ++i) {
    std::copy(capture.payload.begin(),
              capture.payload.end(),
              hci_arr.begin() + kFirstKFrameOverAclMinSize);

    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    proxy.HandleH4HciFromController(std::move(h4_packet));

    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, num_reads);
}

TEST_F(L2capCocReadTest, MultipleReadsMultipleChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr int kNumChannels = 5;
  uint16_t local_cid = 123;
  uint16_t handle = 456;
  auto receive_fn = [&capture](pw::span<uint8_t> payload) {
    ++capture.sends_called;
    EXPECT_TRUE(std::equal(payload.begin(),
                           payload.end(),
                           capture.payload.begin(),
                           capture.payload.end()));
  };
  std::array<L2capCoc, kNumChannels> channels{
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = local_cid,
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 1),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 2),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 3),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 4),
                             .receive_fn = receive_fn}),
  };

  std::array<uint8_t, kFirstKFrameOverAclMinSize + capture.payload.size()>
      hci_arr;
  hci_arr.fill(0);

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 capture.payload.size());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(capture.payload.size() + kSduLengthFieldSize);
  kframe.sdu_length().Write(capture.payload.size());

  for (int i = 0; i < kNumChannels; ++i) {
    kframe.channel_id().Write(local_cid + i);

    std::copy(capture.payload.begin(),
              capture.payload.end(),
              hci_arr.begin() + kFirstKFrameOverAclMinSize);

    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    proxy.HandleH4HciFromController(std::move(h4_packet));

    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, kNumChannels);
}

TEST_F(L2capCocReadTest, ChannelStoppageDoNotAffectOtherChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr int kNumChannels = 5;
  uint16_t local_cid = 123;
  uint16_t handle = 456;
  auto receive_fn = [&capture](pw::span<uint8_t> payload) {
    ++capture.sends_called;
    EXPECT_TRUE(std::equal(payload.begin(),
                           payload.end(),
                           capture.payload.begin(),
                           capture.payload.end()));
  };
  std::array<L2capCoc, kNumChannels> channels{
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = local_cid,
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 1),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 2),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 3),
                             .receive_fn = receive_fn}),
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .local_cid = static_cast<uint16_t>(local_cid + 4),
                             .receive_fn = receive_fn}),
  };

  // Stop the 2nd and 4th of the 5 channels.
  channels[1].Stop();
  channels[3].Stop();

  std::array<uint8_t, kFirstKFrameOverAclMinSize + capture.payload.size()>
      hci_arr;
  hci_arr.fill(0);

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 capture.payload.size());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(capture.payload.size() + kSduLengthFieldSize);
  kframe.sdu_length().Write(capture.payload.size());

  for (int i = 0; i < kNumChannels; ++i) {
    // Still send packets to the stopped channels, so we can validate that it
    // does not cause issues.
    kframe.channel_id().Write(local_cid + i);

    std::copy(capture.payload.begin(),
              capture.payload.end(),
              hci_arr.begin() + kFirstKFrameOverAclMinSize);

    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    proxy.HandleH4HciFromController(std::move(h4_packet));

    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, kNumChannels - 2);
}

TEST_F(L2capCocReadTest, ReadDropsSduSentOver2Segments) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  int sends_called = 0;
  uint16_t local_cid = 234;
  uint16_t handle = 123;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .receive_fn = [&sends_called](pw::span<uint8_t> payload) {
                      EXPECT_EQ(payload.size(), 0ul);
                      ++sends_called;
                    }});

  // Send L2CAP SDU segmented over 2 frames.
  constexpr uint16_t k1stPayloadSize = 13;
  constexpr uint16_t k2ndPayloadSize = 19;
  constexpr uint16_t kSduLength = k1stPayloadSize + k2ndPayloadSize;

  std::array<uint8_t,
             kFirstKFrameOverAclMinSize +
                 std::max(k1stPayloadSize, k2ndPayloadSize)>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_1st_segment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(),
                        kFirstKFrameOverAclMinSize + k1stPayloadSize)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 k1stPayloadSize);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + k1stPayloadSize);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kSduLength);

  proxy.HandleH4HciFromController(std::move(h4_1st_segment));

  // Send 2nd segment.
  acl->data_total_length().Write(emboss::SubsequentKFrame::MinSizeInBytes() +
                                 k2ndPayloadSize);
  kframe.pdu_length().Write(k2ndPayloadSize);
  H4PacketWithHci h4_2nd_segment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(),
                        emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                            emboss::SubsequentKFrame::MinSizeInBytes() +
                            k2ndPayloadSize)};

  proxy.HandleH4HciFromController(std::move(h4_2nd_segment));

  // Now ensure a non-segmented packet with size 0 payload is read.
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.sdu_length().Write(0);
  H4PacketWithHci h4_packet{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(), kFirstKFrameOverAclMinSize)};

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(sends_called, 1);
}

TEST_F(L2capCocReadTest, ReadDropsSduSentOver4Segments) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  int sends_called = 0;
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .receive_fn =
              [&sends_called]([[maybe_unused]] pw::span<uint8_t> payload) {
                EXPECT_EQ(payload.size(), 0ul);
                ++sends_called;
              }});

  // Send L2CAP SDU segmented over 4 frames with equal PDU length.
  constexpr uint16_t kPduLength = 13;
  ASSERT_GE(kPduLength, kSduLengthFieldSize);
  constexpr uint16_t kSduLength = 4 * kPduLength - kSduLengthFieldSize;

  std::array<uint8_t,
             emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                 emboss::BasicL2capHeader::IntrinsicSizeInBytes() + kPduLength>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_1st_segment{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + kPduLength);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kPduLength);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kSduLength);

  proxy.HandleH4HciFromController(std::move(h4_1st_segment));

  H4PacketWithHci h4_2nd_segment{emboss::H4PacketType::ACL_DATA, hci_arr};
  proxy.HandleH4HciFromController(std::move(h4_2nd_segment));

  H4PacketWithHci h4_3rd_segment{emboss::H4PacketType::ACL_DATA, hci_arr};
  proxy.HandleH4HciFromController(std::move(h4_3rd_segment));

  H4PacketWithHci h4_4th_segment{emboss::H4PacketType::ACL_DATA, hci_arr};
  proxy.HandleH4HciFromController(std::move(h4_4th_segment));

  // Now ensure a non-segmented packet with size 0 payload is read.
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.sdu_length().Write(0);
  H4PacketWithHci h4_packet{
      emboss::H4PacketType::ACL_DATA,
      pw::span(hci_arr.data(), kFirstKFrameOverAclMinSize)};

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(sends_called, 1);
}

TEST_F(L2capCocReadTest, NonCocAclPacketPassesThroughToHost) {
  struct {
    int sends_called = 0;
    uint16_t handle = 123;
    std::array<uint8_t, 3> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        ++capture.sends_called;
        Result<emboss::AclDataFrameWriter> acl =
            MakeEmbossWriter<emboss::AclDataFrameWriter>(packet.GetHciSpan());
        EXPECT_EQ(acl->header().handle().Read(), capture.handle);
        emboss::BFrameView bframe =
            emboss::MakeBFrameView(acl->payload().BackingStorage().data(),
                                   acl->data_total_length().Read());
        for (size_t i = 0; i < capture.expected_payload.size(); ++i) {
          EXPECT_EQ(bframe.payload()[i].Read(), capture.expected_payload[i]);
        }
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  // Acquire unused CoC to validate that doing so does not interfere.
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = capture.handle,
          .receive_fn = []([[maybe_unused]] pw::span<uint8_t> payload) {
            FAIL();
          }});

  std::array<uint8_t,
             emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                 emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
                 capture.expected_payload.size()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(capture.handle);
  acl->data_total_length().Write(
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      capture.expected_payload.size());

  emboss::BFrameWriter bframe = emboss::MakeBFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  bframe.pdu_length().Write(capture.expected_payload.size());
  bframe.channel_id().Write(111);
  std::copy(capture.expected_payload.begin(),
            capture.expected_payload.end(),
            hci_arr.begin() +
                emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                emboss::BasicL2capHeader::IntrinsicSizeInBytes());

  // Send ACL packet that should be forwarded to host.
  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(capture.sends_called, 1);
}

TEST_F(L2capCocReadTest,
       FragmentedPduStopsChannelWithoutDisruptingOtherChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  int events_called = 0;
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .receive_fn =
              []([[maybe_unused]] pw::span<uint8_t> payload) { FAIL(); },
          .event_fn =
              [&events_called](L2capChannelEvent event) {
                EXPECT_EQ(event, L2capChannelEvent::kRxFragmented);
                ++events_called;
              }});

  constexpr uint8_t kPduLength = 19;
  ASSERT_GT(kPduLength, kSduLengthFieldSize);
  constexpr uint8_t kSduLength = kPduLength - kSduLengthFieldSize;

  // Send first ACL fragment. Should stop channel with error.
  std::array<uint8_t, emboss::AclDataFrame::MinSizeInBytes() + kSduLength>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_1st_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span(hci_arr.data(), kFirstKFrameOverAclMinSize)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kPduLength);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kSduLength);

  proxy.HandleH4HciFromController(std::move(h4_1st_fragment));

  // Ensure 2nd fragment is silently discarded.
  acl->header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT);
  acl->data_total_length().Write(kSduLength / 2);
  H4PacketWithHci h4_2nd_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(
          hci_arr.data(),
          emboss::AclDataFrame::MinSizeInBytes() + kSduLength / 2)};
  proxy.HandleH4HciFromController(std::move(h4_2nd_fragment));

  // Ensure 3rd fragment is silently discarded.
  if (kSduLength % 2 == 1) {
    acl->data_total_length().Write(kSduLength / 2 + 1);
  }
  H4PacketWithHci h4_3rd_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(hci_arr.data(),
                        emboss::AclDataFrame::MinSizeInBytes() +
                            kSduLength / 2 + (kSduLength % 2))};
  proxy.HandleH4HciFromController(std::move(h4_3rd_fragment));

  EXPECT_EQ(events_called, 1);

  int reads_called = 0;
  uint16_t different_local_cid = 432;
  L2capCoc different_channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .local_cid = different_local_cid,
          .receive_fn =
              [&reads_called]([[maybe_unused]] pw::span<uint8_t> payload) {
                ++reads_called;
              },
          .event_fn =
              []([[maybe_unused]] L2capChannelEvent event) { FAIL(); }});

  // Ensure different channel can still receive valid payload.
  std::array<uint8_t, kFirstKFrameOverAclMinSize> other_hci_arr;
  other_hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, other_hci_arr};

  Result<emboss::AclDataFrameWriter> other_acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(other_hci_arr);
  other_acl->header().handle().Write(handle);
  other_acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter other_kframe =
      emboss::MakeFirstKFrameView(other_acl->payload().BackingStorage().data(),
                                  other_acl->data_total_length().Read());
  other_kframe.pdu_length().Write(kSduLengthFieldSize);
  other_kframe.channel_id().Write(different_local_cid);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(reads_called, 1);
}

TEST_F(L2capCocReadTest, UnexpectedContinuingFragmentStopsChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  int events_received = 0;
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .receive_fn = [](span<uint8_t>) { FAIL(); },
                    .event_fn =
                        [&events_received](L2capChannelEvent event) {
                          ++events_received;
                          EXPECT_EQ(event, L2capChannelEvent::kRxFragmented);
                        }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());
  acl->header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->payload().SizeInBytes());
  kframe.pdu_length().Write(kSduLengthFieldSize);
  kframe.channel_id().Write(local_cid);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(events_received, 1);
}

TEST_F(L2capCocReadTest, AclFrameWithIncompleteL2capHeaderForwardedToHost) {
  int sends_to_host_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&sends_to_host_called]([[maybe_unused]] H4PacketWithHci&& packet) {
        ++sends_to_host_called;
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 123;
  L2capCoc channel = BuildCoc(proxy, CocParameters{.handle = handle});

  std::array<uint8_t, emboss::AclDataFrameHeader::IntrinsicSizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(0);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(sends_to_host_called, 1);
}

TEST_F(L2capCocReadTest, FragmentedPduDoesNotInterfereWithOtherChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle_frag = 0x123, handle_fine = 0x234;
  uint16_t cid_frag = 0x345, cid_fine = 0x456;
  int packets_received = 0;
  auto receive_fn =
      [&packets_received]([[maybe_unused]] pw::span<uint8_t> payload) {
        ++packets_received;
      };
  L2capCoc frag_channel = BuildCoc(proxy,
                                   CocParameters{
                                       .handle = handle_frag,
                                       .local_cid = cid_frag,
                                       .receive_fn = receive_fn,
                                   });
  L2capCoc fine_channel = BuildCoc(proxy,
                                   CocParameters{
                                       .handle = handle_fine,
                                       .local_cid = cid_fine,
                                       .receive_fn = receive_fn,
                                   });

  // Order of receptions:
  // 1. 1st of 3 fragments to frag_channel.
  // 2. Non-fragmented PDU to fine_channel.
  // 3. 2nd of 3 fragments to frag_channel.
  // 4. Non-fragmented PDU to fine_channel.
  // 5. 3rd of 3 fragments to frag_channel.
  // 6. Non-fragmented PDU to fine_channel.

  constexpr uint8_t kPduLength = 14;
  ASSERT_GT(kPduLength, kSduLengthFieldSize);
  constexpr uint8_t kSduLength = kPduLength - kSduLengthFieldSize;

  // 1. 1st of 3 fragments to frag_channel.
  std::array<uint8_t, emboss::AclDataFrame::MinSizeInBytes() + kSduLength>
      frag_hci_arr;
  frag_hci_arr.fill(0);
  H4PacketWithHci h4_1st_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span(frag_hci_arr.data(), kFirstKFrameOverAclMinSize)};

  Result<emboss::AclDataFrameWriter> acl_frag =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(frag_hci_arr);
  acl_frag->header().handle().Write(handle_frag);
  acl_frag->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe_frag =
      emboss::MakeFirstKFrameView(acl_frag->payload().BackingStorage().data(),
                                  acl_frag->data_total_length().Read());
  kframe_frag.pdu_length().Write(kPduLength);
  kframe_frag.channel_id().Write(cid_frag);
  kframe_frag.sdu_length().Write(kSduLength);

  proxy.HandleH4HciFromController(std::move(h4_1st_fragment));

  // 2. Non-fragmented PDU to fine_channel.
  std::array<uint8_t, kFirstKFrameOverAclMinSize> fine_hci_arr;
  fine_hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA,
                            pw::span(fine_hci_arr)};

  Result<emboss::AclDataFrameWriter> acl_fine =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(fine_hci_arr);
  acl_fine->header().handle().Write(handle_fine);
  acl_fine->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes());

  emboss::FirstKFrameWriter kframe_fine =
      emboss::MakeFirstKFrameView(acl_fine->payload().BackingStorage().data(),
                                  acl_fine->data_total_length().Read());
  kframe_fine.pdu_length().Write(kSduLengthFieldSize);
  kframe_fine.channel_id().Write(cid_fine);
  kframe_fine.sdu_length().Write(0);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // 3. 2nd of 3 fragments to frag_channel.
  acl_frag->header().packet_boundary_flag().Write(
      emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT);
  acl_frag->data_total_length().Write(kSduLength / 2);
  H4PacketWithHci h4_2nd_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(
          frag_hci_arr.data(),
          emboss::AclDataFrame::MinSizeInBytes() + kSduLength / 2)};
  proxy.HandleH4HciFromController(std::move(h4_2nd_fragment));

  // 4. Non-fragmented PDU to fine_channel.
  H4PacketWithHci h4_packet_2{emboss::H4PacketType::ACL_DATA,
                              pw::span(fine_hci_arr)};
  proxy.HandleH4HciFromController(std::move(h4_packet_2));

  // 5. 3rd of 3 fragments to frag_channel.
  if (kSduLength % 2 == 1) {
    acl_frag->data_total_length().Write(kSduLength / 2 + 1);
  }
  H4PacketWithHci h4_3rd_fragment{
      emboss::H4PacketType::ACL_DATA,
      pw::span<uint8_t>(frag_hci_arr.data(),
                        emboss::AclDataFrame::MinSizeInBytes() +
                            kSduLength / 2 + (kSduLength % 2))};
  proxy.HandleH4HciFromController(std::move(h4_3rd_fragment));

  // 6. Non-fragmented PDU to fine_channel.
  H4PacketWithHci h4_packet_3{emboss::H4PacketType::ACL_DATA,
                              pw::span(fine_hci_arr)};
  proxy.HandleH4HciFromController(std::move(h4_packet_3));

  EXPECT_EQ(packets_received, 3);
}

// ########## L2capCocQueueTest

class L2capCocQueueTest : public ProxyHostTest {};

TEST_F(L2capCocQueueTest, ReadBufferResponseDrainsQueue) {
  size_t sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn),
                std::move(send_to_controller_fn),
                /*le_acl_credits_to_reserve=*/L2capCoc::QueueCapacity(),
                /*br_edr_acl_credits_to_reserve=*/0);

  L2capCoc channel =
      BuildCoc(proxy, CocParameters{.tx_credits = L2capCoc::QueueCapacity()});

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    PW_TEST_EXPECT_OK(channel.Write(multibuf::MultiBuf{}).status);
  }
  EXPECT_EQ(channel.Write(multibuf::MultiBuf{}).status, PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(sends_called, 0u);

  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));

  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());
}

TEST_F(L2capCocQueueTest, NocpEventDrainsQueue) {
  size_t sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn),
                std::move(send_to_controller_fn),
                /*le_acl_credits_to_reserve=*/L2capCoc::QueueCapacity(),
                /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));

  uint16_t handle = 123;
  L2capCoc channel =
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .tx_credits = 2 * L2capCoc::QueueCapacity()});

  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    PW_TEST_EXPECT_OK(channel.Write(multibuf::MultiBuf{}).status);
  }

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    PW_TEST_EXPECT_OK(channel.Write(multibuf::MultiBuf{}).status);
  }
  EXPECT_EQ(channel.Write(multibuf::MultiBuf{}).status, PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());

  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{handle, L2capCoc::QueueCapacity()}}})));

  EXPECT_EQ(sends_called, 2 * L2capCoc::QueueCapacity());
}

TEST_F(L2capCocQueueTest, RemovingLrdChannelDoesNotInvalidateRoundRobin) {
  size_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn),
                std::move(send_to_controller_fn),
                /*le_acl_credits_to_reserve=*/L2capCoc::QueueCapacity(),
                /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), L2capCoc::QueueCapacity());

  uint16_t handle = 123;
  std::array<uint16_t, 3> remote_cids = {1, 2, 3};
  L2capCoc chan_left = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle, .remote_cid = remote_cids[0], .tx_credits = 1});
  std::optional<L2capCoc> chan_middle =
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .remote_cid = remote_cids[1],
                             .tx_credits = L2capCoc::QueueCapacity() + 1});
  L2capCoc chan_right = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle, .remote_cid = remote_cids[2], .tx_credits = 1});

  // We have 3 channels. Make it so LRD channel iterator is on the middle
  // channel, then release that channel and ensure the other two are still
  // reached in the round robin.

  // Queue a packet in middle channel.
  for (size_t i = 0; i < L2capCoc::QueueCapacity() + 1; ++i) {
    PW_TEST_EXPECT_OK(chan_middle->Write(multibuf::MultiBuf{}).status);
  }
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());

  // Make middle channel the LRD channel.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{handle, 1}}})));
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 1);

  // Queue a packet each in left and right channels.
  PW_TEST_EXPECT_OK(chan_left.Write(multibuf::MultiBuf{}).status);
  PW_TEST_EXPECT_OK(chan_right.Write(multibuf::MultiBuf{}).status);
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 1);

  // Drop middle channel. LRD write iterator should still be valid.
  chan_middle.reset();

  // Confirm packets in remaining two channels are sent in round robin.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{handle, 2}}})));
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 3);
}

TEST_F(L2capCocQueueTest, H4BufferReleaseTriggersQueueDrain) {
  constexpr size_t kNumSends =
      ProxyHost::GetNumSimultaneousAclSendsSupported() + 1;

  struct {
    size_t sends_called = 0;
    Vector<H4PacketWithH4, kNumSends> packet_store;
  } capture;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        ++capture.sends_called;
        capture.packet_store.push_back(std::move(packet));
      });
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/kNumSends,
                              /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, kNumSends));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), kNumSends);

  constexpr uint16_t kHandle = 0x123;
  constexpr uint16_t kRemoteCid = 0x456;
  L2capCoc channel = BuildCoc(proxy,
                              CocParameters{.handle = kHandle,
                                            .remote_cid = kRemoteCid,
                                            .tx_credits = kNumSends});

  // Occupy all buffers. Final Write should queue and not send.
  for (size_t i = 0; i < kNumSends; ++i) {
    PW_TEST_EXPECT_OK(channel.Write(multibuf::MultiBuf{}).status);
  }
  EXPECT_EQ(capture.sends_called, kNumSends - 1);

  // Release a buffer. Queued packet should then send.
  capture.packet_store.pop_back();
  EXPECT_EQ(capture.sends_called, kNumSends);

  capture.packet_store.clear();
}

// ########## L2capCocReassemblyTest

class L2capCocReassemblyTest : public ProxyHostTest {};

TEST_F(L2capCocReassemblyTest, OneSegmentRx) {
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [](H4PacketWithHci&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 0x123;
  uint16_t local_cid = 0x234;
  struct {
    int sdus_received = 0;
    std::array<uint8_t, 10> expected_payload = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  } capture;
  L2capCoc channel = BuildCoc(
      proxy,
      {.handle = handle,
       .local_cid = local_cid,
       .receive_fn_multibuf = [&capture](multibuf::MultiBuf&& payload) {
         ++capture.sdus_received;
         std::optional<ConstByteSpan> rx_sdu = payload.ContiguousSpan();
         ASSERT_TRUE(rx_sdu);
         ConstByteSpan expected_sdu = as_bytes(span(
             capture.expected_payload.data(), capture.expected_payload.size()));
         EXPECT_TRUE(std::equal(rx_sdu->begin(),
                                rx_sdu->end(),
                                expected_sdu.begin(),
                                expected_sdu.end()));
       }});

  Result<BFrameWithStorage> bframe = SetupBFrame(
      handle, local_cid, capture.expected_payload.size() + kSduLengthFieldSize);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA,
                            bframe->acl.hci_span()};

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      bframe->acl.writer.payload().BackingStorage().data(),
      bframe->acl.writer.payload().SizeInBytes());
  kframe.sdu_length().Write(capture.expected_payload.size());
  std::memcpy(/*__dest=*/kframe.payload().BackingStorage().data(),
              /*__src=*/capture.expected_payload.data(),
              /*__n=*/capture.expected_payload.size());

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(capture.sdus_received, 1);
}

TEST_F(L2capCocReassemblyTest, SduReceivedWhenSegmentedOverFullRangeOfMps) {
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [](H4PacketWithHci&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 0x123;
  uint16_t local_cid = 0x234;
  struct {
    uint16_t sdus_received = 0;
    std::array<uint8_t, 19> expected_payload = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
  } capture;
  L2capCoc channel = BuildCoc(
      proxy,
      {.handle = handle,
       .local_cid = local_cid,
       .receive_fn_multibuf = [&capture](multibuf::MultiBuf&& payload) {
         ++capture.sdus_received;
         std::optional<ConstByteSpan> rx_sdu = payload.ContiguousSpan();
         ASSERT_TRUE(rx_sdu);
         ConstByteSpan expected_sdu = as_bytes(span(
             capture.expected_payload.data(), capture.expected_payload.size()));
         EXPECT_TRUE(std::equal(rx_sdu->begin(),
                                rx_sdu->end(),
                                expected_sdu.begin(),
                                expected_sdu.end()));
       }});

  uint16_t sdus_sent = 0;
  // Test sending payload segmented in every possible way, from MPS of 2 octets
  // to MPS values 5 octets greater than the payload size.
  for (uint16_t mps = 2; mps < capture.expected_payload.size() + 5; ++mps) {
    for (size_t segment_no = 0;; ++segment_no) {
      Result<KFrameWithStorage> kframe = SetupKFrame(
          handle, local_cid, mps, segment_no, span(capture.expected_payload));
      if (!kframe.ok()) {
        break;
      }
      H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA,
                                kframe->acl.hci_span()};
      proxy.HandleH4HciFromController(std::move(h4_packet));
    }
    ++sdus_sent;
  }

  EXPECT_EQ(capture.sdus_received, sdus_sent);
}

TEST_F(L2capCocReassemblyTest, ErrorIfPayloadBytesExceedSduLength) {
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [](H4PacketWithHci&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 0x123;
  uint16_t local_cid = 0x234;
  int events_received = 0;
  L2capCoc channel =
      BuildCoc(proxy,
               {.handle = handle,
                .local_cid = local_cid,
                .event_fn =
                    [&events_received](L2capChannelEvent event) {
                      ++events_received;
                      EXPECT_EQ(event, L2capChannelEvent::kRxInvalid);
                    },
                .receive_fn_multibuf = [](multibuf::MultiBuf&&) { FAIL(); }});

  constexpr uint16_t kIndicatedSduLength = 5;
  // First PDU will be 2 bytes for SDU length field + 2 payload bytes
  // Second PDU will have 4 payload bytes, which will exceed SDU length by 1.
  constexpr uint16_t kFirstPayloadLength = 2;

  std::array<uint8_t, kFirstKFrameOverAclMinSize + kFirstPayloadLength> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci first_h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() +
                                 kFirstPayloadLength);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  kframe.pdu_length().Write(kSduLengthFieldSize + kFirstPayloadLength);
  kframe.channel_id().Write(local_cid);
  kframe.sdu_length().Write(kIndicatedSduLength);

  proxy.HandleH4HciFromController(std::move(first_h4_packet));

  H4PacketWithHci second_h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
  proxy.HandleH4HciFromController(std::move(second_h4_packet));

  EXPECT_EQ(events_received, 1);
}

TEST_F(L2capCocReassemblyTest, ErrorIfRxBufferTooSmallForFirstKFrame) {
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [](H4PacketWithHci&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  uint16_t handle = 0x123;
  uint16_t local_cid = 0x234;
  int events_received = 0;
  L2capCoc channel =
      BuildCoc(proxy,
               {.handle = handle,
                .local_cid = local_cid,
                .event_fn =
                    [&events_received](L2capChannelEvent event) {
                      ++events_received;
                      EXPECT_EQ(event, L2capChannelEvent::kRxInvalid);
                    },
                .receive_fn_multibuf = [](multibuf::MultiBuf&&) { FAIL(); }});

  std::array<uint8_t, kFirstKFrameOverAclMinSize - 1> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(emboss::FirstKFrame::MinSizeInBytes() - 1);

  emboss::FirstKFrameWriter kframe = emboss::MakeFirstKFrameView(
      acl->payload().BackingStorage().data(), acl->data_total_length().Read());
  EXPECT_TRUE(!kframe.IsComplete());
  kframe.pdu_length().Write(1);
  kframe.channel_id().Write(local_cid);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(events_received, 1);
}

// ########## L2capCocSegmentation

class L2capCocSegmentationTest : public ProxyHostTest {};

TEST_F(L2capCocSegmentationTest, SduSentWhenSegmentedOverFullRangeOfMps) {
  constexpr size_t kPayloadSize = 312;
  struct {
    uint16_t handle = 0x123;
    uint16_t remote_cid = 0x456;
    uint16_t sdus_received = 0;
    uint16_t mps;
    std::array<uint8_t, kPayloadSize> expected_payload;
  } capture;

  for (size_t i = 0; i < capture.expected_payload.size(); ++i) {
    capture.expected_payload[i] = i % UINT8_MAX;
  }

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture](H4PacketWithH4&& tx_kframe) {
        static uint16_t segment_no = 0;
        static uint16_t pdu_bytes_received = 0;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            KFrameWithStorage expected_kframe,
            SetupKFrame(capture.handle,
                        capture.remote_cid,
                        capture.mps,
                        segment_no,
                        span(capture.expected_payload)));

        EXPECT_TRUE(std::equal(tx_kframe.GetHciSpan().begin(),
                               tx_kframe.GetHciSpan().end(),
                               expected_kframe.acl.hci_span().begin(),
                               expected_kframe.acl.hci_span().end()));

        pdu_bytes_received +=
            expected_kframe.acl.writer.data_total_length().Read() -
            emboss::BasicL2capHeader::IntrinsicSizeInBytes();

        if (pdu_bytes_received ==
            capture.expected_payload.size() + kSduLengthFieldSize) {
          ++capture.sdus_received;
          segment_no = 0;
          pdu_bytes_received = 0;
        } else {
          ++segment_no;
        }
      });
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [](H4PacketWithHci&&) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),

                              /*le_acl_credits_to_reserve=*/UINT8_MAX,
                              /*br_edr_acl_credits_to_reserve=*/0);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(
      proxy,
      /*num_credits_to_reserve=*/UINT8_MAX,
      /*le_acl_data_packet_length=*/UINT16_MAX));

  uint16_t sdus_sent = 0;

  // Test sending payload segmented in every possible way, from MPS of 23 octets
  // to MPS values 5 octets greater than the payload size. 23 bytes is the
  // minimum MPS supported for L2CAP channels.
  for (capture.mps = 23; capture.mps < kPayloadSize + 5; ++capture.mps) {
    L2capCoc channel = BuildCoc(proxy,
                                {.handle = capture.handle,
                                 .remote_cid = capture.remote_cid,
                                 .tx_mtu = capture.expected_payload.size(),
                                 .tx_mps = capture.mps,
                                 .tx_credits = UINT8_MAX});
    PW_TEST_EXPECT_OK(
        channel.Write(MultiBufFromSpan(span(capture.expected_payload))).status);
    ++sdus_sent;

    // Replenish proxy's LE ACL send credits, or else only UINT8_MAX PDUs could
    // be sent in this test.
    PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
        proxy,
        FlatMap<uint16_t, uint16_t, 1>(
            {{{capture.handle,
               static_cast<uint8_t>(UINT8_MAX -
                                    proxy.GetNumFreeLeAclPackets())}}})));
  }

  EXPECT_EQ(capture.sdus_received, sdus_sent);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
