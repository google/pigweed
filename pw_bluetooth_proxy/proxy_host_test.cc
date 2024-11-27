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

#include "pw_bluetooth_proxy/proxy_host.h"

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
#include "pw_containers/flat_map.h"
#include "pw_function/function.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

using containers::FlatMap;

// TODO: https://pwbug.dev/349700888 - Once size is configurable, switch to
// specifying directly. Until then this should match
// AclConnection::kMaxConnections.
static constexpr size_t kMaxProxyActiveConnections = 10;

// ########## Util functions

// Populate passed H4 command buffer and return Emboss view on it.
template <typename EmbossT>
Result<EmbossT> CreateAndPopulateToControllerView(H4PacketWithH4& h4_packet,
                                                  emboss::OpCode opcode) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 100);
  h4_packet.SetH4Type(emboss::H4PacketType::COMMAND);
  PW_TRY_ASSIGN(auto view, MakeEmbossWriter<EmbossT>(h4_packet.GetHciSpan()));
  view.header().opcode_enum().Write(opcode);
  return view;
}

// Return a populated H4 command buffer of a type that proxy host doesn't
// interact with.
Status PopulateNoninteractingToControllerBuffer(H4PacketWithH4& h4_packet) {
  return CreateAndPopulateToControllerView<emboss::InquiryCommandWriter>(
             h4_packet, emboss::OpCode::LINK_KEY_REQUEST_REPLY)
      .status();
}

// Populate passed H4 event buffer and return Emboss view on it.
template <typename EmbossT>
Result<EmbossT> CreateAndPopulateToHostEventView(H4PacketWithHci& h4_packet,
                                                 emboss::EventCode event_code) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 0x10);
  h4_packet.SetH4Type(emboss::H4PacketType::EVENT);

  PW_TRY_ASSIGN(auto view, MakeEmbossWriter<EmbossT>(h4_packet.GetHciSpan()));
  view.header().event_code_enum().Write(event_code);
  view.status().Write(emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(view.Ok());
  return view;
}

// Send an LE_Read_Buffer_Size (V2) CommandComplete event to `proxy` to request
// the reservation of a number of LE ACL send credits.
Status SendLeReadBufferResponseFromController(ProxyHost& proxy,
                                              uint8_t num_credits_to_reserve) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TRY_ASSIGN(auto view,
                CreateAndPopulateToHostEventView<
                    emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
                    h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
  view.total_num_le_acl_data_packets().Write(num_credits_to_reserve);

  proxy.HandleH4HciFromController(std::move(h4_packet));
  return OkStatus();
}

Status SendReadBufferResponseFromController(ProxyHost& proxy,
                                            uint8_t num_credits_to_reserve) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TRY_ASSIGN(auto view,
                CreateAndPopulateToHostEventView<
                    emboss::ReadBufferSizeCommandCompleteEventWriter>(
                    h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(num_credits_to_reserve);
  EXPECT_TRUE(view.Ok());

  proxy.HandleH4HciFromController(std::move(h4_packet));
  return OkStatus();
}

// Send a Number_of_Completed_Packets event to `proxy` that reports each
// {connection handle, number of completed packets} entry provided.
template <size_t kNumConnections>
Status SendNumberOfCompletedPackets(
    ProxyHost& proxy,
    FlatMap<uint16_t, uint16_t, kNumConnections> packets_per_connection) {
  std::array<
      uint8_t,
      emboss::NumberOfCompletedPacketsEvent::MinSizeInBytes() +
          kNumConnections *
              emboss::NumberOfCompletedPacketsEventData::IntrinsicSizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci nocp_event{emboss::H4PacketType::EVENT, hci_arr};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::NumberOfCompletedPacketsEventWriter>(
                    nocp_event.GetHciSpan()));
  view.header().event_code_enum().Write(
      emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);
  view.num_handles().Write(kNumConnections);

  size_t i = 0;
  for (const auto& [handle, num_packets] : packets_per_connection) {
    view.nocp_data()[i].connection_handle().Write(handle);
    view.nocp_data()[i].num_completed_packets().Write(num_packets);
    ++i;
  }

  proxy.HandleH4HciFromController(std::move(nocp_event));
  return OkStatus();
}

// Send a Disconnection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
Status SendDisconnectionCompleteEvent(ProxyHost& proxy,
                                      uint16_t handle,
                                      bool successful = true) {
  std::array<uint8_t,
             emboss::DisconnectionCompleteEvent::IntrinsicSizeInBytes()>
      hci_arr_dc;
  hci_arr_dc.fill(0);
  H4PacketWithHci dc_event{emboss::H4PacketType::EVENT, hci_arr_dc};
  PW_TRY_ASSIGN(auto view,
                MakeEmbossWriter<emboss::DisconnectionCompleteEventWriter>(
                    dc_event.GetHciSpan()));
  view.header().event_code_enum().Write(
      emboss::EventCode::DISCONNECTION_COMPLETE);
  view.status().Write(successful ? emboss::StatusCode::SUCCESS
                                 : emboss::StatusCode::HARDWARE_FAILURE);
  view.connection_handle().Write(handle);
  proxy.HandleH4HciFromController(std::move(dc_event));
  return OkStatus();
}

// Return a populated H4 event buffer of a type that proxy host doesn't interact
// with.
Status CreateNonInteractingToHostBuffer(H4PacketWithHci& h4_packet) {
  return CreateAndPopulateToHostEventView<emboss::InquiryCompleteEventWriter>(
             h4_packet, emboss::EventCode::INQUIRY_COMPLETE)
      .status();
}

struct CocParameters {
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  uint16_t remote_cid = 456;
  uint16_t rx_mtu = 100;
  uint16_t rx_mps = 100;
  uint16_t rx_credits = 1;
  uint16_t tx_mtu = 100;
  uint16_t tx_mps = 100;
  uint16_t tx_credits = 1;
  pw::Function<void(pw::span<uint8_t> payload)>&& receive_fn = nullptr;
  pw::Function<void(L2capCoc::Event event)>&& event_fn = nullptr;
};

// Open and return an L2CAP connection-oriented channel managed by `proxy`.
L2capCoc BuildCoc(ProxyHost& proxy, CocParameters params) {
  pw::Result<L2capCoc> channel =
      proxy.AcquireL2capCoc(params.handle,
                            {.cid = params.local_cid,
                             .mtu = params.rx_mtu,
                             .mps = params.rx_mps,
                             .credits = params.rx_credits},
                            {.cid = params.remote_cid,
                             .mtu = params.tx_mtu,
                             .mps = params.tx_mps,
                             .credits = params.tx_credits},
                            std::move(params.receive_fn),
                            std::move(params.event_fn));
  return std::move(channel.value());
}

struct RfcommParameters {
  uint16_t handle = 123;
  RfcommChannel::Config rx_config = {
      .cid = 234, .max_information_length = 900, .credits = 10};
  RfcommChannel::Config tx_config = {
      .cid = 456, .max_information_length = 900, .credits = 10};
  uint8_t rfcomm_channel = 3;
};

RfcommChannel BuildRfcomm(ProxyHost& proxy, RfcommParameters params) {
  pw::Result<RfcommChannel> channel =
      proxy.AcquireRfcommChannel(params.handle,
                                 params.rx_config,
                                 params.tx_config,
                                 params.rfcomm_channel,
                                 nullptr);
  PW_TEST_EXPECT_OK(channel);
  return std::move((channel.value()));
}

// ########## Examples

// Example for docs.rst.
TEST(Example, ExampleUsage) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      h4_array_from_host{};
  H4PacketWithH4 h4_packet_from_host{emboss::H4PacketType::UNKNOWN,
                                     h4_array_from_host};
  PW_TEST_EXPECT_OK(
      PopulateNoninteractingToControllerBuffer(h4_packet_from_host));

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      hci_array_from_controller{};
  H4PacketWithHci h4_packet_from_controller{emboss::H4PacketType::UNKNOWN,
                                            hci_array_from_controller};

  PW_TEST_EXPECT_OK(
      CreateNonInteractingToHostBuffer(h4_packet_from_controller));

  pw::Function<void(H4PacketWithHci && packet)> container_send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  pw::Function<void(H4PacketWithH4 && packet)> container_send_to_controller_fn(
      ([]([[maybe_unused]] H4PacketWithH4&& packet) {}));

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]

#include "pw_bluetooth_proxy/proxy_host.h"

  // Container creates ProxyHost .
  ProxyHost proxy = ProxyHost(std::move(container_send_to_host_fn),
                              std::move(container_send_to_controller_fn),
                              2);

  // Container passes H4 packets from host through proxy. Proxy will in turn
  // call the container-provided `container_send_to_controller_fn` to pass them
  // on to the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromHost(std::move(h4_packet_from_host));

  // Container passes H4 packets from controller through proxy. Proxy will in
  // turn call the container-provided `container_send_to_host_fn` to pass them
  // on to the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromController(std::move(h4_packet_from_controller));

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]
}

// ########## PassthroughTest

// Verify buffer is properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToControllerPassesEqualBuffer) {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr{};
  H4PacketWithH4 h4_packet{emboss::H4PacketType::UNKNOWN, h4_arr};
  PW_TEST_EXPECT_OK(PopulateNoninteractingToControllerBuffer(h4_packet));

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr;
    H4PacketWithH4* h4_packet;
    uint8_t sends_called;
  } send_capture = {h4_arr, &h4_packet, 0};

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&send_capture](H4PacketWithH4&& packet) {
        send_capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(),
                  emboss::H4PacketType(send_capture.h4_arr[0]));
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_arr.begin() + 1,
                               send_capture.h4_arr.end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(std::move(h4_packet));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

// Verify buffer is properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToHostPassesEqualBuffer) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_EXPECT_OK(CreateNonInteractingToHostBuffer(h4_packet));

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
        hci_arr;
    H4PacketWithHci* h4_packet;
    uint8_t sends_called;
  } send_capture = {hci_arr, &h4_packet, 0};

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci&& packet) {
        send_capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(), send_capture.h4_packet->GetH4Type());
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

