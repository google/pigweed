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

#include "emboss_util.h"
#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/hci_proxy.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

// Util functions

// Simple struct for returning a H4 array and an emboss view on its HCI packet.
template <typename EmbossT>
struct EmbossViewWithH4Buffer {
  // Size is +1 to accomidate the H4 packet type at the front.
  std::array<uint8_t, EmbossT::SizeInBytes() + 1> arr;
  EmbossT view;
};

// Return H4 command buffer and Emboss view using indicated Emboss type to send
// towards controller.
template <typename EmbossT>
EmbossViewWithH4Buffer<EmbossT> CreateToControllerBuffer(
    emboss::OpCode opcode) {
  EmbossViewWithH4Buffer<EmbossT> view_arr;
  std::iota(view_arr.arr.begin(), view_arr.arr.end(), 100);
  view_arr.arr[0] = cpp23::to_underlying(emboss::H4PacketType::COMMAND);
  view_arr.view = MakeEmboss<EmbossT>(H4HciSubspan(view_arr.arr));
  EXPECT_TRUE(view_arr.view.IsComplete());
  view_arr.view.header().opcode_full().Write(opcode);
  return view_arr;
}

// Populate an H4 buffer to send towards controller. Type should be one
// that proxy host doesn't interact with.
std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
CreateNoninteractingToControllerBuffer() {
  EmbossViewWithH4Buffer<emboss::InquiryCommandWriter> view_arr =
      CreateToControllerBuffer<emboss::InquiryCommandWriter>(
          emboss::OpCode::LINK_KEY_REQUEST_REPLY);
  return view_arr.arr;
}

// Return H4 event buffer and Emboss view using indicated Emboss type to send
// towards host.
template <typename EmbossT>
EmbossViewWithH4Buffer<EmbossT> CreateToHostBuffer(
    uint event_code, emboss::StatusCode status_code) {
  EmbossViewWithH4Buffer<EmbossT> view_arr;
  std::iota(view_arr.arr.begin(), view_arr.arr.end(), 100);
  view_arr.arr[0] = cpp23::to_underlying(emboss::H4PacketType::EVENT);
  view_arr.view = MakeEmboss<EmbossT>(H4HciSubspan(view_arr.arr));
  view_arr.view.header().event_code().Write(event_code);
  view_arr.view.status().Write(status_code);
  EXPECT_TRUE(view_arr.view.IsComplete());
  return view_arr;
}

// Populate an H4 buffer to send towards host. Type should be one
// that proxy host doesn't interact with.
std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
CreateNonInteractingToHostBuffer() {
  EmbossViewWithH4Buffer<emboss::InquiryCompleteEventWriter> view_arr =
      CreateToHostBuffer<emboss::InquiryCompleteEventWriter>(
          0x01, emboss::StatusCode::COMMAND_DISALLOWED);
  return view_arr.arr;
}

// Tests

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

#include "pw_bluetooth_proxy/hci_proxy.h"

  // Container creates HciProxy .
  HciProxy proxy = HciProxy(std::move(containerSendToHostFn),
                            std::move(containerSendToControllerFn));

  // Container passes H4 packets from host through proxy. Proxy will in turn
  // call the container-provided `containerSendToControllerFn` to pass them on
  // to the controller. Some packets may be modified, added, or removed.
  proxy.ProcessH4HciFromHost(h4_span_from_host);

  // Container passes H4 packets from controller through proxy. Proxy will in
  // turn call the container-provided `containerSendToHostFn` to pass them on to
  // the controller. Some packets may be modified, added, or removed.
  proxy.ProcessH4HciFromController(h4_span_from_controller);

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]
}

// Verify buffer is properly passed (is equal value).
TEST(PassthroughTest, ToControllerPassesEqualBuffer) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array =
      CreateNoninteractingToControllerBuffer();

  // Outbound callbacks where we verify packet is equal (just testing to
  // controller in this test).
  bool send_called = false;
  auto send_capture = std::pair(&send_called, &h4_array);
  H4HciPacketSendFn send_to_controller_fn([&send_capture](H4HciPacket packet) {
    *(send_capture.first) = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.second->begin(),
                           send_capture.second->end()));
  });

  H4HciPacketSendFn send_to_host_fn([]([[maybe_unused]] H4HciPacket packet) {});

  HciProxy proxy =
      HciProxy(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.ProcessH4HciFromHost(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

// Verify buffer is passed to host callback with the same contents.
TEST(PassthroughTest, ToHostPassesEqualBuffer) {
  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array = CreateNonInteractingToHostBuffer();

  // Outbound callbacks where we verify packet is equal (just testing to host
  // in this test).
  bool send_called = false;
  auto send_capture = std::pair(&send_called, &h4_array);
  H4HciPacketSendFn send_to_host_fn([&send_capture](H4HciPacket packet) {
    *(send_capture.first) = true;
    EXPECT_TRUE(std::equal(packet.begin(),
                           packet.end(),
                           send_capture.second->begin(),
                           send_capture.second->end()));
  });

  H4HciPacketSendFn send_to_controller_fn(
      []([[maybe_unused]] H4HciPacket packet) {});

  HciProxy proxy =
      HciProxy(std::move(send_to_host_fn), std::move(send_to_controller_fn));

  proxy.ProcessH4HciFromController(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
