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
#include <numeric>

#include "pw_bluetooth/att.emb.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth/l2cap_frames.emb.h"
#include "pw_bluetooth/rfcomm_frames.emb.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/proxy_host.h"
#include "pw_bluetooth_proxy_private/test_utils.h"
#include "pw_containers/flat_map.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {
namespace {

using containers::FlatMap;

// ########## RfcommWriteTest

// Construct and send an RFCOMM frame from controller->host.
Status SendRfcommFromController(ProxyHost& proxy,
                                RfcommParameters params,
                                uint8_t fcs,
                                std::optional<uint8_t> credits,
                                pw::span<uint8_t> payload) {
  constexpr size_t kMaxShortLength = 0x7f;
  const size_t credits_field_size = credits.has_value() ? 1 : 0;
  const bool uses_extended_length = payload.size() > kMaxShortLength;
  const size_t length_extended_size = uses_extended_length ? 1 : 0;
  const size_t frame_size = emboss::RfcommFrame::MinSizeInBytes() +
                            length_extended_size + credits_field_size +
                            payload.size();

  PW_TRY_ASSIGN(BFrameWithStorage bframe,
                SetupBFrame(params.handle, params.rx_config.cid, frame_size));

  auto rfcomm = emboss::MakeRfcommFrameView(
      bframe.writer.payload().BackingStorage().data(),
      bframe.writer.payload().SizeInBytes());
  rfcomm.extended_address().Write(true);
  rfcomm.command_response_direction().Write(
      emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
  rfcomm.channel().Write(params.rfcomm_channel);

  if (!uses_extended_length) {
    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
    rfcomm.length().Write(payload.size());
  } else {
    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::EXTENDED);
    rfcomm.length_extended().Write(payload.size());
  }

  if (credits.has_value()) {
    rfcomm.control().Write(
        emboss::RfcommFrameType::
            UNNUMBERED_INFORMATION_WITH_HEADER_CHECK_AND_POLL_FINAL);
    rfcomm.credits().Write(*credits);
  } else {
    rfcomm.control().Write(
        emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);
  }

  EXPECT_EQ(rfcomm.information().SizeInBytes(), payload.size());
  std::memcpy(rfcomm.information().BackingStorage().data(),
              payload.data(),
              payload.size());
  rfcomm.fcs().Write(fcs);
  auto hci_span = bframe.acl.hci_span();
  H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, hci_span};

  proxy.HandleH4HciFromController(std::move(packet));

  return OkStatus();
}