// Verify a command complete event (of a type that proxy doesn't act on) is
// properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToHostPassesEqualCommandComplete) {
  std::array<
      uint8_t,
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<
        uint8_t,
        emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes()>
        hci_arr;
    H4PacketWithHci* h4_packet;
    uint8_t sends_called;
  } send_capture = {hci_arr, &h4_packet, 0};

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci&& packet) {
        send_capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(), send_capture.h4_packet->GetH4Type());
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

// ########## BadPacketTest
// The proxy should not affect buffers it can't process (it should just pass
// them on).

TEST(BadPacketTest, BadH4TypeToControllerIsPassedOn) {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr{};
  H4PacketWithH4 h4_packet{emboss::H4PacketType::UNKNOWN, h4_arr};
  PW_TEST_EXPECT_OK(PopulateNoninteractingToControllerBuffer(h4_packet));
  // Set back to an invalid type (after
  // PopulateNoninteractingToControllerBuffer).
  h4_packet.SetH4Type(emboss::H4PacketType::UNKNOWN);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr;
    H4PacketWithH4* h4_packet;
    uint8_t sends_called;
  } send_capture = {h4_arr, &h4_packet, 0};

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&send_capture](H4PacketWithH4&& packet) {
        send_capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(),
                  emboss::H4PacketType(send_capture.h4_arr[0]));
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_arr.begin() + 1,
                               send_capture.h4_arr.end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(std::move(h4_packet));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

TEST(BadPacketTest, BadH4TypeToHostIsPassedOn) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_EXPECT_OK(CreateNonInteractingToHostBuffer(h4_packet));

  // Set back to an invalid type.
  h4_packet.SetH4Type(emboss::H4PacketType::UNKNOWN);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
        hci_arr;
    H4PacketWithHci* h4_packet;
    uint8_t sends_called = 0;
  } send_capture = {hci_arr, &h4_packet, 0};

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci&& packet) {
        send_capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

TEST(BadPacketTest, EmptyBufferToControllerIsPassedOn) {
  std::array<uint8_t, 0> h4_arr;
  H4PacketWithH4 h4_packet{emboss::H4PacketType::COMMAND, h4_arr};
  // H4PacketWithH4 use the underlying h4 buffer to store type. Since its length
  // is zero, it can't store it and will always return UNKNOWN.
  EXPECT_EQ(h4_packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&sends_called](H4PacketWithH4&& packet) {
        sends_called++;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
        EXPECT_TRUE(packet.GetHciSpan().empty());
      });

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(std::move(h4_packet));

  // Verify callback was called.
  EXPECT_EQ(sends_called, 1);
}

TEST(BadPacketTest, EmptyBufferToHostIsPassedOn) {
  std::array<uint8_t, 0> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::EVENT, hci_arr};

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& packet) {
        sends_called++;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
        EXPECT_TRUE(packet.GetHciSpan().empty());
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify callback was called.
  EXPECT_EQ(sends_called, 1);
}

TEST(BadPacketTest, TooShortEventToHostIsPassOn) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
      valid_hci_arr{};
  H4PacketWithHci valid_packet{emboss::H4PacketType::UNKNOWN, valid_hci_arr};
  PW_TEST_EXPECT_OK(CreateNonInteractingToHostBuffer(valid_packet));

  // Create packet for sending whose span size is one less than a valid command
  // complete event.
  H4PacketWithHci h4_packet{valid_packet.GetH4Type(),
                            valid_packet.GetHciSpan().subspan(
                                0, emboss::EventHeaderView::SizeInBytes() - 1)};

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::EventHeaderView::SizeInBytes() - 1> hci_arr;
    uint8_t sends_called = 0;
  } send_capture;
  // Copy valid event into a short_array whose size is one less than a valid
  // EventHeader.
  std::copy(h4_packet.GetHciSpan().begin(),
            h4_packet.GetHciSpan().end(),
            send_capture.hci_arr.begin());
  send_capture.sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci&& packet) {
        send_capture.sends_called++;
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               send_capture.hci_arr.begin(),
                               send_capture.hci_arr.end()));
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

TEST(BadPacketTest, TooShortCommandCompleteEventToHost) {
  std::array<
      uint8_t,
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes()>
      valid_hci_arr{};
  H4PacketWithHci valid_packet{emboss::H4PacketType::UNKNOWN, valid_hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          valid_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  // Create packet for sending whose span size is one less than a valid command
  // complete event.
  H4PacketWithHci h4_packet{
      valid_packet.GetH4Type(),
      valid_packet.GetHciSpan().subspan(
          0,
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter::
                  SizeInBytes() -
              1)};

  // Struct for capturing because `pw::Function` capture can't fit multiple
  // fields .
  struct {
    std::array<
        uint8_t,
        emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes() -
            1>
        hci_arr;
    uint8_t sends_called = 0;
  } send_capture;
  std::copy(h4_packet.GetHciSpan().begin(),
            h4_packet.GetHciSpan().end(),
            send_capture.hci_arr.begin());
  send_capture.sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci&& packet) {
        send_capture.sends_called++;
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               send_capture.hci_arr.begin(),
                               send_capture.hci_arr.end()));
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  // Verify callback was called.
  EXPECT_EQ(send_capture.sends_called, 1);
}

// ########## ReserveLeAclCredits Tests

// Proxy Host should reserve requested ACL credits from controller's ACL credits
// when using ReadBufferSize command.
TEST(ReserveBrEdrAclCredits, ProxyCreditsReserveCreditsWithReadBufferSize) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::ReadBufferSizeCommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossWriter<emboss::ReadBufferSizeCommandCompleteEventWriter>(
                received_packet.GetHciSpan()));
        // Should reserve 2 credits from original total of 10 (so 8 left for
        // host).
        EXPECT_EQ(event_view.total_num_acl_data_packets().Read(), 8);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/0,
                              /*br_edr_acl_credits_to_reserve=*/2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeBrEdrAclPackets(), 2);

  EXPECT_TRUE(proxy.HasSendBrEdrAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// Proxy Host should reserve requested ACL LE credits from controller's ACL LE
// credits when using LEReadBufferSizeV1 command.
TEST(ReserveLeAclCredits, ProxyCreditsReserveCreditsWithLEReadBufferSizeV1) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossView<
                emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan()));

        // Should reserve 2 credits from original total of 10 (so 8 left for
        // host).
        EXPECT_EQ(event_view.total_num_le_acl_data_packets().Read(), 8);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  EXPECT_TRUE(proxy.HasSendLeAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// Proxy Host should reserve requested ACL LE credits from controller's ACL LE
// credits when using LEReadBufferSizeV2 command.
TEST(ReserveLeAclCredits, ProxyCreditsReserveCreditsWithLEReadBufferSizeV2) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossView<
                emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
                received_packet.GetHciSpan()));
        // Should reserve 2 credits from original total of 10 (so 8 left for
        // host).
        EXPECT_EQ(event_view.total_num_le_acl_data_packets().Read(), 8);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  EXPECT_TRUE(proxy.HasSendLeAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// If controller provides less than wanted credits, we should reserve that
// smaller amount.
TEST(ReserveLeAclCredits, ProxyCreditsCappedByControllerCredits) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(5);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        // We want 7, but can reserve only 5 from original 5 (so 0 left for
        // host).
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossView<
                emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan()));
        EXPECT_EQ(event_view.total_num_le_acl_data_packets().Read(), 0);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 7);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 5);

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// Proxy Host can reserve zero credits from controller's ACL LE credits.
TEST(ReserveLeAclCredits, ProxyCreditsReserveZeroCredits) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossView<
                emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan()));
        // Should reserve 0 credits from original total of 10 (so 10 left for
        // host).
        EXPECT_EQ(event_view.total_num_le_acl_data_packets().Read(), 10);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_FALSE(proxy.HasSendLeAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// If controller has no credits, proxy should reserve none.
TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenHostCreditsZero) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto view,
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE));
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(0);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_view,
            MakeEmbossView<
                emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan()));
        // Should reserve 0 credit from original total of 0 (so 0 left for
        // host).
        EXPECT_EQ(event_view.total_num_le_acl_data_packets().Read(), 0);
      });

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_TRUE(proxy.HasSendLeAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenNotInitialized) {
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_TRUE(proxy.HasSendLeAclCapability());
}

// ########## GattNotifyTest

