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

#include "emboss_util.h"
#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

// ########## Util functions

// Populate passed H4 command buffer and return Emboss view on it.
template <typename EmbossT, std::size_t arr_size>
EmbossT CreateAndPopulateToControllerView(std::array<uint8_t, arr_size>& h4_arr,
                                          emboss::OpCode opcode) {
  std::iota(h4_arr.begin(), h4_arr.end(), 100);
  h4_arr[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  EmbossT view = MakeEmboss<EmbossT>(H4HciSubspan(h4_arr));
  EXPECT_TRUE(view.IsComplete());
  view.header().opcode_enum().Write(opcode);
  return view;
}

// Return a populated H4 command buffer of a type that proxy host doesn't
// interact with.
std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
PopulateNoninteractingToControllerBuffer(
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>& arr) {
  CreateAndPopulateToControllerView<emboss::InquiryCommandWriter>(
      arr, emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  return arr;
}

// Populate passed H4 event buffer and return Emboss view on it.
template <typename EmbossT, std::size_t arr_size>
EmbossT CreateAndPopulateToHostEventView(std::array<uint8_t, arr_size>& arr,
                                         emboss::EventCode event_code) {
  std::iota(arr.begin(), arr.end(), 0x10);
  arr[0] = cpp23::to_underlying(emboss::H4PacketType::EVENT);
  EmbossT view = MakeEmboss<EmbossT>(H4HciSubspan(arr));
  view.header().event_code_enum().Write(event_code);
  view.status().Write(emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(view.IsComplete());
  return view;
}

// Return a populated H4 event buffer of a type that proxy host doesn't interact
// with.
std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
CreateNonInteractingToHostBuffer() {
  std::array<uint8_t, emboss::InquiryCompleteEventWriter::SizeInBytes() + 1>
      arr;
  CreateAndPopulateToHostEventView<emboss::InquiryCompleteEventWriter>(
      arr, emboss::EventCode::INQUIRY_COMPLETE);
  return arr;
}

// ########## Examples

// Example for docs.rst.
TEST(Example, ExampleUsage) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      h4_array_from_host;
  PopulateNoninteractingToControllerBuffer(h4_array_from_host);
  auto h4_span_from_host = pw::span(h4_array_from_host);

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array_from_controller = CreateNonInteractingToHostBuffer();
  auto h4_span_from_controller = pw::span(h4_array_from_controller);

  H4HciPacketSendFn containerSendToHostFn([](H4HciPacket packet) {});

  H4HciPacketSendFn containerSendToControllerFn(([](H4HciPacket packet) {}));

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]

#include "pw_bluetooth_proxy/proxy_host.h"

  // Container creates ProxyHost .
  ProxyHost proxy = ProxyHost(std::move(containerSendToHostFn),
                              std::move(containerSendToControllerFn));

  // Container passes H4 packets from host through proxy. Proxy will in turn
  // call the container-provided `containerSendToControllerFn` to pass them on
  // to the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromHost(h4_span_from_host);

  // Container passes H4 packets from controller through proxy. Proxy will in
  // turn call the container-provided `containerSendToHostFn` to pass them on to
  // the controller. Some packets may be modified, added, or removed.
  proxy.HandleH4HciFromController(h4_span_from_controller);

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]
}

// ########## PassthroughTest

// Verify buffer is properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToControllerPassesEqualBuffer) {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      send_packet;
  PopulateNoninteractingToControllerBuffer(send_packet);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array;
    // Also use pointer to verify zero-copy.
    uint8_t* send_array_ptr;
    bool send_called;
  } send_capture = {send_packet, send_packet.data(), false};

  H4HciPacketSendFn send_to_controller_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.h4_array.begin(),
                           send_capture.h4_array.end()));
    EXPECT_EQ(packet.data(), send_capture.send_array_ptr);
  });

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromHost(send_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// Verify buffer is properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToHostPassesEqualBuffer) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      send_packet = CreateNonInteractingToHostBuffer();

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    // Use a copy for comparison to catch if proxy incorrectly changes the
    // passed buffer.
    std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
        h4_array;
    // Also use pointer to verify zero-copy.
    uint8_t* send_array_ptr;
    bool send_called;
  } send_capture = {send_packet, send_packet.data(), false};

  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.h4_array.begin(),
                           send_capture.h4_array.end()));
    EXPECT_EQ(packet.data(), send_capture.send_array_ptr);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// Verify a command complete event (of a type that proxy doesn't act on) is
// properly passed (contents unaltered and zero-copy).
TEST(PassthroughTest, ToHostPassesEqualCommandComplete) {
  std::array<
      uint8_t,
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes() + 1>
      send_packet;
  emboss::ReadLocalVersionInfoCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          send_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<
        uint8_t,
        emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes() +
            1>
        arr;
    // Also use pointer to verify zero-copy.
    uint8_t* send_array_ptr;
    bool send_called;
  } send_capture = {send_packet, send_packet.data(), false};

  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.arr.begin(),
                           send_capture.arr.end()));
    EXPECT_EQ(packet.data(), send_capture.send_array_ptr);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// ########## BadPacketTest
// The proxy should not affect buffers it can't process (it should just pass
// them on).

