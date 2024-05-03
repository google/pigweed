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

// Simple struct for returning a H4 array and an emboss view on its HCI packet.
template <typename EmbossT>
struct EmbossViewWithH4Buffer {
  // Size is +1 to accommodate the H4 packet type at the front.
  std::array<uint8_t, EmbossT::SizeInBytes() + 1> arr;
  EmbossT view;
};

// Return H4 command buffer and Emboss view using indicated Emboss type to send
// towards controller.
template <typename EmbossT>
EmbossViewWithH4Buffer<EmbossT> CreateToControllerViewWithBuffer(
    emboss::OpCode opcode) {
  EmbossViewWithH4Buffer<EmbossT> view_arr;
  std::iota(view_arr.arr.begin(), view_arr.arr.end(), 100);
  view_arr.arr[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  view_arr.view = MakeEmboss<EmbossT>(H4HciSubspan(view_arr.arr));
  EXPECT_TRUE(view_arr.view.IsComplete());
  view_arr.view.header().opcode_enum().Write(opcode);
  return view_arr;
}

// Populate an H4 buffer to send towards controller. Type should be one
// that proxy host doesn't interact with.
std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
CreateNoninteractingToControllerBuffer() {
  EmbossViewWithH4Buffer<emboss::InquiryCommandWriter> view_arr =
      CreateToControllerViewWithBuffer<emboss::InquiryCommandWriter>(
          emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  return view_arr.arr;
}

// Return H4 event buffer and Emboss view using indicated Emboss type to send
// towards host.
template <typename EmbossT>
EmbossViewWithH4Buffer<EmbossT> CreateToHostEventViewWithBuffer(
    emboss::EventCode event_code, emboss::StatusCode status_code) {
  EmbossViewWithH4Buffer<EmbossT> view_arr;
  std::iota(view_arr.arr.begin(), view_arr.arr.end(), 100);
  view_arr.arr[0] = cpp23::to_underlying(emboss::H4PacketType::EVENT);
  view_arr.view = MakeEmboss<EmbossT>(H4HciSubspan(view_arr.arr));
  view_arr.view.header().event_code_enum().Write(event_code);
  view_arr.view.status().Write(status_code);
  EXPECT_TRUE(view_arr.view.IsComplete());
  return view_arr;
}

// Populate an H4 buffer to send towards host. Type should be one
// that proxy host doesn't interact with.
std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
CreateNonInteractingToHostBuffer() {
  EmbossViewWithH4Buffer<emboss::InquiryCompleteEventWriter> view_arr =
      CreateToHostEventViewWithBuffer<emboss::InquiryCompleteEventWriter>(
          emboss::EventCode::INQUIRY_COMPLETE,
          emboss::StatusCode::COMMAND_DISALLOWED);
  return view_arr.arr;
}

// ########## Examples

// Example for docs.rst.
TEST(Example, ExampleUsage) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      h4_array_from_host = CreateNoninteractingToControllerBuffer();
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

// Verify buffer is properly passed (is equal value).
TEST(PassthroughTest, ToControllerPassesEqualBuffer) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array;
    bool send_called;
  } send_capture;

  send_capture.h4_array = CreateNoninteractingToControllerBuffer();
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array =
      CreateNoninteractingToControllerBuffer();

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_controller_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.h4_array.begin(),
                           send_capture.h4_array.end()));
  });

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromHost(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// Verify buffer is passed to host callback with the same contents.
TEST(PassthroughTest, ToHostPassesEqualBuffer) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
        h4_array;
    bool send_called;
  } send_capture;

  send_capture.h4_array = CreateNonInteractingToHostBuffer();

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.h4_array.begin(),
                           send_capture.h4_array.end()));
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// Verify a command complete event (of a type that proxy doesn't act on) is
// passed to host callback with the same contents.
TEST(PassthroughTest, ToHostPassesEqualCommandComplete) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    EmbossViewWithH4Buffer<
        emboss::ReadLocalVersionInfoCommandCompleteEventWriter>
        view_arr;
    bool send_called;
  } send_capture;

  send_capture.view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  send_capture.view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.view_arr.arr.begin(),
                           send_capture.view_arr.arr.end()));
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.view_arr.arr));

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// ########## BadPacketTest
// The proxy should not affect buffers it can't process (it should just pass
// them on).