TEST(GattNotifyTest, Send1ByteAttribute) {
  struct {
    int sends_called = 0;
    // First four bits 0x0 encode PB & BC flags
    uint16_t handle = 0x0ACB;
    // Length of L2CAP PDU
    uint16_t acl_data_total_length = 0x0008;
    // Length of ATT PDU
    uint16_t pdu_length = 0x0004;
    // Attribute protocol channel ID (0x0004)
    uint16_t channel_id = 0x0004;
    // ATT_HANDLE_VALUE_NTF opcode 0x1B
    uint8_t attribute_opcode = 0x1B;
    uint16_t attribute_handle = 0x4321;
    std::array<uint8_t, 1> attribute_value = {0xFA};

    // Built from the preceding values in little endian order.
    std::array<uint8_t, 12> expected_gatt_notify_packet = {
        0xCB, 0x0A, 0x08, 0x00, 0x04, 0x00, 0x04, 0x00, 0x1B, 0x21, 0x43, 0xFA};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        capture.sends_called++;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::ACL_DATA);
        EXPECT_EQ(packet.GetHciSpan().size(),
                  capture.expected_gatt_notify_packet.size());
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               capture.expected_gatt_notify_packet.begin(),
                               capture.expected_gatt_notify_packet.end()));
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto acl,
            MakeEmbossView<emboss::AclDataFrameView>(packet.GetHciSpan()));
        emboss::BFrameView l2cap =
            emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                                   acl.data_total_length().Read());
        emboss::AttHandleValueNtfView gatt_notify =
            emboss::MakeAttHandleValueNtfView(
                capture.attribute_value.size(),
                l2cap.payload().BackingStorage().data(),
                l2cap.pdu_length().Read());
        EXPECT_EQ(acl.header().handle().Read(), capture.handle);
        EXPECT_EQ(acl.header().packet_boundary_flag().Read(),
                  emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
        EXPECT_EQ(acl.header().broadcast_flag().Read(),
                  emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
        EXPECT_EQ(acl.data_total_length().Read(),
                  capture.acl_data_total_length);
        EXPECT_EQ(l2cap.pdu_length().Read(), capture.pdu_length);
        EXPECT_EQ(l2cap.channel_id().Read(), capture.channel_id);
        EXPECT_EQ(gatt_notify.attribute_opcode().Read(),
                  static_cast<emboss::AttOpcode>(capture.attribute_opcode));
        EXPECT_EQ(gatt_notify.attribute_handle().Read(),
                  capture.attribute_handle);
        EXPECT_EQ(gatt_notify.attribute_value()[0].Read(),
                  capture.attribute_value[0]);
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  PW_TEST_EXPECT_OK(proxy.SendGattNotify(capture.handle,
                                         capture.attribute_handle,
                                         pw::span(capture.attribute_value)));
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(GattNotifyTest, Send2ByteAttribute) {
  struct {
    int sends_called = 0;
    // Max connection_handle value; first four bits 0x0 encode PB & BC flags
    const uint16_t handle = 0x0EFF;
    // Length of L2CAP PDU
    const uint16_t acl_data_total_length = 0x0009;
    // Length of ATT PDU
    const uint16_t pdu_length = 0x0005;
    // Attribute protocol channel ID (0x0004)
    const uint16_t channel_id = 0x0004;
    // ATT_HANDLE_VALUE_NTF opcode 0x1B
    const uint8_t attribute_opcode = 0x1B;
    const uint16_t attribute_handle = 0x1234;
    const std::array<uint8_t, 2> attribute_value = {0xAB, 0xCD};

    // Built from the preceding values in little endian order.
    const std::array<uint8_t, 13> expected_gatt_notify_packet = {0xFF,
                                                                 0x0E,
                                                                 0x09,
                                                                 0x00,
                                                                 0x05,
                                                                 0x00,
                                                                 0x04,
                                                                 0x00,
                                                                 0x1B,
                                                                 0x34,
                                                                 0x12,
                                                                 0xAB,
                                                                 0XCD};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});

  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        ++capture.sends_called;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::ACL_DATA);
        EXPECT_EQ(packet.GetHciSpan().size(),
                  capture.expected_gatt_notify_packet.size());
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               capture.expected_gatt_notify_packet.begin(),
                               capture.expected_gatt_notify_packet.end()));
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto acl,
            MakeEmbossView<emboss::AclDataFrameView>(packet.GetHciSpan()));
        emboss::BFrameView l2cap = emboss::MakeBFrameView(
            acl.payload().BackingStorage().data(), acl.SizeInBytes());
        emboss::AttHandleValueNtfView gatt_notify =
            emboss::MakeAttHandleValueNtfView(
                capture.attribute_value.size(),
                l2cap.payload().BackingStorage().data(),
                l2cap.pdu_length().Read());
        EXPECT_EQ(acl.header().handle().Read(), capture.handle);
        EXPECT_EQ(acl.header().packet_boundary_flag().Read(),
                  emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
        EXPECT_EQ(acl.header().broadcast_flag().Read(),
                  emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
        EXPECT_EQ(acl.data_total_length().Read(),
                  capture.acl_data_total_length);
        EXPECT_EQ(l2cap.pdu_length().Read(), capture.pdu_length);
        EXPECT_EQ(l2cap.channel_id().Read(), capture.channel_id);
        EXPECT_EQ(gatt_notify.attribute_opcode().Read(),
                  static_cast<emboss::AttOpcode>(capture.attribute_opcode));
        EXPECT_EQ(gatt_notify.attribute_handle().Read(),
                  capture.attribute_handle);
        EXPECT_EQ(gatt_notify.attribute_value()[0].Read(),
                  capture.attribute_value[0]);
        EXPECT_EQ(gatt_notify.attribute_value()[1].Read(),
                  capture.attribute_value[1]);
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  PW_TEST_EXPECT_OK(proxy.SendGattNotify(capture.handle,
                                         capture.attribute_handle,
                                         pw::span(capture.attribute_value)));
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(GattNotifyTest, ReturnsErrorIfAttributeTooLarge) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4 packet) { FAIL(); });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  // attribute_value 1 byte too large
  std::array<uint8_t,
             proxy.GetMaxAclSendSize() -
                 emboss::AclDataFrameHeader::IntrinsicSizeInBytes() -
                 emboss::BasicL2capHeader::IntrinsicSizeInBytes() -
                 emboss::AttHandleValueNtf::MinSizeInBytes() + 1>
      attribute_value_too_large;
  EXPECT_EQ(proxy.SendGattNotify(123, 456, attribute_value_too_large),
            PW_STATUS_INVALID_ARGUMENT);
}

TEST(GattNotifyTest, ChannelIsNotConstructedIfParametersInvalid) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4 packet) { FAIL(); });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  EXPECT_EQ(proxy.SendGattNotify(123, 0, {}), PW_STATUS_INVALID_ARGUMENT);
  // connection_handle too large
  EXPECT_EQ(proxy.SendGattNotify(0x0FFF, 345, {}), PW_STATUS_INVALID_ARGUMENT);
}

// ########## NumberOfCompletedPacketsTest

TEST(NumberOfCompletedPacketsTest, TwoOfThreeSentPacketsComplete) {
  constexpr size_t kNumConnections = 3;
  struct {
    int sends_called = 0;
    const std::array<uint16_t, kNumConnections> connection_handles = {
        0x123, 0x456, 0x789};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 15ul);
        EXPECT_EQ(view.num_handles().Read(), capture.connection_handles.size());
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Proxy should have reclaimed 1 credit from Connection 0 (leaving 0
        // credits in packet), no credits from Connection 1 (meaning 0 will be
        // unchanged), and 1 credit from Connection 2 (leaving 0).
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handles[0]);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 0);

        EXPECT_EQ(view.nocp_data()[1].connection_handle().Read(),
                  capture.connection_handles[1]);
        EXPECT_EQ(view.nocp_data()[1].num_completed_packets().Read(), 0);

        EXPECT_EQ(view.nocp_data()[2].connection_handle().Read(),
                  capture.connection_handles[2]);
        EXPECT_EQ(view.nocp_data()[2].num_completed_packets().Read(), 0);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              kNumConnections);
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, kNumConnections));
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 3);

  // Send packet; num free packets should decrement.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[0],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);
  // Proxy host took all credits so will not pass NOCP on to host.
  EXPECT_EQ(capture.sends_called, 1);

  // Send packet over Connection 1, which will not have a packet completed in
  // the Number_of_Completed_Packets event.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[1],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);

  // Send third packet; num free packets should decrement again.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[2],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Number_of_Completed_Packets event that reports 1 packet on Connection
  // 0, 0 packets on Connection 1, and 1 packet on Connection 2. Checks in
  // send_to_host_fn will ensure we have reclaimed 2 of 3 credits.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 3>({{{capture.connection_handles[0], 1},
                                       {capture.connection_handles[1], 0},
                                       {capture.connection_handles[2], 1}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);
  // Proxy host took all credits so will not pass NOCP event on to host.
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(NumberOfCompletedPacketsTest, ManyMorePacketsCompletedThanPacketsPending) {
  constexpr size_t kNumConnections = 2;
  struct {
    int sends_called = 0;
    const std::array<uint16_t, kNumConnections> connection_handles = {0x123,
                                                                      0x456};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 11ul);
        EXPECT_EQ(view.num_handles().Read(), capture.connection_handles.size());
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Proxy should have reclaimed 1 credit from Connection 0 (leaving
        // 9 credits in packet) and 1 credit from Connection 2 (leaving 14).
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handles[0]);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 9);

        EXPECT_EQ(view.nocp_data()[1].connection_handle().Read(),
                  capture.connection_handles[1]);
        EXPECT_EQ(view.nocp_data()[1].num_completed_packets().Read(), 14);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 2));
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  // Send packet over Connection 0; num free packets should decrement.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[0],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);

  // Send packet over Connection 1; num free packets should decrement again.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[1],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Number_of_Completed_Packets event that reports 10 packets on
  // Connection 0 and 15 packets on Connection 1. Checks in send_to_host_fn
  // will ensure we have reclaimed exactly 2 credits, 1 from each Connection.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);
  EXPECT_EQ(capture.sends_called, 2);
}

TEST(NumberOfCompletedPacketsTest, ProxyReclaimsOnlyItsUsedCredits) {
  constexpr size_t kNumConnections = 2;
  struct {
    int sends_called = 0;
    const std::array<uint16_t, kNumConnections> connection_handles = {0x123,
                                                                      0x456};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 11ul);
        EXPECT_EQ(view.num_handles().Read(), 2);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Proxy has 4 credits it wants to reclaim, but it should have only
        // reclaimed the 2 credits it used on Connection 0.
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handles[0]);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 8);
        EXPECT_EQ(view.nocp_data()[1].connection_handle().Read(),
                  capture.connection_handles[1]);
        EXPECT_EQ(view.nocp_data()[1].num_completed_packets().Read(), 15);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 4);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 4));
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Use 2 credits on Connection 0 and 2 credits on random connections that will
  // not be included in the NOCP event.
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[0],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.connection_handles[0],
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_TRUE(proxy.SendGattNotify(0xABC, 1, pw::span(attribute_value)).ok());
  EXPECT_TRUE(proxy.SendGattNotify(0xBCD, 1, pw::span(attribute_value)).ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Number_of_Completed_Packets event that reports 10 packets on
  // Connection 0 and 15 packets on Connection 1. Checks in send_to_host_fn
  // will ensure we have reclaimed only 2 credits.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(capture.sends_called, 2);
}

