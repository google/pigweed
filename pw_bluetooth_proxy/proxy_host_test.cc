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

#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/emboss_util.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_function/function.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

// ########## Util functions

// Populate passed H4 command buffer and return Emboss view on it.
template <typename EmbossT>
EmbossT CreateAndPopulateToControllerView(H4PacketWithHci& h4_packet,
                                          emboss::OpCode opcode) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 100);
  h4_packet.GetH4Type(emboss::H4PacketType::COMMAND);
  EmbossT view = MakeEmboss<EmbossT>(h4_packet.GetHciSpan());
  EXPECT_TRUE(view.IsComplete());
  view.header().opcode_enum().Write(opcode);
  return view;
}

// Return a populated H4 command buffer of a type that proxy host doesn't
// interact with.
void PopulateNoninteractingToControllerBuffer(H4PacketWithHci& h4_packet) {
  CreateAndPopulateToControllerView<emboss::InquiryCommandWriter>(
      h4_packet, emboss::OpCode::LINK_KEY_REQUEST_REPLY);
}

// Populate passed H4 event buffer and return Emboss view on it.
template <typename EmbossT>
EmbossT CreateAndPopulateToHostEventView(H4PacketWithHci& h4_packet,
                                         emboss::EventCode event_code) {
  std::iota(h4_packet.GetHciSpan().begin(), h4_packet.GetHciSpan().end(), 0x10);
  h4_packet.GetH4Type(emboss::H4PacketType::EVENT);
  EmbossT view = MakeEmboss<EmbossT>(h4_packet.GetHciSpan());
  view.header().event_code_enum().Write(event_code);
  view.status().Write(emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(view.IsComplete());
  return view;
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
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes()>
      hci_array_from_host;
  H4PacketWithHci h4_packet_from_host{emboss::H4PacketType::UNKNOWN,
                                      hci_array_from_host};
  PopulateNoninteractingToControllerBuffer(h4_packet_from_host);

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      hci_array_from_controller;
  H4PacketWithHci h4_packet_from_controller{emboss::H4PacketType::UNKNOWN,
                                            hci_array_from_controller};

  CreateNonInteractingToHostBuffer(h4_packet_from_controller);

  pw::Function<void(H4PacketWithHci packet)> containerSendToHostFn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  pw::Function<void(H4PacketWithHci packet)> containerSendToControllerFn(
      ([]([[maybe_unused]] H4PacketWithHci packet) {}));

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]

#include "pw_bluetooth_proxy/proxy_host.h"

  // Container creates ProxyHost .
  ProxyHost proxy = ProxyHost(std::move(containerSendToHostFn),
                              std::move(containerSendToControllerFn),
                              2);

  // Container passes H4 packets from host through proxy. Proxy will in turn
  // call the container-provided `containerSendToControllerFn` to pass them on
  // to the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromHost(h4_packet_from_host);

  // Container passes H4 packets from controller through proxy. Proxy will in
  // turn call the container-provided `containerSendToHostFn` to pass them on to
  // the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromController(h4_packet_from_controller);

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]
}

// ########## PassthroughTest

// Verify buffer is properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToControllerPassesEqualBuffer) {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes()> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PopulateNoninteractingToControllerBuffer(h4_packet);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes()> hci_arr;
    H4PacketWithHci* h4_packet;
    bool send_called;
  } send_capture = {hci_arr, &h4_packet, false};

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_EQ(packet.GetH4Type(), send_capture.h4_packet->GetH4Type());
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(h4_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
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
    bool send_called;
  } send_capture = {hci_arr, &h4_packet, false};

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_EQ(packet.GetH4Type(), send_capture.h4_packet->GetH4Type());
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
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
    bool send_called;
  } send_capture = {hci_arr, &h4_packet, false};

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_EQ(packet.GetH4Type(), send_capture.h4_packet->GetH4Type());
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// ########## BadPacketTest
// The proxy should not affect buffers it can't process (it should just pass
// them on).

