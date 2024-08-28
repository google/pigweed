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
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/emboss_util.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_containers/flat_map.h"
#include "pw_function/function.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

using containers::FlatMap;

// ########## Util functions

// Populate passed H4 command buffer and return Emboss view on it.
template <typename EmbossT>
EmbossT CreateAndPopulateToControllerView(H4PacketWithH4& h4_packet,
                                          emboss::OpCode opcode) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 100);
  h4_packet.SetH4Type(emboss::H4PacketType::COMMAND);
  EmbossT view = MakeEmboss<EmbossT>(h4_packet.GetHciSpan());
  EXPECT_TRUE(view.IsComplete());
  view.header().opcode_enum().Write(opcode);
  return view;
}

// Return a populated H4 command buffer of a type that proxy host doesn't
// interact with.
void PopulateNoninteractingToControllerBuffer(H4PacketWithH4& h4_packet) {
  CreateAndPopulateToControllerView<emboss::InquiryCommandWriter>(
      h4_packet, emboss::OpCode::LINK_KEY_REQUEST_REPLY);
}

// Populate passed H4 event buffer and return Emboss view on it.
template <typename EmbossT>
EmbossT CreateAndPopulateToHostEventView(H4PacketWithHci& h4_packet,
                                         emboss::EventCode event_code) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 0x10);
  h4_packet.SetH4Type(emboss::H4PacketType::EVENT);
  EmbossT view = MakeEmboss<EmbossT>(h4_packet.GetHciSpan());
  view.header().event_code_enum().Write(event_code);
  view.status().Write(emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(view.IsComplete());
  return view;
}

// Send an LE_Read_Buffer_Size (V2) CommandComplete event to `proxy` to request
// the reservation of a number of LE ACL send credits.
void SendReadBufferResponseFromController(ProxyHost& proxy,
                                          uint8_t num_credits_to_reserve) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV2CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
  view.total_num_le_acl_data_packets().Write(num_credits_to_reserve);

  proxy.HandleH4HciFromController(std::move(h4_packet));
}

// Send a Number_of_Completed_Packets event to `proxy` that reports each
// {connection handle, number of completed packets} entry provided.
template <size_t kNumConnections>
void SendNumberOfCompletedPackets(
    ProxyHost& proxy,
    FlatMap<uint16_t, uint16_t, kNumConnections> packets_per_connection) {
  std::array<
      uint8_t,
      emboss::NumberOfCompletedPacketsEvent::MinSizeInBytes() +
          kNumConnections *
              emboss::NumberOfCompletedPacketsEventData::IntrinsicSizeInBytes()>
      hci_arr;
  H4PacketWithHci nocp_event{emboss::H4PacketType::EVENT, hci_arr};
  auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventWriter>(
      nocp_event.GetHciSpan());
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
}

// Send a Disconnection_Complete event to `proxy` indicating the provided
// `handle` has disconnected.
void SendDisconnectionCompleteEvent(ProxyHost& proxy,
                                    uint16_t handle,
                                    bool successful = true) {
  std::array<uint8_t,
             emboss::DisconnectionCompleteEvent::IntrinsicSizeInBytes()>
      hci_arr_dc;
  H4PacketWithHci dc_event{emboss::H4PacketType::EVENT, hci_arr_dc};
  auto view = MakeEmboss<emboss::DisconnectionCompleteEventWriter>(
      dc_event.GetHciSpan());
  view.header().event_code_enum().Write(
      emboss::EventCode::DISCONNECTION_COMPLETE);
  view.status().Write(successful ? emboss::StatusCode::SUCCESS
                                 : emboss::StatusCode::HARDWARE_FAILURE);
  view.connection_handle().Write(handle);
  proxy.HandleH4HciFromController(std::move(dc_event));
}

// Return a populated H4 event buffer of a type that proxy host doesn't interact
// with.
void CreateNonInteractingToHostBuffer(H4PacketWithHci& h4_packet) {
  CreateAndPopulateToHostEventView<emboss::InquiryCompleteEventWriter>(
      h4_packet, emboss::EventCode::INQUIRY_COMPLETE);
}

// ########## Examples