TEST(NumberOfCompletedPacketsTest, EventUnmodifiedIfNoCreditsInUse) {
  constexpr size_t kNumConnections = 2;
  struct {
    int sends_called = 0;
    const std::array<uint16_t, kNumConnections> connection_handles = {0x123,
                                                                      0x456};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 11ul);
        EXPECT_EQ(view.num_handles().Read(), 2);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Event should be unmodified.
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handles[0]);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 10);
        EXPECT_EQ(view.nocp_data()[1].connection_handle().Read(),
                  capture.connection_handles[1]);
        EXPECT_EQ(view.nocp_data()[1].num_completed_packets().Read(), 15);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 10);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 10));
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event that reports 10 packets on
  // Connection 0 and 15 packets on Connection 1. Checks in send_to_host_fn
  // will ensure we have not modified the NOCP event.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 10);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(capture.sends_called, 2);
}

TEST(NumberOfCompletedPacketsTest, HandlesUnusualEvents) {
  constexpr size_t kNumConnections = 5;
  struct {
    int sends_called = 0;
    const std::array<uint16_t, kNumConnections> connection_handles = {
        0x123, 0x234, 0x345, 0x456, 0x567};
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        if (view.num_handles().Read() == 0) {
          return;
        }

        EXPECT_EQ(packet.GetHciSpan().size(), 23ul);
        EXPECT_EQ(view.num_handles().Read(), 5);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Event should be unmodified.
        for (int i = 0; i < 5; ++i) {
          EXPECT_EQ(view.nocp_data()[i].connection_handle().Read(),
                    capture.connection_handles[i]);
          EXPECT_EQ(view.nocp_data()[i].num_completed_packets().Read(), 0);
        }
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 10);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 10));
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event with no entries.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 0>({{}})));
  // NOCP has no entries, so will not be passed on to host.
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event that reports 0 packets for various
  // connections.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 5>({{{capture.connection_handles[0], 0},
                                       {capture.connection_handles[1], 0},
                                       {capture.connection_handles[2], 0},
                                       {capture.connection_handles[3], 0},
                                       {capture.connection_handles[4], 0}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 10);
  // Proxy host will not pass on a NOCP with no credits.
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(NumberOfCompletedPacketsTest, MultipleChannelsDifferentTransports) {
  static constexpr size_t kPayloadSize = 3;
  struct {
    int sends_called = 0;
    std::array<uint8_t, kPayloadSize> payload = {
        0xAB,
        0xCD,
        0xEF,
    };
  } capture;

  pw::Function<void(H4PacketWithHci&&)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4&&)>&& send_to_controller_fn(
      [&capture](H4PacketWithH4&&) { ++capture.sends_called; });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              /*le_acl_credits_to_reserve=*/1,
                              /*br_edr_acl_credits_to_reserve=*/1);
  // Allow proxy to reserve BR/EDR 1 credit.
  PW_TEST_EXPECT_OK(SendReadBufferResponseFromController(proxy, 1));
  // Allow proxy to reserve LE 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  // Test that sending on one type of transport doesn't get blocked if the other
  // type of transport is out of credits.

  L2capCoc le_channel =
      BuildCoc(proxy, CocParameters{.handle = 0x123, .tx_credits = 2});
  EXPECT_EQ(le_channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 1);

  RfcommChannel bredr_channel =
      BuildRfcomm(proxy, RfcommParameters{.handle = 0x456});
  EXPECT_EQ(bredr_channel.Write(capture.payload), PW_STATUS_OK);
  // Send should succeed even though no LE credits available
  EXPECT_EQ(capture.sends_called, 2);

  // Queue an LE write
  EXPECT_EQ(le_channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 2);

  // Complete previous LE write
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{0x123, 1}}})));
  EXPECT_EQ(capture.sends_called, 3);

  // Complete BR/EDR write
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{0x456, 1}}})));

  // Write again
  EXPECT_EQ(bredr_channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 4);
}

// ########## DisconnectionCompleteTest

TEST(DisconnectionCompleteTest, DisconnectionReclaimsCredits) {
  struct {
    int sends_called = 0;
    uint16_t connection_handle = 0x123;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 7ul);
        EXPECT_EQ(view.num_handles().Read(), 1);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Event should be unmodified.
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handle);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 10);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 10);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 10));
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Use up 3 of the 10 credits on the Connection that will be disconnected.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(proxy
                    .SendGattNotify(
                        capture.connection_handle, 1, pw::span(attribute_value))
                    .ok());
  }
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 7);
  // Use up 2 credits on a random Connection.
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(0x456, 1, pw::span(attribute_value)).ok());
  }
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 5);

  // Send Disconnection_Complete event, which should reclaim 3 credits.
  PW_TEST_EXPECT_OK(
      SendDisconnectionCompleteEvent(proxy, capture.connection_handle));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 8);

  // Use 1 credit and reclaim it on a bunch of random channels. Then send
  // disconnect and ensure it was cleaned up in connections list. The send will
  // fail if disconnect doesn't cleanup properly.
  //
  // We already have an active connection at this point in the test, so loop
  // over the remaining slots + 1 which would otherwise fail if cleanup wasn't
  // working right.
  for (uint16_t i = 0; i < kMaxProxyActiveConnections; ++i) {
    uint16_t handle = 0x234 + i;
    EXPECT_TRUE(
        proxy.SendGattNotify(handle, 1, pw::span(attribute_value)).ok());
    PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
        proxy, FlatMap<uint16_t, uint16_t, 1>({{{handle, 1}}})));
    EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 8);
    PW_TEST_EXPECT_OK(SendDisconnectionCompleteEvent(proxy, handle));
  }

  // Send Number_of_Completed_Packets event that reports 10 packets, none of
  // which should be reclaimed because this Connection has disconnected. Checks
  // in send_to_host_fn will ensure we have not modified the NOCP event.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{capture.connection_handle, 10}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 8);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(capture.sends_called, 13);
}

TEST(DisconnectionCompleteTest, FailedDisconnectionHasNoEffect) {
  uint16_t connection_handle = 0x123;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  std::array<uint8_t, 1> attribute_value = {0};

  // Use sole credit.
  EXPECT_TRUE(
      proxy.SendGattNotify(connection_handle, 1, pw::span(attribute_value))
          .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send failed Disconnection_Complete event, should not reclaim credit.
  PW_TEST_EXPECT_OK(
      SendDisconnectionCompleteEvent(proxy,
                                     connection_handle, /*successful=*/
                                     false));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
}

TEST(DisconnectionCompleteTest, DisconnectionOfUnusedConnectionHasNoEffect) {
  uint16_t connection_handle = 0x123;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  std::array<uint8_t, 1> attribute_value = {0};

  // Use sole credit.
  EXPECT_TRUE(
      proxy.SendGattNotify(connection_handle, 1, pw::span(attribute_value))
          .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Disconnection_Complete event to random Connection, should have no
  // effect.
  PW_TEST_EXPECT_OK(SendDisconnectionCompleteEvent(proxy, 0x456));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
}

TEST(DisconnectionCompleteTest, CanReuseConnectionHandleAfterDisconnection) {
  struct {
    int sends_called = 0;
    uint16_t connection_handle = 0x123;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 7ul);
        EXPECT_EQ(view.num_handles().Read(), 1);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Should have reclaimed the 1 packet.
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  capture.connection_handle);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 0);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Establish connection over `connection_handle`.
  EXPECT_TRUE(proxy
                  .SendGattNotify(
                      capture.connection_handle, 1, pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Disconnect `connection_handle`.
  PW_TEST_EXPECT_OK(
      SendDisconnectionCompleteEvent(proxy, capture.connection_handle));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);
  EXPECT_EQ(capture.sends_called, 2);

  // Re-establish connection over `connection_handle`.
  EXPECT_TRUE(proxy
                  .SendGattNotify(
                      capture.connection_handle, 1, pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Number_of_Completed_Packets event that reports 1 packet. Checks in
  // send_to_host_fn will ensure packet has been reclaimed.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{capture.connection_handle, 1}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);
  // Since proxy reclaimed the one credit, it does not pass event on to host.
  EXPECT_EQ(capture.sends_called, 2);
}

// ########## ResetTest

TEST(ResetTest, ResetClearsActiveConnections) {
  struct {
    int sends_called = 0;
    const uint16_t connection_handle = 0x123;
  } host_capture;
  struct {
    int sends_called = 0;
    const uint16_t connection_handle = 0x123;
  } controller_capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&host_capture](H4PacketWithHci&& packet) {
        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto event_header,
            MakeEmbossView<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes())));
        host_capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        PW_TEST_ASSERT_OK_AND_ASSIGN(
            auto view,
            MakeEmbossView<emboss::NumberOfCompletedPacketsEventView>(
                packet.GetHciSpan()));
        EXPECT_EQ(packet.GetHciSpan().size(), 7ul);
        EXPECT_EQ(view.num_handles().Read(), 1);
        EXPECT_EQ(view.header().event_code_enum().Read(),
                  emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS);

        // Should be unchanged.
        EXPECT_EQ(view.nocp_data()[0].connection_handle().Read(),
                  host_capture.connection_handle);
        EXPECT_EQ(view.nocp_data()[0].num_completed_packets().Read(), 1);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&controller_capture]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++controller_capture.sends_called;
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 2));
  EXPECT_EQ(host_capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};
  EXPECT_TRUE(proxy
                  .SendGattNotify(controller_capture.connection_handle,
                                  1,
                                  pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(controller_capture.sends_called, 1);

  proxy.Reset();

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  // Reset should not have cleared `le_acl_credits_to_reserve`, so proxy should
  // still indicate the capability.
  EXPECT_TRUE(proxy.HasSendLeAclCapability());

  // Re-initialize AclDataChannel with 2 credits.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 2));
  EXPECT_EQ(host_capture.sends_called, 2);

  // Send ACL on random handle to expend one credit.
  EXPECT_TRUE(proxy.SendGattNotify(1, 1, pw::span(attribute_value)).ok());
  EXPECT_EQ(controller_capture.sends_called, 2);
  // This should have no effect, as the reset has cleared our active connection
  // on this handle.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{host_capture.connection_handle, 1}}})));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(host_capture.sends_called, 3);
}

TEST(ResetTest, ProxyHandlesMultipleResets) {
  int sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  proxy.Reset();
  proxy.Reset();

  std::array<uint8_t, 1> attribute_value = {0};
  // Validate state after double reset.
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  EXPECT_TRUE(proxy.HasSendAclCapability());
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));
  EXPECT_EQ(proxy.SendGattNotify(1, 1, pw::span(attribute_value)),
            PW_STATUS_OK);
  EXPECT_EQ(sends_called, 1);

  proxy.Reset();

  // Validate state after third reset.
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  EXPECT_TRUE(proxy.HasSendAclCapability());
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));
  EXPECT_EQ(proxy.SendGattNotify(1, 1, pw::span(attribute_value)),
            PW_STATUS_OK);
  EXPECT_EQ(sends_called, 2);
}

