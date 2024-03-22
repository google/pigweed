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
#include <type_traits>

#include "pw_bluetooth/hci_commands.emb.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_bluetooth_proxy/hci_proxy.h"
#include "pw_bluetooth_proxy/passthrough_policy.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {

namespace {

// Util functions

// Equivalent to C++23 std::to_underlying.
// TODO: b/330064223 - Move to pw::to_underlying once landed.
template <typename E>
constexpr auto to_underlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

// Create pw::span on the HCI portion of an H4 buffer.
template <typename C>
constexpr const pw::span<uint8_t> HciSubspanOfH4Buffer(C&& container) {
  return pw::span(container.data() + 1, container.size() - 1);
}

// Populate H4 buffer to send towards controller.
std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
CreateToControllerBuffer() {
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array;
  std::iota(h4_array.begin(), h4_array.end(), 100);
  h4_array[0] = to_underlying(emboss::H4PacketType::COMMAND);
  const auto hci_span = HciSubspanOfH4Buffer(h4_array);
  auto packet_view = emboss::MakeInquiryCommandView(&hci_span);
  EXPECT_TRUE(packet_view.IsComplete());
  // TODO: b/330064223 - Move to using opcode constants once we have them in
  // pw_bluetooth.
  packet_view.header().opcode().ogf().Write(0x01);    // Link Control commands
  packet_view.header().opcode().ocf().Write(0x0001);  // Inquiry Command
  return h4_array;
}

// Populate H4 buffer to send towards host.
std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
CreateToHostBuffer() {
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array;
  std::iota(h4_array.begin(), h4_array.end(), 100);
  h4_array[0] = to_underlying(emboss::H4PacketType::EVENT);
  const auto hci_span = HciSubspanOfH4Buffer(h4_array);
  auto packet_view = emboss::MakeInquiryCompleteEventView(&hci_span);
  EXPECT_TRUE(packet_view.IsComplete());
  // TODO: b/330064223 - Move to using event code constants once we have them in
  // pw_bluetooth.
  packet_view.header().event_code().Write(0x01);
  packet_view.status().Write(emboss::StatusCode::COMMAND_DISALLOWED);
  return h4_array;
}

// Tests

// Example for docs.rst.
TEST(ExampleUsage, Example) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1>
      h4_array_from_host = CreateToControllerBuffer();
  auto h4_span_from_host = pw::span(h4_array_from_host);

  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array_from_controller = CreateToHostBuffer();
  auto h4_span_from_controller = pw::span(h4_array_from_controller);

  H4HciPacketSendFn containerSendToHostFn([](H4HciPacket packet) {});

  H4HciPacketSendFn containerSendToControllerFn(([](H4HciPacket packet) {}));

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]

#include "pw_bluetooth_proxy/hci_proxy.h"
#include "pw_bluetooth_proxy/passthrough_policy.h"

  // Container creates proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy{};
  std::array<ProxyPolicy*, 1> policies{&passthrough_policy};
  HciProxy proxy = HciProxy(std::move(containerSendToHostFn),
                            std::move(containerSendToControllerFn),
                            policies);

  // Container passes H4 packets from host through proxy. Proxy will in turn
  // call the container-provided `containerSendToControllerFn` to pass them on
  // to the controller. Note depending on the policies, some packets may be
  // modified, added, or removed.
  proxy.ProcessH4HciFromHost(h4_span_from_host);

  // Container passes H4 packets from controller through proxy. Proxy will in
  // turn call the container-provided `containerSendToHostFn` to pass them
  // on to the controller. Note depending on the policies, some packets may be
  // modified, added, or removed.
  proxy.ProcessH4HciFromController(h4_span_from_controller);

  // DOCSTAG: [pw_bluetooth_proxy-examples-basic]
}

// Verify buffer is properly passed (is equal value) when we have no policies.
TEST(WithNoPoliciesTheToControllerPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array =
      CreateToControllerBuffer();

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

  // Create proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy{};
  std::array<ProxyPolicy*, 0> policies{};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromHost(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(WithOnePolicyTheToControllerPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array =
      CreateToControllerBuffer();

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

  // Create proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy{};
  std::array<ProxyPolicy*, 1> policies{&passthrough_policy};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromHost(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(WithThreePoliciesTheToControllerPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards controller.
  std::array<uint8_t, emboss::InquiryCommandView::SizeInBytes() + 1> h4_array =
      CreateToControllerBuffer();

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

  // Create proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy1{};
  PassthroughPolicy passthrough_policy2{};
  PassthroughPolicy passthrough_policy3{};
  std::array<ProxyPolicy*, 3> policies{
      &passthrough_policy1, &passthrough_policy2, &passthrough_policy3};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromHost(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

// Verify buffer is passed to host callback with the same contents with no
// policies.
TEST(WithNoPoliciesTheToHostPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array = CreateToHostBuffer();

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

  // Create proxy with simple passthrough policy.
  std::array<ProxyPolicy*, 0> policies{};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromController(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(WithOnePolicyTheToHostPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array = CreateToHostBuffer();

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

  // Create proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy{};
  std::array<ProxyPolicy*, 1> policies{&passthrough_policy};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromController(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

TEST(WithThreePoliciesTheToHostPassesEqualBuffer, PassthroughTest) {
  // Populate H4 buffer to send towards host.
  std::array<uint8_t, emboss::InquiryCompleteEventView::SizeInBytes() + 1>
      h4_array = CreateToHostBuffer();

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

  // Create proxy with simple passthrough policy.
  PassthroughPolicy passthrough_policy1{};
  PassthroughPolicy passthrough_policy2{};
  PassthroughPolicy passthrough_policy3{};
  std::array<ProxyPolicy*, 3> policies{
      &passthrough_policy1, &passthrough_policy2, &passthrough_policy3};
  HciProxy proxy = HciProxy(
      std::move(send_to_host_fn), std::move(send_to_controller_fn), policies);

  proxy.ProcessH4HciFromController(pw::span(h4_array));

  // Verify to controller callback was called.
  EXPECT_EQ(send_called, true);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