TEST(BadPacketTest, EmptyBufferToController) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, 0> h4_array;
    bool send_called;
  } send_capture;

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_controller_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(packet.empty());
  });

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromHost(pw::span(send_capture.h4_array));

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(BadPacketTest, EmptyBufferToHostIsPassedOn) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, 0> h4_array;
    bool send_called;
  } send_capture;

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    EXPECT_TRUE(packet.empty());
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.h4_array));

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(BadPacketTest, TooShortEventToHost) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::EventHeaderView::SizeInBytes() - 1> short_array;
    bool send_called;
  } send_capture;

  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      valid_array = CreateNonInteractingToHostBuffer();

  // Copy valid event into a short_array whose size is one less than a valid
  // EventHeader.
  std::copy_n(std::begin(valid_array),
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

  proxy.HandleH4HciFromController(pw::span(send_capture.short_array));

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(BadPacketTest, TooShortCommandCompleteEventToHost) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    std::array<uint8_t, emboss::CommandCompleteEventView::SizeInBytes() - 1>
        short_array;
    bool send_called;
  } send_capture;

  EmbossViewWithH4Buffer<emboss::ReadLocalVersionInfoCommandCompleteEventWriter>
      view_arr;
  view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadLocalVersionInfoCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_LOCAL_VERSION_INFO);

  // Copy valid event into a short_array whose size is one less than a valid
  // command complete event.
  std::copy_n(std::begin(view_arr.arr),
              emboss::CommandCompleteEventView::SizeInBytes() - 1,
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

  proxy.HandleH4HciFromController(pw::span(send_capture.short_array));

  // Verify callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// ########## ReserveLeAclCredits Tests

// Proxy Host should reserve 2 ACL LE credits from controller's ACL LE credits.
TEST(ReserveLeAclCredits, ProxyCreditsReservedFromControllerCredits) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    EmbossViewWithH4Buffer<emboss::ReadBufferSizeCommandCompleteEventWriter>
        view_arr;
    bool send_called;
  } send_capture;

  send_capture.view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadBufferSizeCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  send_capture.view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  send_capture.view_arr.view.total_num_acl_data_packets().Write(10);

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    // Should reserve 2 credits from original total of 10 (so 8 left for host).
    EXPECT_EQ(send_capture.view_arr.view.total_num_acl_data_packets().Read(),
              8);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.view_arr.arr));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 2);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// If controller provides less than wanted 2 credits, we should reserve that
// smaller amount.
TEST(ReserveLeAclCredits, ProxyCreditsCappedByControllerCredits) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    EmbossViewWithH4Buffer<emboss::ReadBufferSizeCommandCompleteEventWriter>
        view_arr;
    bool send_called;
  } send_capture;

  send_capture.view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadBufferSizeCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  send_capture.view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  send_capture.view_arr.view.total_num_acl_data_packets().Write(1);

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    // Should reserve 1 credit from original total of 1 (so 0 left for host).
    EXPECT_EQ(send_capture.view_arr.view.total_num_acl_data_packets().Read(),
              0);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.view_arr.arr));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 1);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

// If controller has no credits, proxy should reserve none.
TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenHostCreditsZero) {
  // Struct for capturing because `pw::Function` can't fit multiple captures.
  struct {
    EmbossViewWithH4Buffer<emboss::ReadBufferSizeCommandCompleteEventWriter>
        view_arr;
    bool send_called;
  } send_capture;

  send_capture.view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadBufferSizeCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  send_capture.view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  send_capture.view_arr.view.total_num_acl_data_packets().Write(0);

  send_capture.send_called = false;
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    send_capture.send_called = true;
    // Should reserve 0 credit from original total of 0 (so 0 left for host).
    EXPECT_EQ(send_capture.view_arr.view.total_num_acl_data_packets().Read(),
              0);
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.HandleH4HciFromController(pw::span(send_capture.view_arr.arr));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);

  // Verify to controller callback was called.
  EXPECT_EQ(send_capture.send_called, true);
}

TEST(ReserveLeAclPackets, ProxyCreditsZeroWhenNotInitialized) {
  auto view_arr = CreateToHostEventViewWithBuffer<
      emboss::ReadBufferSizeCommandCompleteEventWriter>(
      emboss::EventCode::COMMAND_COMPLETE, emboss::StatusCode::SUCCESS);
  view_arr.view.command_complete().command_opcode_enum().Write(
      emboss::OpCode::READ_BUFFER_SIZE);
  view_arr.view.total_num_acl_data_packets().Write(0);

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  ProxyHost proxy =
      ProxyHost(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  EXPECT_EQ(proxy.GetNumFreeLeAclPackets(), 0);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