// ########## MultiSendTest

TEST(MultiSendTest, CanOccupyAllThenReuseEachBuffer) {
  constexpr size_t kMaxSends = ProxyHost::GetNumSimultaneousAclSendsSupported();
  struct {
    size_t sends_called = 0;
    std::array<H4PacketWithH4, 2 * kMaxSends> released_packets;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        // Capture all packets to prevent their destruction.
        capture.released_packets[capture.sends_called++] = std::move(packet);
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              2 * kMaxSends);
  // Allow proxy to reserve enough credits to send twice the number of
  // simultaneous sends supported by proxy.
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, 2 * kMaxSends));

  std::array<uint8_t, 1> attribute_value = {0xF};
  // Occupy all send buffers.
  for (size_t i = 0; i < kMaxSends; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
  }
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), kMaxSends);
  EXPECT_EQ(proxy.SendGattNotify(123, 345, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);

  // Confirm we can release and reoccupy each buffer slot.
  for (size_t i = 0; i < kMaxSends; ++i) {
    capture.released_packets[i].~H4PacketWithH4();
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
    EXPECT_EQ(proxy.SendGattNotify(123, 345, pw::span(attribute_value)),
              PW_STATUS_UNAVAILABLE);
  }
  EXPECT_EQ(capture.sends_called, 2 * kMaxSends);

  // If captured packets are not reset here, they may destruct after the proxy
  // and lead to a crash when trying to lock the proxy's destructed mutex.
  for (auto& packet : capture.released_packets) {
    packet.ResetAndReturnReleaseFn();
  }
}

TEST(MultiSendTest, CanRepeatedlyReuseOneBuffer) {
  constexpr size_t kMaxSends = ProxyHost::GetNumSimultaneousAclSendsSupported();
  struct {
    size_t sends_called = 0;
    std::array<H4PacketWithH4, kMaxSends> released_packets;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        // Capture first kMaxSends packets linearly.
        if (capture.sends_called < capture.released_packets.size()) {
          capture.released_packets[capture.sends_called] = std::move(packet);
        } else {
          // Reuse only first packet slot after kMaxSends.
          capture.released_packets[0] = std::move(packet);
        }
        ++capture.sends_called;
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              2 * kMaxSends);
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, 2 * kMaxSends));

  std::array<uint8_t, 1> attribute_value = {0xF};
  // Occupy all send buffers.
  for (size_t i = 0; i < kMaxSends; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
  }

  // Repeatedly free and reoccupy first buffer.
  for (size_t i = 0; i < kMaxSends; ++i) {
    capture.released_packets[0].~H4PacketWithH4();
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
    EXPECT_EQ(proxy.SendGattNotify(123, 345, pw::span(attribute_value)),
              PW_STATUS_UNAVAILABLE);
  }
  EXPECT_EQ(capture.sends_called, 2 * kMaxSends);

  // If captured packets are not reset here, they may destruct after the proxy
  // and lead to a crash when trying to lock the proxy's destructed mutex.
  for (auto& packet : capture.released_packets) {
    packet.ResetAndReturnReleaseFn();
  }
}

TEST(MultiSendTest, CanSendOverManyDifferentConnections) {
  std::array<uint8_t, 1> attribute_value = {0xF};
  struct {
    uint16_t sends_called = 0;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++capture.sends_called;
      });

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              ProxyHost::GetMaxNumLeAclConnections());
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(
      proxy, ProxyHost::GetMaxNumLeAclConnections()));

  for (uint16_t send = 1; send <= ProxyHost::GetMaxNumLeAclConnections();
       send++) {
    // Use current send count as the connection handle.
    uint16_t conn_handle = send;
    EXPECT_TRUE(
        proxy.SendGattNotify(conn_handle, 345, pw::span(attribute_value)).ok());
    EXPECT_EQ(capture.sends_called, send);
  }
}

TEST(MultiSendTest, AttemptToSendOverMaxConnectionsFails) {
  constexpr uint16_t kSends = kMaxProxyActiveConnections + 1;
  std::array<uint8_t, 1> attribute_value = {0xF};
  struct {
    uint16_t sends_called = 0;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++capture.sends_called;
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), kSends);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, kSends));

  for (uint16_t send = 1; send <= kMaxProxyActiveConnections; send++) {
    // Use current send count as the connection handle.
    uint16_t conn_handle = send;
    EXPECT_TRUE(
        proxy.SendGattNotify(conn_handle, 345, pw::span(attribute_value)).ok());
    EXPECT_EQ(capture.sends_called, send);
  }

  // Last one should fail
  uint16_t conn_handle = kSends;
  EXPECT_FALSE(
      proxy.SendGattNotify(conn_handle, 345, pw::span(attribute_value)).ok());
  EXPECT_EQ(capture.sends_called, kMaxProxyActiveConnections);
}

TEST(MultiSendTest, ResetClearsBuffOccupiedFlags) {
  constexpr size_t kMaxSends = ProxyHost::GetNumSimultaneousAclSendsSupported();
  struct {
    size_t sends_called = 0;
    std::array<H4PacketWithH4, 2 * kMaxSends> released_packets;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) {
        // Capture all packets to prevent their destruction.
        capture.released_packets[capture.sends_called++] = std::move(packet);
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), kMaxSends);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, kMaxSends));

  std::array<uint8_t, 1> attribute_value = {0xF};
  // Occupy all send buffers.
  for (size_t i = 0; i < kMaxSends; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
  }

  proxy.Reset();
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, kMaxSends));

  // Although sent packets have not been released, proxy.Reset() should have
  // marked all buffers as unoccupied.
  for (size_t i = 0; i < kMaxSends; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
  }
  EXPECT_EQ(capture.sends_called, 2 * kMaxSends);

  // If captured packets are not reset here, they may destruct after the proxy
  // and lead to a crash when trying to lock the proxy's destructed mutex.
  for (auto& packet : capture.released_packets) {
    packet.ResetAndReturnReleaseFn();
  }
}

// ########## BasicL2capChannelTest

TEST(BasicL2capChannelTest, CannotCreateChannelWithInvalidArgs) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  // Connection handle too large by 1.
  EXPECT_EQ(proxy
                .AcquireBasicL2capChannel(/*connection_handle=*/0x0FFF,
                                          /*local_cid=*/0x123,
                                          /*type=*/AclTransport::kLe,
                                          /*controller_receive_fn=*/nullptr)
                .status(),
            PW_STATUS_INVALID_ARGUMENT);

  // Local CID invalid (0).
  EXPECT_EQ(proxy
                .AcquireBasicL2capChannel(/*connection_handle=*/0x123,
                                          /*local_cid=*/0,
                                          /*type=*/AclTransport::kLe,
                                          /*controller_receive_fn=*/nullptr)
                .status(),
            PW_STATUS_INVALID_ARGUMENT);
}

TEST(BasicL2capChannelTest, BasicRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  struct {
    int sends_called = 0;
    std::array<uint8_t, 3> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  uint16_t handle = 334;
  uint16_t local_cid = 443;
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      BasicL2capChannel channel,
      proxy.AcquireBasicL2capChannel(
          /*connection_handle=*/handle,
          /*local_cid=*/local_cid,
          /*transport=*/AclTransport::kLe,
          /*controller_receive_fn=*/
          [&capture](pw::span<uint8_t> payload) {
            ++capture.sends_called;
            EXPECT_TRUE(std::equal(payload.begin(),
                                   payload.end(),
                                   capture.expected_payload.begin(),
                                   capture.expected_payload.end()));
          }));

  std::array<uint8_t,
             emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                 emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
                 capture.expected_payload.size()>
      hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      capture.expected_payload.size());

  emboss::BFrameWriter bframe = emboss::MakeBFrameView(
      acl->payload().BackingStorage().data(), acl->payload().SizeInBytes());
  bframe.pdu_length().Write(capture.expected_payload.size());
  bframe.channel_id().Write(local_cid);
  std::copy(capture.expected_payload.begin(),
            capture.expected_payload.end(),
            hci_arr.begin() +
                emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
                emboss::BasicL2capHeader::IntrinsicSizeInBytes());

  // Send ACL data packet destined for the CoC we registered.
  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(capture.sends_called, 1);
}

// ########## L2capCocTest