TEST(BadPacketTest, EmptyBufferToControllerIsPassedOn) {
  std::array<uint8_t, 0> send_packet;

  bool send_called = false;
  H4HciPacketSendFn send_to_controller_fn([&send_called](H4HciPacket packet) {
    send_called = true;
    EXPECT_TRUE(packet.empty());
  });

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromHost(send_packet);

  // Verify callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(BadPacketTest, EmptyBufferToHostIsPassedOn) {
  std::array<uint8_t, 0> send_packet;

  bool send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_called](H4HciPacket packet) {
    send_called = true;
    EXPECT_TRUE(packet.empty());
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  // Verify callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(BadPacketTest, TooShortEventToHostIsPassOn) {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      valid_packet = CreateNonInteractingToHostBuffer();

  // Create span for sending whose size is one less than a valid command
  // complete event.
  pw::span<uint8_t> send_packet =
      pw::span<uint8_t>(valid_packet)
          .subspan(0, emboss::EventHeaderView::SizeInBytes() - 1);

  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::EventHeaderView::SizeInBytes() - 1> short_array;
    bool send_called;
  } send_capture;
  // Copy valid event into a short_array whose size is one less than a valid
  // EventHeader.
  std::copy_n(std::begin(send_packet),
              emboss::EventHeaderView::SizeInBytes() - 1,
              std::begin(send_capture.short_array));
  send_capture.send_called = false;

  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.short_array.begin(),
                           send_capture.short_array.end()));
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(BadPacketTest, TooShortCommandCompleteEventToHost) {
  std::array<
      uint8_t,
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter::SizeInBytes() + 1>
      valid_packet;
  emboss::ReadLocalVersionInfoCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
          valid_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  // Create span for sending whose size is one less than a valid command
  // complete event.
  pw::span<uint8_t> send_packet =
      pw::span<uint8_t>(valid_packet)
          .subspan(0, emboss::CommandCompleteEventView::SizeInBytes() - 1);

  // Struct for capturing because `pw::Function` capture can't fit multiple
  // fields .
  struct {
    std::array<uint8_t, emboss::CommandCompleteEventView::SizeInBytes() - 1>
        expected_packet;
    bool send_called;
  } send_capture;
  std::copy_n(std::begin(send_packet),
              emboss::CommandCompleteEventView::SizeInBytes() - 1,
              std::begin(send_capture.expected_packet));
  send_capture.send_called = false;

  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.expected_packet.begin(),
                           send_capture.expected_packet.end()));
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// ########## ReserveLeAclCredits Tests

// Proxy Host should reserve 2 ACL LE credits from controller's ACL LE credits.
TEST(ReserveLeAclCredits, ProxyCreditsReservedFromControllerCredits) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes() +
                 1>
      send_packet;
  emboss::ReadBufferSizeCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadBufferSizeCommandCompleteEventWriter>(
          send_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(10);

  bool send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_called](H4HciPacket h4_packet) {
    send_called = true;
    emboss::ReadBufferSizeCommandCompleteEventWriter view =
        MakeEmboss<emboss::ReadBufferSizeCommandCompleteEventWriter>(
            H4HciSubspan(h4_packet));
    // Should reserve 2 credits from original total of 10 (so 8 left for host).
    EXPECT_EQ(view.total_num_acl_data_packets().Read(), 8);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

// If controller provides less than wanted 2 credits, we should reserve that
// smaller amount.
TEST(ReserveLeAclCredits, ProxyCreditsCappedByControllerCredits) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes() +
                 1>
      send_packet;
  emboss::ReadBufferSizeCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadBufferSizeCommandCompleteEventWriter>(
          send_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(1);

  bool send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_called](H4HciPacket h4_packet) {
    send_called = true;
    // Should reserve 1 credit from original total of 1 (so 0 left for host).
    emboss::ReadBufferSizeCommandCompleteEventWriter view =
        MakeEmboss<emboss::ReadBufferSizeCommandCompleteEventWriter>(
            H4HciSubspan(h4_packet));
    EXPECT_EQ(view.total_num_acl_data_packets().Read(), 0);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

// If controller has no credits, proxy should reserve none.
TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenHostCreditsZero) {
  std::array<uint8_t,
             emboss::ReadBufferSizeCommandCompleteEventWriter::SizeInBytes() +
                 1>
      send_packet;
  emboss::ReadBufferSizeCommandCompleteEventWriter view =
      CreateAndPopulateToHostEventView<
          emboss::ReadBufferSizeCommandCompleteEventWriter>(
          send_packet, emboss::EventCode::COMMAND_COMPLETE);
  view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view.total_num_acl_data_packets().Write(0);

  bool send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_called](H4HciPacket h4_packet) {
    send_called = true;
    emboss::ReadBufferSizeCommandCompleteEventWriter view =
        MakeEmboss<emboss::ReadBufferSizeCommandCompleteEventWriter>(
            H4HciSubspan(h4_packet));
    // Should reserve 0 credit from original total of 0 (so 0 left for host).
    EXPECT_EQ(view.total_num_acl_data_packets().Read(), 0);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(send_packet);

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenNotInitialized) {
  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