// Example for docs.rst.
TEST(Example, ExampleUsage) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      h4_array_from_host;
  H4PacketWithH4 h4_packet_from_host{emboss::H4PacketType::UNKNOWN,
                                     h4_array_from_host};
  PopulateNoninteractingToControllerBuffer(h4_packet_from_host);

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      hci_array_from_controller;
  H4PacketWithHci h4_packet_from_controller{emboss::H4PacketType::UNKNOWN,
                                            hci_array_from_controller};

  CreateNonInteractingToHostBuffer(h4_packet_from_controller);

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
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr;
  H4PacketWithH4 h4_packet{emboss::H4PacketType::UNKNOWN, h4_arr};
  PopulateNoninteractingToControllerBuffer(h4_packet);

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
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  CreateNonInteractingToHostBuffer(h4_packet);

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
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::ReadLocalVersionInfoCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
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
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_arr;
  H4PacketWithH4 h4_packet{emboss::H4PacketType::UNKNOWN, h4_arr};
  PopulateNoninteractingToControllerBuffer(h4_packet);
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

TEST(PBadPacketTest, BadH4TypeToHostIsPassedOn) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  CreateNonInteractingToHostBuffer(h4_packet);

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
      valid_hci_arr;
  H4PacketWithHci valid_packet{emboss::H4PacketType::UNKNOWN, valid_hci_arr};
  CreateNonInteractingToHostBuffer(valid_packet);

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
      valid_hci_arr;
  H4PacketWithHci valid_packet{emboss::H4PacketType::UNKNOWN, valid_hci_arr};
  emboss::ReadLocalVersionInfoCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          valid_packet, emboss::EventCode::COMMAND_COMPLETE);
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

// Proxy Host should reserve requested ACL LE credits from controller's ACL LE
// credits when using LEReadBufferSizeV1 command.
TEST(ReserveLeAclCredits, ProxyCreditsReserveCreditsWithLEReadBufferSizeV1) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter event_view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan());
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

  EXPECT_TRUE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// Proxy Host should reserve requested ACL LE credits from controller's ACL LE
// credits when using LEReadBufferSizeV2 command.
TEST(ReserveLeAclCredits, ProxyCreditsReserveCreditsWithLEReadBufferSizeV2) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV2CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        emboss::LEReadBufferSizeV2CommandCompleteEventWriter event_view =
            MakeEmboss<emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
                received_packet.GetHciSpan());
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

  EXPECT_TRUE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// If controller provides less than wanted credits, we should reserve that
// smaller amount.
TEST(ReserveLeAclCredits, ProxyCreditsCappedByControllerCredits) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(5);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        // We want 7, but can reserve only 5 from original 5 (so 0 left for
        // host).
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter event_view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan());
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
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(10);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter event_view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan());
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

  EXPECT_FALSE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(sends_called, 1);
}

// If controller has no credits, proxy should reserve none.
TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenHostCreditsZero) {
  std::array<
      uint8_t,
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter::SizeInBytes()>
      hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
          h4_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::LE_READ_BUFFER_SIZE_V1);
  view.total_num_le_acl_data_packets().Write(0);

  uint8_t sends_called = 0;
  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&sends_called](H4PacketWithHci&& received_packet) {
        sends_called++;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter event_view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                received_packet.GetHciSpan());
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

  EXPECT_TRUE(proxy.HasSendAclCapability());

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

  EXPECT_TRUE(proxy.HasSendAclCapability());
}

// ########## GattNotifyTest