TEST(L2capCocTest, CannotCreateChannelWithInvalidArgs) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  // Connection handle too large by 1.
  EXPECT_EQ(
      proxy
          .AcquireL2capCoc(
              /*connection_handle=*/0x0FFF,
              /*rx_config=*/
              {.cid = 0x123, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*tx_config=*/
              {.cid = 0x123, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*receive_fn=*/nullptr,
              /*event_fn=*/nullptr)
          .status(),
      PW_STATUS_INVALID_ARGUMENT);

  // Local CID invalid (0).
  EXPECT_EQ(
      proxy
          .AcquireL2capCoc(
              /*connection_handle=*/0x123,
              /*rx_config=*/
              {.cid = 0, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*tx_config=*/
              {.cid = 0x123, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*receive_fn=*/nullptr,
              /*event_fn=*/nullptr)
          .status(),
      PW_STATUS_INVALID_ARGUMENT);

  // Remote CID invalid (0).
  EXPECT_EQ(
      proxy
          .AcquireL2capCoc(
              /*connection_handle=*/0x123,
              /*rx_config=*/
              {.cid = 0x123, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*tx_config=*/
              {.cid = 0, .mtu = 0x123, .mps = 0x123, .credits = 0x123},
              /*receive_fn=*/nullptr,
              /*event_fn=*/nullptr)
          .status(),
      PW_STATUS_INVALID_ARGUMENT);
}

// ########## L2capCocWriteTest

// Size of sdu_length field in first K-frames.
constexpr uint8_t kSduLengthFieldSize = 2;
// Size of a K-Frame over Acl packet with no payload.
constexpr uint8_t kFirstKFrameOverAclMinSize =
    emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    emboss::FirstKFrame::MinSizeInBytes();

TEST(L2capCocWriteTest, BasicWrite) {
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

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  L2capCoc channel = BuildCoc(proxy,
                              CocParameters{.handle = capture.handle,
                                            .remote_cid = capture.channel_id});
  EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(L2capCocWriteTest, ErrorOnWriteToStoppedChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = 123,
          .tx_credits = 1,
          .event_fn = []([[maybe_unused]] L2capCoc::Event event) { FAIL(); }});

  EXPECT_EQ(channel.Stop(), PW_STATUS_OK);
  EXPECT_EQ(channel.Write({}), PW_STATUS_FAILED_PRECONDITION);
}

TEST(L2capCocWriteTest, TooLargeWritesFail) {
  int sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(proxy, 1));

  // Payload size exceeds MTU.
  L2capCoc small_mtu_channel = BuildCoc(proxy, CocParameters{.tx_mtu = 1});
  std::array<uint8_t, 24> payload;
  EXPECT_EQ(small_mtu_channel.Write(payload), PW_STATUS_INVALID_ARGUMENT);

  // Payload size exceeds MPS.
  L2capCoc small_mps_channel = BuildCoc(proxy, CocParameters{.tx_mps = 23});
  EXPECT_EQ(small_mps_channel.Write(payload), PW_STATUS_INVALID_ARGUMENT);

  // Payload size exceeds max allowable based on H4 buffer size.
  std::array<uint8_t,
             proxy.GetMaxAclSendSize() - kFirstKFrameOverAclMinSize + 1>
      payload_one_byte_too_large;
  L2capCoc channel = BuildCoc(proxy, {});
  EXPECT_EQ(channel.Write(payload_one_byte_too_large),
            PW_STATUS_INVALID_ARGUMENT);

  EXPECT_EQ(sends_called, 0);
}

TEST(L2capCocWriteTest, MultipleWritesSameChannel) {
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
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), num_writes);
  PW_TEST_EXPECT_OK(SendLeReadBufferResponseFromController(
      proxy,
      /*num_credits_to_reserve=*/num_writes));

  L2capCoc channel = BuildCoc(proxy, CocParameters{.tx_credits = num_writes});
  for (int i = 0; i < num_writes; ++i) {
    EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, num_writes);
}

TEST(L2capCocWriteTest, MultipleWritesMultipleChannels) {
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
                              kNumChannels);
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
    EXPECT_EQ(channels[i].Write(capture.payload), PW_STATUS_OK);
    std::for_each(capture.payload.begin(),
                  capture.payload.end(),
                  [](uint8_t& byte) { ++byte; });
  }

  EXPECT_EQ(capture.sends_called, kNumChannels);
}

// ########## L2capCocReadTest

TEST(L2capCocReadTest, BasicRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, ChannelHandlesReadWithNullReceiveFn) {
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
      CocParameters{
          .handle = handle,
          .local_cid = local_cid,
          .rx_credits = 1,
          .event_fn = []([[maybe_unused]] L2capCoc::Event event) { FAIL(); }});

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

TEST(L2capCocReadTest, ErrorOnRxToStoppedChannel) {
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
              [&events_received](L2capCoc::Event event) {
                ++events_received;
                EXPECT_EQ(event, L2capCoc::Event::kRxWhileStopped);
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

  EXPECT_EQ(channel.Stop(), PW_STATUS_OK);
  for (int i = 0; i < num_invalid_rx; ++i) {
    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    proxy.HandleH4HciFromController(std::move(h4_packet));
  }
  EXPECT_EQ(events_received, num_invalid_rx);
}

TEST(L2capCocReadTest, TooShortAclPassedToHost) {
  int sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithHci&& packet) {
        ++sends_called;
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, ChannelClosedWithErrorIfMtuExceeded) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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
              [&events_received](L2capCoc::Event event) {
                ++events_received;
                EXPECT_EQ(event, L2capCoc::Event::kRxInvalid);
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

TEST(L2capCocReadTest, ChannelClosedWithErrorIfMpsExceeded) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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
              [&events_received](L2capCoc::Event event) {
                ++events_received;
                EXPECT_EQ(event, L2capCoc::Event::kRxInvalid);
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

TEST(L2capCocReadTest, ChannelClosedWithErrorIfPayloadsExceedSduLength) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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
              [&events_received](L2capCoc::Event event) {
                ++events_received;
                EXPECT_EQ(event, L2capCoc::Event::kRxInvalid);
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

TEST(L2capCocReadTest, NoReadOnStoppedChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

  EXPECT_EQ(channel.Stop(), PW_STATUS_OK);
  proxy.HandleH4HciFromController(std::move(h4_packet));
}

TEST(L2capCocReadTest, NoReadOnSameCidDifferentConnectionHandle) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, MultipleReadsSameChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, MultipleReadsMultipleChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, ChannelStoppageDoNotAffectOtherChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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
  EXPECT_EQ(channels[1].Stop(), PW_STATUS_OK);
  EXPECT_EQ(channels[3].Stop(), PW_STATUS_OK);

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

TEST(L2capCocReadTest, ReadDropsSduSentOver2Segments) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, ReadDropsSduSentOver4Segments) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, NonCocAclPacketPassesThroughToHost) {
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
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, FragmentedPduStopsChannelWithoutDisruptingOtherChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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
              [&events_called](L2capCoc::Event event) {
                EXPECT_EQ(event, L2capCoc::Event::kRxFragmented);
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
          .event_fn = []([[maybe_unused]] L2capCoc::Event event) { FAIL(); }});

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

TEST(L2capCocReadTest, UnexpectedContinuingFragmentStopsChannel) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [](H4PacketWithHci&&) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  int events_received = 0;
  uint16_t handle = 123;
  uint16_t local_cid = 234;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{.handle = handle,
                    .local_cid = local_cid,
                    .receive_fn = [](span<uint8_t>) { FAIL(); },
                    .event_fn =
                        [&events_received](L2capCoc::Event event) {
                          ++events_received;
                          EXPECT_EQ(event, L2capCoc::Event::kRxFragmented);
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

TEST(L2capCocReadTest, AclFrameWithIncompleteL2capHeaderForwardedToHost) {
  int sends_to_host_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&sends_to_host_called]([[maybe_unused]] H4PacketWithHci&& packet) {
        ++sends_to_host_called;
      });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocReadTest, FragmentedPduDoesNotInterfereWithOtherChannels) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

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

TEST(L2capCocQueueTest, ReadBufferResponseDrainsQueue) {
  size_t sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());

  L2capCoc channel =
      BuildCoc(proxy, CocParameters{.tx_credits = L2capCoc::QueueCapacity()});

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    EXPECT_EQ(channel.Write({}), PW_STATUS_OK);
  }
  EXPECT_EQ(channel.Write({}), PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(sends_called, 0u);

  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));

  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());
}

TEST(L2capCocQueueTest, NocpEventDrainsQueue) {
  size_t sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));

  uint16_t handle = 123;
  L2capCoc channel =
      BuildCoc(proxy,
               CocParameters{.handle = handle,
                             .tx_credits = 2 * L2capCoc::QueueCapacity()});

  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    EXPECT_EQ(channel.Write({}), PW_STATUS_OK);
  }

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    EXPECT_EQ(channel.Write({}), PW_STATUS_OK);
  }
  EXPECT_EQ(channel.Write({}), PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());

  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{handle, L2capCoc::QueueCapacity()}}})));

  EXPECT_EQ(sends_called, 2 * L2capCoc::QueueCapacity());
}

TEST(L2capCocQueueTest, RemovingLrdWriteChannelDoesNotInvalidateRoundRobin) {
  size_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());
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

  // We have 3 channels. Make it so LRD write channel iterator is on the
  // middle channel, then release that channel and ensure the other two are
  // still reached in the round robin.

  // Queue a packet in middle channel.
  for (size_t i = 0; i < L2capCoc::QueueCapacity() + 1; ++i) {
    EXPECT_EQ(chan_middle->Write({}), PW_STATUS_OK);
  }
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());

  // Make middle channel the LRD write channel.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{handle, 1}}})));
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 1);

  // Queue a packet each in left and right channels.
  EXPECT_EQ(chan_left.Write({}), PW_STATUS_OK);
  EXPECT_EQ(chan_right.Write({}), PW_STATUS_OK);
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 1);

  // Drop middle channel. LRD write channel iterator should still be valid.
  chan_middle.reset();

  // Confirm packets in remaining two channels are sent in round robin.
  PW_TEST_EXPECT_OK(SendNumberOfCompletedPackets(
      proxy, FlatMap<uint16_t, uint16_t, 1>({{{handle, 2}}})));
  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity() + 3);
}

// ########## L2capSignalingTest