TEST(BadPacketTest, BadH4TypeToControllerIsPassedOn) {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes()> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  PopulateNoninteractingToControllerBuffer(h4_packet);

  // Set back to an invalid type.
  h4_packet.GetH4Type(emboss::H4PacketType::UNKNOWN);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes()> hci_arr;
    H4PacketWithHci* h4_packet;
    bool send_called;
  } send_capture = {hci_arr, &h4_packet, false};

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(h4_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(PBadPacketTest, BadH4TypeToHostIsPassedOn) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::UNKNOWN, hci_arr};
  CreateNonInteractingToHostBuffer(h4_packet);

  // Set back to an invalid type.
  h4_packet.GetH4Type(emboss::H4PacketType::UNKNOWN);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes()>
        hci_arr;
    H4PacketWithHci* h4_packet;
    bool send_called;
  } send_capture = {hci_arr, &h4_packet, false};

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::UNKNOWN);
        EXPECT_TRUE(std::equal(send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end(),
                               send_capture.h4_packet->GetHciSpan().begin(),
                               send_capture.h4_packet->GetHciSpan().end()));
        // Verify no copy by verifying buffer is at the same memory location.
        EXPECT_EQ(packet.GetHciSpan().data(),
                  send_capture.h4_packet->GetHciSpan().data());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(BadPacketTest, EmptyBufferToControllerIsPassedOn) {
  std::array<uint8_t, 0> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::COMMAND, hci_arr};

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      [&send_called](H4PacketWithHci packet) {
        send_called = true;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::COMMAND);
        EXPECT_TRUE(packet.GetHciSpan().empty());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromHost(h4_packet);

  // Verify callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(BadPacketTest, EmptyBufferToHostIsPassedOn) {
  std::array<uint8_t, 0> hci_arr;
  H4PacketWithHci h4_packet{emboss::H4PacketType::EVENT, hci_arr};

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci packet) {
        send_called = true;
        EXPECT_EQ(packet.GetH4Type(), emboss::H4PacketType::EVENT);
        EXPECT_TRUE(packet.GetHciSpan().empty());
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify callback was called.
  EXPECT_EQ(send_called, true);
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
    bool send_called;
  } send_capture;
  // Copy valid event into a short_array whose size is one less than a valid
  // EventHeader.
  std::copy(h4_packet.GetHciSpan().begin(),
            h4_packet.GetHciSpan().end(),
            send_capture.hci_arr.begin());
  send_capture.send_called = false;

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               send_capture.hci_arr.begin(),
                               send_capture.hci_arr.end()));
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
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
    bool send_called;
  } send_capture;
  std::copy(h4_packet.GetHciSpan().begin(),
            h4_packet.GetHciSpan().end(),
            send_capture.hci_arr.begin());
  send_capture.send_called = false;

  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_capture](H4PacketWithHci packet) {
        send_capture.send_called = true;
        EXPECT_TRUE(std::equal(packet.GetHciSpan().begin(),
                               packet.GetHciSpan().end(),
                               send_capture.hci_arr.begin(),
                               send_capture.hci_arr.end()));
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
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

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci h4_packet) {
        send_called = true;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                h4_packet.GetHciSpan());
        // Should reserve 2 credits from original total of 10 (so 8 left for
        // host).
        EXPECT_EQ(view.total_num_le_acl_data_packets().Read(), 8);
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  EXPECT_TRUE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
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

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci h4_packet) {
        send_called = true;
        emboss::LEReadBufferSizeV2CommandCompleteEventWriter view =
            MakeEmboss<emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
                h4_packet.GetHciSpan());
        // Should reserve 2 credits from original total of 10 (so 8 left for
        // host).
        EXPECT_EQ(view.total_num_le_acl_data_packets().Read(), 8);
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  EXPECT_TRUE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
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

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci h4_packet) {
        send_called = true;
        // We want 7, but can reserve only 5 from original 5 (so 0 left for
        // host).
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                h4_packet.GetHciSpan());
        EXPECT_EQ(view.total_num_le_acl_data_packets().Read(), 0);
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 7);

  proxy.HandleH4HciFromController(h4_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 5);

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
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

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci h4_packet) {
        send_called = true;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                h4_packet.GetHciSpan());
        // Should reserve 0 credits from original total of 10 (so 10 left for
        // host).
        EXPECT_EQ(view.total_num_le_acl_data_packets().Read(), 10);
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 0);

  proxy.HandleH4HciFromController(h4_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_FALSE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
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

  bool send_called = false;
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      [&send_called](H4PacketWithHci h4_packet) {
        send_called = true;
        emboss::LEReadBufferSizeV1CommandCompleteEventWriter view =
            MakeEmboss<emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
                h4_packet.GetHciSpan());
        // Should reserve 0 credit from original total of 0 (so 0 left for
        // host).
        EXPECT_EQ(view.total_num_le_acl_data_packets().Read(), 0);
      });

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  proxy.HandleH4HciFromController(h4_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_TRUE(proxy.HasSendAclCapability());

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenNotInitialized) {
  pw::Function<void(H4PacketWithHci packet)> send_to_host_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  pw::Function<void(H4PacketWithHci packet)> send_to_controller_fn(
      []([[maybe_unused]] H4PacketWithHci packet) {});

  ProxyHost proxy = ProxyHost(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), 2);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  EXPECT_TRUE(proxy.HasSendAclCapability());
}

}  // namespace
}  // namespace pw::bluetooth::proxy