TEST(RfcommWriteTest, BasicWrite) {
  struct {
    int sends_called = 0;
    // First four bits 0x0 encode PB & BC flags
    uint16_t handle = 0x0ACB;
    // Length of L2CAP PDU
    uint16_t acl_data_total_length = 0x000C;
    // L2CAP header PDU length field
    uint16_t pdu_length = 0x0008;
    // Random CID
    uint16_t channel_id = 0x1234;
    // RFCOMM header
    std::array<uint8_t, 3> rfcomm_header = {0x19, 0xFF, 0x07};
    uint8_t rfcomm_credits = 0;
    // RFCOMM information payload
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
    uint8_t rfcomm_fcs = 0x49;

    // Built from the preceding values in little endian order (except payload in
    // big endian).
    std::array<uint8_t, 16> expected_hci_packet = {0xCB,
                                                   0x0A,
                                                   0x0C,
                                                   0x00,
                                                   0x08,
                                                   0x00,
                                                   0x34,
                                                   0x12,
                                                   // RFCOMM header
                                                   0x19,
                                                   0xFF,
                                                   0x07,
                                                   0x00,
                                                   0xAB,
                                                   0xCD,
                                                   0xEF,
                                                   // FCS
                                                   0x49};
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
        emboss::BFrameView bframe = emboss::BFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        EXPECT_EQ(bframe.pdu_length().Read(), capture.pdu_length);
        EXPECT_EQ(bframe.channel_id().Read(), capture.channel_id);
        EXPECT_TRUE(std::equal(bframe.payload().BackingStorage().begin(),
                               bframe.payload().BackingStorage().begin() +
                                   capture.rfcomm_header.size(),
                               capture.rfcomm_header.begin(),
                               capture.rfcomm_header.end()));
        auto rfcomm = emboss::MakeRfcommFrameView(
            bframe.payload().BackingStorage().data(),
            bframe.payload().SizeInBytes());
        EXPECT_TRUE(rfcomm.Ok());
        EXPECT_EQ(rfcomm.credits().Read(), capture.rfcomm_credits);

        for (size_t i = 0; i < 3; ++i) {
          EXPECT_EQ(rfcomm.information()[i].Read(), capture.payload[i]);
        }

        EXPECT_EQ(rfcomm.fcs().Read(), capture.rfcomm_fcs);
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(proxy, 1));

  RfcommParameters params = {.handle = capture.handle,
                             .tx_config = {
                                 .cid = capture.channel_id,
                                 .max_information_length = 900,
                                 .credits = 10,
                             }};
  RfcommChannel channel = BuildRfcomm(proxy, params);
  EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(RfcommWriteTest, ExtendedWrite) {
  constexpr size_t kPayloadSize = 0x80;
  struct {
    int sends_called = 0;
    // First four bits 0x0 encode PB & BC flags
    uint16_t handle = 0x0ACB;
    // Length of L2CAP PDU
    uint16_t acl_data_total_length = 0x008A;
    // L2CAP header PDU length field
    uint16_t pdu_length = 0x0086;
    // Random CID
    uint16_t channel_id = 0x1234;
    // RFCOMM header
    std::array<uint8_t, 4> rfcomm_header = {0x19, 0xFF, 0x00, 0x01};
    uint8_t rfcomm_credits = 0;
    // RFCOMM information payload
    std::array<uint8_t, kPayloadSize> payload = {
        0xAB,
        0xCD,
        0xEF,
    };
    uint8_t rfcomm_fcs = 0x49;

    // Built from the preceding values in little endian order (except payload in
    // big endian).
    std::array<uint8_t, kPayloadSize + 14> expected_hci_packet = {
        0xCB,
        0x0A,
        0x8A,
        0x00,
        0x86,
        0x00,
        0x34,
        0x12,
        // RFCOMM header
        0x19,
        0xFF,
        0x00,
        0x01,
        0x00,
        0xAB,
        0xCD,
        0xEF,
    };
  } capture;

  // FCS
  capture.expected_hci_packet[capture.expected_hci_packet.size() - 1] =
      capture.rfcomm_fcs;

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
        emboss::BFrameView bframe = emboss::BFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        EXPECT_EQ(bframe.pdu_length().Read(), capture.pdu_length);
        EXPECT_EQ(bframe.channel_id().Read(), capture.channel_id);
        EXPECT_TRUE(std::equal(bframe.payload().BackingStorage().begin(),
                               bframe.payload().BackingStorage().begin() +
                                   capture.rfcomm_header.size(),
                               capture.rfcomm_header.begin(),
                               capture.rfcomm_header.end()));
        auto rfcomm = emboss::MakeRfcommFrameView(
            bframe.payload().BackingStorage().data(),
            bframe.payload().SizeInBytes());
        EXPECT_TRUE(rfcomm.Ok());
        EXPECT_EQ(rfcomm.credits().Read(), capture.rfcomm_credits);

        for (size_t i = 0; i < 3; ++i) {
          EXPECT_EQ(rfcomm.information()[i].Read(), capture.payload[i]);
        }

        EXPECT_EQ(rfcomm.fcs().Read(), capture.rfcomm_fcs);
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(proxy, 1));

  RfcommParameters params = {.handle = capture.handle,
                             .tx_config = {
                                 .cid = capture.channel_id,
                                 .max_information_length = 900,
                                 .credits = 10,
                             }};
  RfcommChannel channel = BuildRfcomm(proxy, params);
  EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(RfcommWriteTest, WriteFlowControl) {
  struct {
    int sends_called = 0;
    int queue_unblocked = 0;
    // RFCOMM information payload
    std::array<uint8_t, 3> payload = {0xAB, 0xCD, 0xEF};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&&) { ++capture.sends_called; });
  pw::Function<void()> queue_space_available_fn(
      [&capture]() { ++capture.queue_unblocked; });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn),
      std::move(send_to_controller_fn),
      /*le_acl_credits_to_reserve=*/0,
      /*br_edr_acl_credits_to_reserve=*/RfcommChannel::QueueCapacity() + 1);
  // Start with plenty of ACL credits to test RFCOMM logic.
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(
      proxy, RfcommChannel::QueueCapacity() + 1));

  RfcommParameters params = {.tx_config = {
                                 .cid = 123,
                                 .max_information_length = 900,
                                 .credits = 0,
                             }};
  RfcommChannel channel = BuildRfcomm(
      proxy,
      params,
      /*receive_fn=*/nullptr,
      /*queue_space_available_fn=*/std::move(queue_space_available_fn),
      /*event_fn=*/nullptr);

  // Writes while queue has space will return Ok. No RFCOMM credits yet though
  // so no sends complete.
  EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 0);
  EXPECT_EQ(capture.queue_unblocked, 0);

  // Provide a credit
  constexpr uint8_t kExpectedFcs = 0xE6;
  PW_TEST_EXPECT_OK(SendRfcommFromController(
      proxy, params, kExpectedFcs, /*credits=*/1, /*payload=*/{}));
  EXPECT_EQ(capture.queue_unblocked, 0);
  EXPECT_EQ(capture.sends_called, 1);

  // Now fill up queue
  uint16_t queued = 0;
  while (true) {
    if (const auto status = channel.Write(capture.payload);
        status == Status::Unavailable()) {
      break;
    }
    ++queued;
  }

  // Unblock queue with ACL and RFCOMM credits
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{params.handle, queued}}})));
  PW_TEST_EXPECT_OK(SendRfcommFromController(
      proxy, params, kExpectedFcs, /*credits=*/queued, /*payload=*/{}));

  EXPECT_EQ(capture.sends_called, queued + 1);
  EXPECT_EQ(capture.queue_unblocked, 1);
}