TEST(L2capSignalingTest, FlowControlCreditIndDrainsQueue) {
  size_t sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [&sends_called]([[maybe_unused]] H4PacketWithH4&& packet) {
        ++sends_called;
      });
  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());
  PW_TEST_EXPECT_OK(
      SendLeReadBufferResponseFromController(proxy, L2capCoc::QueueCapacity()));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), L2capCoc::QueueCapacity());

  uint16_t handle = 123;
  uint16_t remote_cid = 456;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle, .remote_cid = remote_cid, .tx_credits = 0});

  for (size_t i = 0; i < L2capCoc::QueueCapacity(); ++i) {
    EXPECT_EQ(channel.Write({}), PW_STATUS_OK);
  }
  EXPECT_EQ(channel.Write({}), PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(sends_called, 0u);

  constexpr size_t kL2capLength =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes();
  constexpr size_t kHciLength =
      emboss::AclDataFrame::MinSizeInBytes() + kL2capLength;
  std::array<uint8_t, kHciLength> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci flow_control_credit_ind{emboss::H4PacketType::ACL_DATA,
                                          pw::span(hci_arr.data(), kHciLength)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(kL2capLength);

  emboss::CFrameWriter l2cap = emboss::MakeCFrameView(
      acl->payload().BackingStorage().data(), kL2capLength);
  l2cap.pdu_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  // 0x0005 = LE-U fixed signaling channel ID.
  l2cap.channel_id().Write(0x0005);

  emboss::L2capFlowControlCreditIndWriter ind =
      emboss::MakeL2capFlowControlCreditIndView(
          l2cap.payload().BackingStorage().data(),
          emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  ind.command_header().code().Write(
      emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND);
  ind.command_header().data_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes() -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  ind.cid().Write(remote_cid);
  ind.credits().Write(L2capCoc::QueueCapacity());

  proxy.HandleH4HciFromController(std::move(flow_control_credit_ind));

  EXPECT_EQ(sends_called, L2capCoc::QueueCapacity());
}

TEST(L2capSignalingTest, ChannelClosedWithErrorIfCreditsExceeded) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());

  uint16_t handle = 123;
  uint16_t remote_cid = 456;
  int events_received = 0;
  L2capCoc channel = BuildCoc(
      proxy,
      CocParameters{
          .handle = handle,
          .remote_cid = remote_cid,
          // Initialize with max credit count.
          .tx_credits =
              emboss::L2capLeCreditBasedConnectionReq::max_credit_value(),
          .event_fn = [&events_received](L2capCoc::Event event) {
            EXPECT_EQ(event, L2capCoc::Event::kRxInvalid);
            ++events_received;
          }});

  constexpr size_t kL2capLength =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes();
  constexpr size_t kHciLength =
      emboss::AclDataFrame::MinSizeInBytes() + kL2capLength;
  std::array<uint8_t, kHciLength> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci flow_control_credit_ind{emboss::H4PacketType::ACL_DATA,
                                          pw::span(hci_arr.data(), kHciLength)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(kL2capLength);

  emboss::CFrameWriter l2cap =
      emboss::MakeCFrameView(acl->payload().BackingStorage().data(),
                             emboss::BasicL2capHeader::IntrinsicSizeInBytes());
  l2cap.pdu_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  // 0x0005 = LE-U fixed signaling channel ID.
  l2cap.channel_id().Write(0x0005);

  emboss::L2capFlowControlCreditIndWriter ind =
      emboss::MakeL2capFlowControlCreditIndView(
          l2cap.payload().BackingStorage().data(),
          emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  ind.command_header().code().Write(
      emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND);
  ind.command_header().data_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes() -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  ind.cid().Write(remote_cid);
  // Exceed max credit count by 1.
  ind.credits().Write(1);

  proxy.HandleH4HciFromController(std::move(flow_control_credit_ind));

  EXPECT_EQ(events_received, 1);
}

TEST(L2capSignalingTest, CreditIndAddressedToNonManagedChannelForwardedToHost) {
  int forwards_to_host = 0;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&forwards_to_host](H4PacketWithHci&&) { ++forwards_to_host; });
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      [](H4PacketWithH4&&) {});

  ProxyHost proxy = ProxyHost(std::move(send_to_host_fn),
                              std::move(send_to_controller_fn),
                              L2capCoc::QueueCapacity());

  uint16_t handle = 123;
  uint16_t remote_cid = 456;
  L2capCoc channel = BuildCoc(
      proxy, CocParameters{.handle = handle, .remote_cid = remote_cid});

  constexpr size_t kL2capLength =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes();
  constexpr size_t kHciLength =
      emboss::AclDataFrame::MinSizeInBytes() + kL2capLength;
  std::array<uint8_t, kHciLength> hci_arr;
  hci_arr.fill(0);
  H4PacketWithHci flow_control_credit_ind{emboss::H4PacketType::ACL_DATA,
                                          pw::span(hci_arr.data(), kHciLength)};

  Result<emboss::AclDataFrameWriter> acl =
      MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr);
  acl->header().handle().Write(handle);
  acl->data_total_length().Write(kL2capLength);

  emboss::CFrameWriter l2cap =
      emboss::MakeCFrameView(acl->payload().BackingStorage().data(),
                             emboss::BasicL2capHeader::IntrinsicSizeInBytes());
  l2cap.pdu_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  // 0x0005 = LE-U fixed signaling channel ID.
  l2cap.channel_id().Write(0x0005);

  emboss::L2capFlowControlCreditIndWriter ind =
      emboss::MakeL2capFlowControlCreditIndView(
          l2cap.payload().BackingStorage().data(),
          emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes());
  ind.command_header().code().Write(
      emboss::L2capSignalingPacketCode::FLOW_CONTROL_CREDIT_IND);
  ind.command_header().data_length().Write(
      emboss::L2capFlowControlCreditInd::IntrinsicSizeInBytes() -
      emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
  // Address packet to different CID on same connection.
  ind.cid().Write(remote_cid + 1);

  proxy.HandleH4HciFromController(std::move(flow_control_credit_ind));

  EXPECT_EQ(forwards_to_host, 1);
}

// ########## AcluSignalingChannelTest

TEST(AcluSignalingChannelTest, HandlesMultipleCommands) {
  std::optional<H4PacketWithHci> host_packet;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&host_packet](H4PacketWithHci&& packet) {
        host_packet = std::move(packet);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);

  constexpr uint16_t kHandle = 123;

  // Test that the proxy can parse a CFrame containing multiple commands and
  // pass it through. We pack 3 CONNECTION_REQ commands into one CFrame.
  constexpr size_t kNumCommands = 3;
  constexpr size_t kCmdLen = emboss::L2capConnectionReq::IntrinsicSizeInBytes();
  constexpr size_t kL2capLength =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + kCmdLen * kNumCommands;
  constexpr size_t kHciLength =
      emboss::AclDataFrame::MinSizeInBytes() + kL2capLength;
  std::array<uint8_t, kHciLength> hci_arr{};
  H4PacketWithHci l2cap_cframe_packet{emboss::H4PacketType::ACL_DATA,
                                      pw::span(hci_arr.data(), kHciLength)};

  // ACL header
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
  acl.header().handle().Write(kHandle);
  acl.data_total_length().Write(kL2capLength);
  EXPECT_EQ(kL2capLength, acl.payload().BackingStorage().SizeInBytes());

  // L2CAP header
  auto l2cap =
      emboss::MakeCFrameView(acl.payload().BackingStorage().data(),
                             acl.payload().BackingStorage().SizeInBytes());
  l2cap.pdu_length().Write(kNumCommands * kCmdLen);
  l2cap.channel_id().Write(
      cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING));
  EXPECT_TRUE(l2cap.Ok());

  auto command_buffer =
      pw::span(l2cap.payload().BackingStorage().data(),
               l2cap.payload().BackingStorage().SizeInBytes());
  EXPECT_EQ(l2cap.payload().BackingStorage().SizeInBytes(),
            kCmdLen * kNumCommands);

  do {
    // CONNECTION_REQ
    auto cmd_writer = emboss::MakeL2capConnectionReqView(command_buffer.data(),
                                                         command_buffer.size());
    cmd_writer.command_header().code().Write(
        emboss::L2capSignalingPacketCode::CONNECTION_REQ);
    // Note data_length doesn't include command header.
    cmd_writer.command_header().data_length().Write(
        kCmdLen - emboss::L2capSignalingCommandHeader::IntrinsicSizeInBytes());
    cmd_writer.psm().Write(1);
    cmd_writer.source_cid().Write(1);
    EXPECT_TRUE(cmd_writer.Ok());
    EXPECT_EQ(cmd_writer.SizeInBytes(), kCmdLen);
    command_buffer = command_buffer.subspan(cmd_writer.SizeInBytes());
  } while (!command_buffer.empty());

  proxy.HandleH4HciFromController(std::move(l2cap_cframe_packet));
  // We should get back what we sent, since the proxy doesn't consume
  // CONNECTION_REQ commands. It would be nice to also verify the individual
  // commands were parsed out but hooks don't exist for that at the time of
  // writing.
  EXPECT_TRUE(host_packet.has_value());
  EXPECT_EQ(host_packet->GetHciSpan().size(), kHciLength);
}

TEST(AcluSignalingChannelTest, InvalidPacketForwarded) {
  std::optional<H4PacketWithHci> host_packet;
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      [&host_packet](H4PacketWithHci&& packet) {
        host_packet = std::move(packet);
      });
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);

  constexpr uint16_t kHandle = 123;

  // Test that the proxy forwards on invalid L2cap B-frames destined for
  // signaling channel.

  constexpr size_t kL2capLength =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes();
  constexpr size_t kHciLength =
      emboss::AclDataFrame::MinSizeInBytes() + kL2capLength;
  std::array<uint8_t, kHciLength> hci_arr{};
  H4PacketWithHci l2cap_cframe_packet{emboss::H4PacketType::ACL_DATA,
                                      pw::span(hci_arr.data(), kHciLength)};

  // ACL header
  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
  acl.header().handle().Write(kHandle);
  acl.data_total_length().Write(kL2capLength);
  EXPECT_EQ(kL2capLength, acl.payload().BackingStorage().SizeInBytes());

  // L2CAP header
  auto l2cap =
      emboss::MakeCFrameView(acl.payload().BackingStorage().data(),
                             acl.payload().BackingStorage().SizeInBytes());
  // Invalid length, since we aren't encoding a payload.
  l2cap.pdu_length().Write(1);
  l2cap.channel_id().Write(
      cpp23::to_underlying(emboss::L2capFixedCid::ACL_U_SIGNALING));
  EXPECT_FALSE(l2cap.Ok());

  proxy.HandleH4HciFromController(std::move(l2cap_cframe_packet));
  // We should get back what we sent.
  EXPECT_TRUE(host_packet.has_value());
  EXPECT_EQ(host_packet->GetHciSpan().size(), kHciLength);
}

// ########## RfcommWriteTest