TEST(GattNotifyTest, SendGattNotify1ByteAttribute) {
  struct {
    int sends_called = 0;
    // First four bits 0x0 encode PB & BC flags
    uint16_t handle = 0x0ACB;
    // Length of L2CAP PDU
    uint16_t data_total_length = 0x0008;
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
        auto gatt_notify = emboss::AttNotifyOverAclView(
            capture.attribute_value.size(),
            packet.GetHciSpan().data(),
            capture.expected_gatt_notify_packet.size());
        EXPECT_EQ(gatt_notify.acl_header().handle().Read(), capture.handle);
        EXPECT_EQ(gatt_notify.acl_header().packet_boundary_flag().Read(),
                  emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
        EXPECT_EQ(gatt_notify.acl_header().broadcast_flag().Read(),
                  emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
        EXPECT_EQ(gatt_notify.acl_header().data_total_length().Read(),
                  capture.data_total_length);
        EXPECT_EQ(gatt_notify.l2cap_header().pdu_length().Read(),
                  capture.pdu_length);
        EXPECT_EQ(gatt_notify.l2cap_header().channel_id().Read(),
                  capture.channel_id);
        EXPECT_EQ(gatt_notify.att_handle_value_ntf().attribute_opcode().Read(),
                  static_cast<emboss::AttOpcode>(capture.attribute_opcode));
        EXPECT_EQ(gatt_notify.att_handle_value_ntf().attribute_handle().Read(),
                  capture.attribute_handle);
        EXPECT_EQ(
            gatt_notify.att_handle_value_ntf().attribute_value()[0].Read(),
            capture.attribute_value[0]);
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  SendReadBufferResponseFromController(proxy, 1);

  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.handle,
                                  capture.attribute_handle,
                                  pw::span(capture.attribute_value))
                  .ok());
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(GattNotifyTest, SendGattNotify2ByteAttribute) {
  struct {
    int sends_called = 0;
    // Max connection_handle value; first four bits 0x0 encode PB & BC flags
    const uint16_t handle = 0x0EFF;
    // Length of L2CAP PDU
    const uint16_t data_total_length = 0x0009;
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
        auto gatt_notify = emboss::AttNotifyOverAclView(
            capture.attribute_value.size(),
            packet.GetHciSpan().data(),
            capture.expected_gatt_notify_packet.size());
        EXPECT_EQ(gatt_notify.acl_header().handle().Read(), capture.handle);
        EXPECT_EQ(gatt_notify.acl_header().packet_boundary_flag().Read(),
                  emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
        EXPECT_EQ(gatt_notify.acl_header().broadcast_flag().Read(),
                  emboss::AclDataPacketBroadcastFlag::POINT_TO_POINT);
        EXPECT_EQ(gatt_notify.acl_header().data_total_length().Read(),
                  capture.data_total_length);
        EXPECT_EQ(gatt_notify.l2cap_header().pdu_length().Read(),
                  capture.pdu_length);
        EXPECT_EQ(gatt_notify.l2cap_header().channel_id().Read(),
                  capture.channel_id);
        EXPECT_EQ(gatt_notify.att_handle_value_ntf().attribute_opcode().Read(),
                  static_cast<emboss::AttOpcode>(capture.attribute_opcode));
        EXPECT_EQ(gatt_notify.att_handle_value_ntf().attribute_handle().Read(),
                  capture.attribute_handle);
        EXPECT_EQ(
            gatt_notify.att_handle_value_ntf().attribute_value()[0].Read(),
            capture.attribute_value[0]);
        EXPECT_EQ(
            gatt_notify.att_handle_value_ntf().attribute_value()[1].Read(),
            capture.attribute_value[1]);
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  // Allow proxy to reserve 1 credit.
  SendReadBufferResponseFromController(proxy, 1);

  EXPECT_TRUE(proxy
                  .SendGattNotify(capture.handle,
                                  capture.attribute_handle,
                                  pw::span(capture.attribute_value))
                  .ok());
  EXPECT_EQ(capture.sends_called, 1);
}

TEST(GattNotifyTest, SendGattNotifyReturnsErrorForInvalidArgs) {
  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4 packet) { FAIL(); });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  std::array<uint8_t, 2> attribute_value = {0xAB, 0xCD};
  // connection_handle too large
  EXPECT_EQ(proxy.SendGattNotify(0x0FFF, 345, pw::span(attribute_value)),
            PW_STATUS_INVALID_ARGUMENT);
  // attribute_handle is 0
  EXPECT_EQ(proxy.SendGattNotify(123, 0, pw::span(attribute_value)),
            PW_STATUS_INVALID_ARGUMENT);
  // attribute_value too large
  std::array<uint8_t, 3> attribute_value_too_large = {0xAB, 0xCD, 0xEF};
  EXPECT_EQ(proxy.SendGattNotify(123, 345, pw::span(attribute_value_too_large)),
            PW_STATUS_INVALID_ARGUMENT);
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, kNumConnections);
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

  // At this point, proxy has used all 3 credits, 1 on each Connection, so send
  // should fail.
  EXPECT_EQ(proxy.SendGattNotify(
                capture.connection_handles[0], 1, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);

  // Send Number_of_Completed_Packets event that reports 1 packet on Connection
  // 0, 0 packets on Connection 1, and 1 packet on Connection 2. Checks in
  // send_to_host_fn will ensure we have reclaimed 2 of 3 credits.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 3>({{{capture.connection_handles[0], 1},
                                       {capture.connection_handles[1], 0},
                                       {capture.connection_handles[2], 1}}}));
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 2);
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

  // At this point proxy, has used both credits, so send should fail.
  EXPECT_EQ(proxy.SendGattNotify(
                capture.connection_handles[1], 1, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);

  // Send Number_of_Completed_Packets event that reports 10 packets on
  // Connection 0 and 15 packets on Connection 1. Checks in send_to_host_fn
  // will ensure we have reclaimed exactly 2 credits, 1 from each Connection.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}}));
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 4);
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
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}}));
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 10);
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event that reports 10 packets on
  // Connection 0 and 15 packets on Connection 1. Checks in send_to_host_fn
  // will ensure we have not modified the NOCP event.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 2>({{{capture.connection_handles[0], 10},
                                       {capture.connection_handles[1], 15}}}));
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 10);
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event with no entries.
  SendNumberOfCompletedPackets(proxy, FlatMap<uint16_t, uint16_t, 0>({{}}));
  // NOCP has no entries, so will not be passed on to host.
  EXPECT_EQ(capture.sends_called, 1);

  // Send Number_of_Completed_Packets event that reports 0 packets for various
  // connections.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 5>({{{capture.connection_handles[0], 0},
                                       {capture.connection_handles[1], 0},
                                       {capture.connection_handles[2], 0},
                                       {capture.connection_handles[3], 0},
                                       {capture.connection_handles[4], 0}}}));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 10);
  // Proxy host will not pass on a NOCP with no credits.
  EXPECT_EQ(capture.sends_called, 1);
}