// ########## RfcommReadTest

TEST(RfcommReadTest, BasicRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  struct {
    int rx_called = 0;
    std::array<uint8_t, 3> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr uint8_t kExpectedFcs = 0xFA;

  RfcommParameters params = {};
  RfcommChannel channel =
      BuildRfcomm(proxy,
                  params,
                  /*receive_fn=*/[&capture](pw::span<uint8_t> payload) {
                    ++capture.rx_called;
                    EXPECT_TRUE(std::equal(payload.begin(),
                                           payload.end(),
                                           capture.expected_payload.begin(),
                                           capture.expected_payload.end()));
                  });

  PW_TEST_EXPECT_OK(SendRfcommFromController(proxy,
                                             params,
                                             kExpectedFcs,
                                             /*credits=*/std::nullopt,
                                             capture.expected_payload));
  EXPECT_EQ(capture.rx_called, 1);
}

TEST(RfcommReadTest, ExtendedRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  constexpr size_t kPayloadSize = 0x80;
  struct {
    int rx_called = 0;
    std::array<uint8_t, kPayloadSize> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr uint8_t kExpectedFcs = 0xFA;

  RfcommParameters params = {};
  RfcommChannel channel =
      BuildRfcomm(proxy,
                  params, /*receive_fn=*/
                  [&capture](pw::span<uint8_t> payload) {
                    ++capture.rx_called;
                    EXPECT_TRUE(std::equal(payload.begin(),
                                           payload.end(),
                                           capture.expected_payload.begin(),
                                           capture.expected_payload.end()));
                  });
  PW_TEST_EXPECT_OK(SendRfcommFromController(proxy,
                                             params,
                                             kExpectedFcs,
                                             /*credits=*/std::nullopt,
                                             capture.expected_payload));

  EXPECT_EQ(capture.rx_called, 1);
}

TEST(RfcommReadTest, InvalidReads) {
  struct {
    int rx_called = 0;
    int host_called = 0;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&capture]([[maybe_unused]] H4PacketWithHci&& packet) {
        ++capture.host_called;
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/0);

  constexpr uint8_t kExpectedFcs = 0xFA;
  constexpr uint8_t kInvalidFcs = 0xFF;

  RfcommParameters params = {};
  RfcommChannel channel =
      BuildRfcomm(proxy,
                  params, /*receive_fn=*/
                  [&capture](pw::span<uint8_t>) { ++capture.rx_called; });

  // Construct valid packet but put invalid checksum on the end. Test that we
  // don't get it sent on to us.
  PW_TEST_EXPECT_OK(SendRfcommFromController(proxy,
                                             params,
                                             kInvalidFcs,
                                             /*credits=*/std::nullopt,
                                             /*payload=*/{}));
  EXPECT_EQ(capture.rx_called, 0);
  EXPECT_EQ(capture.host_called, 1);

  // Construct packet with everything valid but wrong length for actual data
  // size. Ensure it doesn't end up being sent to our channel, but does get
  // forwarded to host.
  {
    PW_TEST_ASSERT_OK_AND_ASSIGN(
        BFrameWithStorage bframe,
        SetupBFrame(params.handle,
                    params.rx_config.cid,
                    emboss::RfcommFrame::MinSizeInBytes()));

    auto rfcomm = emboss::MakeRfcommFrameView(
        bframe.writer.payload().BackingStorage().data(),
        bframe.writer.payload().SizeInBytes());
    rfcomm.extended_address().Write(true);
    rfcomm.command_response_direction().Write(
        emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
    rfcomm.channel().Write(params.rfcomm_channel);

    rfcomm.control().Write(
        emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);

    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
    // Invalid length.
    rfcomm.length().Write(1);
    // Can't Write FCS as emboss will assert because of invalid length. Place
    // manually.
    pw::span<uint8_t> hci_span = bframe.acl.hci_span();
    hci_span[hci_span.size() - 1] = kExpectedFcs;

    H4PacketWithHci packet{emboss::H4PacketType::ACL_DATA, hci_span};
    proxy.HandleH4HciFromController(std::move(packet));
  }

  EXPECT_EQ(capture.rx_called, 0);
  EXPECT_EQ(capture.host_called, 2);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