constexpr uint8_t kBFrameOverAclMinSize =
    emboss::AclDataFrameHeader::IntrinsicSizeInBytes() +
    emboss::BFrame::MinSizeInBytes();

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

  constexpr uint8_t kMaxCredits = 10;
  constexpr uint16_t kMaxFrameSize = 900;
  constexpr uint8_t kRfcommChannel = 0x03;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto channel,
      proxy.AcquireRfcommChannel(
          capture.handle,
          /*rx_config=*/
          RfcommChannel::Config{.cid = capture.channel_id,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          /*tx_config*/
          RfcommChannel::Config{.cid = capture.channel_id,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          kRfcommChannel,
          nullptr));
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

  constexpr uint8_t kMaxCredits = 10;
  constexpr uint16_t kMaxFrameSize = 900;
  constexpr uint8_t kRfcommChannel = 0x03;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto channel,
      proxy.AcquireRfcommChannel(
          capture.handle,
          /*rx_config=*/
          RfcommChannel::Config{.cid = capture.channel_id,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          /*tx_config*/
          RfcommChannel::Config{.cid = capture.channel_id,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          kRfcommChannel,
          nullptr));
  EXPECT_EQ(channel.Write(capture.payload), PW_STATUS_OK);
  EXPECT_EQ(capture.sends_called, 1);
}

// ########## RfcommReadTest

TEST(RfcommReadTest, BasicRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  struct {
    int rx_called = 0;
    std::array<uint8_t, 3> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr uint8_t kExpectedFcs = 0xFA;
  constexpr uint16_t kHandle = 123;
  constexpr uint16_t kCid = 234;
  constexpr uint8_t kMaxCredits = 10;
  constexpr uint16_t kMaxFrameSize = 900;
  constexpr uint8_t kRfcommChannel = 0x03;
  const size_t frame_size = emboss::RfcommFrame::MinSizeInBytes() +
                            /*payload*/ capture.expected_payload.size();
  const size_t acl_data_length =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + frame_size;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto channel,
      proxy.AcquireRfcommChannel(
          kHandle,
          /*rx_config=*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          /*tx_config*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          kRfcommChannel,
          [&capture](pw::span<uint8_t> payload) {
            ++capture.rx_called;
            EXPECT_TRUE(std::equal(payload.begin(),
                                   payload.end(),
                                   capture.expected_payload.begin(),
                                   capture.expected_payload.end()));
          }));

  std::array<uint8_t, kBFrameOverAclMinSize + frame_size> hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
  acl.header().handle().Write(kHandle);
  acl.data_total_length().Write(acl_data_length);

  auto bframe = emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                                       acl.data_total_length().Read());
  bframe.pdu_length().Write(frame_size);
  bframe.channel_id().Write(kCid);

  auto rfcomm = emboss::MakeRfcommFrameView(
      bframe.payload().BackingStorage().data(), bframe.payload().SizeInBytes());
  rfcomm.extended_address().Write(true);
  rfcomm.command_response_direction().Write(
      emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
  rfcomm.channel().Write(kRfcommChannel);

  rfcomm.control().Write(
      emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);

  rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
  rfcomm.length().Write(capture.expected_payload.size());

  std::memcpy(rfcomm.information().BackingStorage().data(),
              capture.expected_payload.data(),
              capture.expected_payload.size());
  rfcomm.fcs().Write(kExpectedFcs);

  // Send ACL data packet destined for the channel we registered.
  proxy.HandleH4HciFromController(std::move(h4_packet));

  EXPECT_EQ(capture.rx_called, 1);
}

TEST(RfcommReadTest, ExtendedRead) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)>&& send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  constexpr size_t kPayloadSize = 0x80;
  struct {
    int rx_called = 0;
    std::array<uint8_t, kPayloadSize> expected_payload = {0xAB, 0xCD, 0xEF};
  } capture;

  constexpr uint8_t kExpectedFcs = 0xFA;
  constexpr uint16_t kHandle = 123;
  constexpr uint16_t kCid = 234;
  constexpr uint8_t kMaxCredits = 10;
  constexpr uint16_t kMaxFrameSize = 900;
  constexpr uint8_t kRfcommChannel = 0x03;

  const size_t frame_size = emboss::RfcommFrame::MinSizeInBytes() +
                            /*length_extended*/ 1 +
                            /*payload*/ capture.expected_payload.size();
  const size_t acl_data_length =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + frame_size;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto channel,
      proxy.AcquireRfcommChannel(
          kHandle,
          /*rx_config=*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          /*tx_config*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          kRfcommChannel,
          [&capture](pw::span<uint8_t> payload) {
            ++capture.rx_called;
            EXPECT_TRUE(std::equal(payload.begin(),
                                   payload.end(),
                                   capture.expected_payload.begin(),
                                   capture.expected_payload.end()));
          }));

  std::array<uint8_t, kBFrameOverAclMinSize + frame_size> hci_arr{};
  H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
  acl.header().handle().Write(kHandle);
  acl.data_total_length().Write(acl_data_length);

  auto bframe = emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                                       acl.data_total_length().Read());
  bframe.pdu_length().Write(frame_size);
  bframe.channel_id().Write(kCid);

  auto rfcomm = emboss::MakeRfcommFrameView(
      bframe.payload().BackingStorage().data(), bframe.payload().SizeInBytes());
  rfcomm.extended_address().Write(true);
  rfcomm.command_response_direction().Write(
      emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
  rfcomm.channel().Write(kRfcommChannel);

  rfcomm.control().Write(
      emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);

  rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::EXTENDED);
  rfcomm.length_extended().Write(capture.expected_payload.size());

  std::memcpy(rfcomm.information().BackingStorage().data(),
              capture.expected_payload.data(),
              capture.expected_payload.size());
  rfcomm.fcs().Write(kExpectedFcs);

  // Send ACL data packet destined for the channel we registered.
  proxy.HandleH4HciFromController(std::move(h4_packet));

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
  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  constexpr uint8_t kExpectedFcs = 0xFA;
  constexpr uint8_t kInvalidFcs = 0xFF;
  constexpr uint16_t kHandle = 123;
  constexpr uint16_t kCid = 234;
  constexpr uint8_t kMaxCredits = 10;
  constexpr uint16_t kMaxFrameSize = 900;
  constexpr uint8_t kRfcommChannel = 0x03;
  const size_t frame_size = emboss::RfcommFrame::MinSizeInBytes() +
                            /*payload*/ 0;
  const size_t acl_data_length =
      emboss::BasicL2capHeader::IntrinsicSizeInBytes() + frame_size;

  PW_TEST_ASSERT_OK_AND_ASSIGN(
      auto channel,
      proxy.AcquireRfcommChannel(
          kHandle,
          /*rx_config=*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          /*tx_config*/
          RfcommChannel::Config{.cid = kCid,
                                .max_information_length = kMaxFrameSize,
                                .credits = kMaxCredits},
          kRfcommChannel,
          [&capture](pw::span<uint8_t>) { ++capture.rx_called; }));

  std::array<uint8_t, kBFrameOverAclMinSize + frame_size> hci_arr{};

  // Construct valid packet but put invalid checksum on the end. Test that we
  // don't get it sent on to us.
  {
    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};
    PW_TEST_ASSERT_OK_AND_ASSIGN(
        auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
    acl.header().handle().Write(kHandle);
    acl.data_total_length().Write(acl_data_length);

    auto bframe = emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                                         acl.data_total_length().Read());
    bframe.pdu_length().Write(frame_size);
    bframe.channel_id().Write(kCid);

    auto rfcomm =
        emboss::MakeRfcommFrameView(bframe.payload().BackingStorage().data(),
                                    bframe.payload().SizeInBytes());
    rfcomm.extended_address().Write(true);
    rfcomm.command_response_direction().Write(
        emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
    rfcomm.channel().Write(kRfcommChannel);

    rfcomm.control().Write(
        emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);

    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
    rfcomm.length().Write(0);
    rfcomm.fcs().Write(kInvalidFcs);

    proxy.HandleH4HciFromController(std::move(h4_packet));
  }

  EXPECT_EQ(capture.rx_called, 0);
  EXPECT_EQ(capture.host_called, 1);

  // Construct packet with everything valid but wrong length for actual data
  // size. Ensure it doesn't end up being sent to our channel, but does get
  // forwarded to host.
  {
    H4PacketWithHci h4_packet{emboss::H4PacketType::ACL_DATA, hci_arr};

    PW_TEST_ASSERT_OK_AND_ASSIGN(
        auto acl, MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_arr));
    acl.header().handle().Write(kHandle);
    acl.data_total_length().Write(acl_data_length);

    auto bframe = emboss::MakeBFrameView(acl.payload().BackingStorage().data(),
                                         acl.data_total_length().Read());
    bframe.pdu_length().Write(frame_size);
    bframe.channel_id().Write(kCid);

    auto rfcomm =
        emboss::MakeRfcommFrameView(bframe.payload().BackingStorage().data(),
                                    bframe.payload().SizeInBytes());
    rfcomm.extended_address().Write(true);
    rfcomm.command_response_direction().Write(
        emboss::RfcommCommandResponseAndDirection::COMMAND_FROM_INITIATOR);
    rfcomm.channel().Write(kRfcommChannel);

    rfcomm.control().Write(
        emboss::RfcommFrameType::UNNUMBERED_INFORMATION_WITH_HEADER_CHECK);

    rfcomm.length_extended_flag().Write(emboss::RfcommLengthExtended::NORMAL);
    // Invalid length.
    rfcomm.length().Write(1);
    // Can't Write FCS as emboss will assert because of invalid length. Place
    // manually.
    hci_arr[hci_arr.size() - 1] = kExpectedFcs;

    proxy.HandleH4HciFromController(std::move(h4_packet));
  }

  EXPECT_EQ(capture.rx_called, 0);
  EXPECT_EQ(capture.host_called, 2);
}

// TODO: https://pwbug.dev/360929142 - Add many more tests exercising queueing +
// credit-based control flow.

}  // namespace
}  // namespace pw::bluetooth::proxy