// ########## DisconnectionCompleteTest

TEST(DisconnectionCompleteTest, DisconnectionReclaimsCredits) {
  struct {
    int sends_called = 0;
    uint16_t connection_handle = 0x123;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 10);
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
  SendDisconnectionCompleteEvent(proxy, capture.connection_handle);
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 8);

  // Send Number_of_Completed_Packets event that reports 10 packets, none of
  // which should be reclaimed because this Connection has disconnected. Checks
  // in send_to_host_fn will ensure we have not modified the NOCP event.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{capture.connection_handle, 10}}}));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 8);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(capture.sends_called, 3);
}

TEST(DisconnectionCompleteTest, FailedDisconnectionHasNoEffect) {
  uint16_t connection_handle = 0x123;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithH4&& packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  SendReadBufferResponseFromController(proxy, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Use sole credit.
  EXPECT_TRUE(
      proxy.SendGattNotify(connection_handle, 1, pw::span(attribute_value))
          .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send failed Disconnection_Complete event, should not reclaim credit.
  SendDisconnectionCompleteEvent(
      proxy, connection_handle, /*successful=*/false);
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
  SendReadBufferResponseFromController(proxy, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Use sole credit.
  EXPECT_TRUE(
      proxy.SendGattNotify(connection_handle, 1, pw::span(attribute_value))
          .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Send Disconnection_Complete event to random Connection, should have no
  // effect.
  SendDisconnectionCompleteEvent(proxy, 0x456);
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
}

TEST(DisconnectionCompleteTest, CanReuseConnectionHandleAfterDisconnection) {
  struct {
    int sends_called = 0;
    uint16_t connection_handle = 0x123;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      [&capture](H4PacketWithHci&& packet) {
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
  SendReadBufferResponseFromController(proxy, 1);
  EXPECT_EQ(capture.sends_called, 1);

  std::array<uint8_t, 1> attribute_value = {0};

  // Establish connection over `connection_handle`.
  EXPECT_TRUE(proxy
                  .SendGattNotify(
                      capture.connection_handle, 1, pw::span(attribute_value))
                  .ok());
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Disconnect `connection_handle`.
  SendDisconnectionCompleteEvent(proxy, capture.connection_handle);
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
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{capture.connection_handle, 1}}}));
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
        auto event_header =
            MakeEmboss<emboss::EventHeaderView>(packet.GetHciSpan().subspan(
                0, emboss::EventHeader::IntrinsicSizeInBytes()));
        host_capture.sends_called++;
        if (event_header.event_code_enum().Read() !=
            emboss::EventCode::NUMBER_OF_COMPLETED_PACKETS) {
          return;
        }

        auto view = MakeEmboss<emboss::NumberOfCompletedPacketsEventView>(
            packet.GetHciSpan());
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
      [&controller_capture](H4PacketWithH4&& packet) {
        ++controller_capture.sends_called;
      });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);
  SendReadBufferResponseFromController(proxy, 2);
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
  EXPECT_TRUE(proxy.HasSendAclCapability());
  // Reset clears credit reservation, so send should fail.
  EXPECT_EQ(proxy.SendGattNotify(1, 1, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);
  EXPECT_EQ(controller_capture.sends_called, 1);

  // Re-initialize AclDataChannel with 2 credits.
  SendReadBufferResponseFromController(proxy, 2);
  EXPECT_EQ(host_capture.sends_called, 2);

  // Send ACL on random handle to expend one credit.
  EXPECT_TRUE(proxy.SendGattNotify(1, 1, pw::span(attribute_value)).ok());
  EXPECT_EQ(controller_capture.sends_called, 2);
  // This should have no effect, as the reset has cleared our active connection
  // on this handle.
  SendNumberOfCompletedPackets(
      proxy,
      FlatMap<uint16_t, uint16_t, 1>({{{host_capture.connection_handle, 1}}}));
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);
  // NOCP has credits remaining so will be passed on to host.
  EXPECT_EQ(host_capture.sends_called, 3);
}

TEST(ResetTest, ProxyHandlesMultipleResets) {
  int sends_called = 0;

  pw::Function<void(H4PacketWithHci && packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&sends_called](H4PacketWithH4&& packet) { ++sends_called; });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 1);
  SendReadBufferResponseFromController(proxy, 1);

  proxy.Reset();
  proxy.Reset();

  std::array<uint8_t, 1> attribute_value = {0};
  // Validate state after double reset.
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  EXPECT_TRUE(proxy.HasSendAclCapability());
  EXPECT_EQ(proxy.SendGattNotify(1, 1, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);
  SendReadBufferResponseFromController(proxy, 1);
  EXPECT_TRUE(proxy.SendGattNotify(1, 1, pw::span(attribute_value)).ok());
  EXPECT_EQ(sends_called, 1);

  proxy.Reset();

  // Validate state after third reset.
  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
  EXPECT_TRUE(proxy.HasSendAclCapability());
  EXPECT_EQ(proxy.SendGattNotify(1, 1, pw::span(attribute_value)),
            PW_STATUS_UNAVAILABLE);
  SendReadBufferResponseFromController(proxy, 1);
  EXPECT_TRUE(proxy.SendGattNotify(1, 1, pw::span(attribute_value)).ok());
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
  SendReadBufferResponseFromController(proxy, 2 * kMaxSends);

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
  SendReadBufferResponseFromController(proxy, 2 * kMaxSends);

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

// Verify we can send packets over many different connections. Test sends a
// packet and then NOCP one at a time over more connections than our max active
// connections size. This should succeed since we only track proxy-related
// connections, not all host connections.
TEST(MultiSendTest, CanSendOverManyDifferentConnections) {
  // This should match AclConnection::kMaxConnections.
  static constexpr size_t kMaxProxyActiveConnections = 10;
  constexpr uint16_t kSends = kMaxProxyActiveConnections * 2;
  std::array<uint8_t, 1> attribute_value = {0xF};
  struct {
    uint16_t sends_called = 0;
  } capture;

  pw::Function<void(H4PacketWithHci && packet)>&& send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci&& packet) {});
  pw::Function<void(H4PacketWithH4 && packet)> send_to_controller_fn(
      [&capture](H4PacketWithH4&& packet) { ++capture.sends_called; });

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);
  SendReadBufferResponseFromController(proxy, 1);

  for (uint16_t send = 1; send <= kSends; send++) {
    // Use current send count as the connection handle.
    uint16_t conn_handle = send;
    EXPECT_TRUE(
        proxy.SendGattNotify(conn_handle, 345, pw::span(attribute_value)).ok());
    EXPECT_EQ(capture.sends_called, send);

    SendNumberOfCompletedPackets(proxy,
                                 FlatMap<uint16_t, uint16_t, 1>({{
                                     {conn_handle, 1},
                                 }}));
  }
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
  SendReadBufferResponseFromController(proxy, kMaxSends);

  std::array<uint8_t, 1> attribute_value = {0xF};
  // Occupy all send buffers.
  for (size_t i = 0; i < kMaxSends; ++i) {
    EXPECT_TRUE(proxy.SendGattNotify(123, 345, pw::span(attribute_value)).ok());
  }

  proxy.Reset();
  SendReadBufferResponseFromController(proxy, kMaxSends);

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

}  // namespace
}  // namespace pw::bluetooth::proxy
