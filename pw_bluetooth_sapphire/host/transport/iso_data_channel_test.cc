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

#include "pw_bluetooth_sapphire/internal/host/transport/iso_data_channel.h"

#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::hci {

const DataBufferInfo kDefaultIsoBufferInfo(/*max_data_length=*/128,
                                           /*max_num_packets=*/4);

using TestBase = testing::FakeDispatcherControllerTest<testing::MockController>;

class IsoDataChannelTests : public TestBase {
 public:
  void SetUp() override {
    TestBase::SetUp(pw::bluetooth::Controller::FeaturesBits::kHciIso);
    ASSERT_TRUE(transport()->InitializeIsoDataChannel(kDefaultIsoBufferInfo));
  }

  IsoDataChannel* iso_data_channel() { return transport()->iso_data_channel(); }
};

// Placeholder (for now)
class IsoMockConnectionInterface : public IsoDataChannel::ConnectionInterface {
 public:
  IsoMockConnectionInterface() : weak_self_(this) {}
  ~IsoMockConnectionInterface() override = default;

  void ReceiveInboundPacket(pw::span<const std::byte> packet) override {
    received_packets_.emplace(packet);
  }

  std::queue<pw::span<const std::byte>>* received_packets() {
    return &received_packets_;
  }

  using WeakPtr = WeakSelf<IsoMockConnectionInterface>::WeakPtr;
  IsoMockConnectionInterface::WeakPtr GetWeakPtr() {
    return weak_self_.GetWeakPtr();
  }

 private:
  WeakSelf<IsoMockConnectionInterface> weak_self_;
  std::queue<pw::span<const std::byte>> received_packets_;
};

// Verify that we can register and unregister connections
TEST_F(IsoDataChannelTests, RegisterConnections) {
  ASSERT_NE(iso_data_channel(), nullptr);
  IsoMockConnectionInterface mock_iface;
  constexpr hci_spec::ConnectionHandle kIsoHandle1 = 0x123;
  EXPECT_TRUE(iso_data_channel()->RegisterConnection(kIsoHandle1,
                                                     mock_iface.GetWeakPtr()));

  constexpr hci_spec::ConnectionHandle kIsoHandle2 = 0x456;
  EXPECT_TRUE(iso_data_channel()->RegisterConnection(kIsoHandle2,
                                                     mock_iface.GetWeakPtr()));

  // Attempt to re-register a handle fails
  EXPECT_FALSE(iso_data_channel()->RegisterConnection(kIsoHandle1,
                                                      mock_iface.GetWeakPtr()));

  // Can unregister connections that were previously registered
  EXPECT_TRUE(iso_data_channel()->UnregisterConnection(kIsoHandle2));
  EXPECT_TRUE(iso_data_channel()->UnregisterConnection(kIsoHandle1));

  // Cannot unregister connections that never been registered, or that have
  // already been unregistered
  constexpr hci_spec::ConnectionHandle kIsoHandle3 = 0x789;
  EXPECT_FALSE(iso_data_channel()->UnregisterConnection(kIsoHandle3));
  EXPECT_FALSE(iso_data_channel()->UnregisterConnection(kIsoHandle2));
  EXPECT_FALSE(iso_data_channel()->UnregisterConnection(kIsoHandle1));
}

// Verify that data gets directed to the correct connection
TEST_F(IsoDataChannelTests, DataDemuxification) {
  ASSERT_NE(iso_data_channel(), nullptr);

  constexpr uint32_t kNumRegisteredInterfaces = 2;
  constexpr uint32_t kNumUnregisteredInterfaces = 1;
  constexpr uint32_t kNumTotalInterfaces =
      kNumRegisteredInterfaces + kNumUnregisteredInterfaces;
  constexpr hci_spec::ConnectionHandle connection_handles[kNumTotalInterfaces] =
      {0x123, 0x456, 0x789};
  IsoMockConnectionInterface interfaces[kNumTotalInterfaces];
  size_t expected_packet_count[kNumTotalInterfaces] = {0};

  // Register interfaces
  for (uint32_t iface_num = 0; iface_num < kNumRegisteredInterfaces;
       iface_num++) {
    ASSERT_TRUE(iso_data_channel()->RegisterConnection(
        connection_handles[iface_num], interfaces[iface_num].GetWeakPtr()));
    ASSERT_EQ(interfaces[iface_num].received_packets()->size(), 0u);
  }

  constexpr size_t kNumTestPackets = 8;
  struct {
    size_t packet_size;
    size_t connection_num;
  } test_vector[kNumTestPackets] = {
      {100, 0},
      {120, 1},
      {140, 2},
      {160, 0},
      {180, 0},
      {200, 1},
      {220, 1},
      {240, 2},
  };

  // Send frames and verify that they are sent to the correct interfaces (or not
  // sent at all if the connection handle is unregistered).
  for (size_t test_num = 0; test_num < kNumTestPackets; test_num++) {
    size_t packet_size = test_vector[test_num].packet_size;
    size_t connection_num = test_vector[test_num].connection_num;
    ASSERT_TRUE(connection_num < kNumTotalInterfaces);

    DynamicByteBuffer frame =
        testing::IsoDataPacket(packet_size, connection_handles[connection_num]);
    pw::span<const std::byte> frame_as_span = frame.subspan();

    if (connection_num < kNumRegisteredInterfaces) {
      expected_packet_count[connection_num]++;
    }
    test_device()->SendIsoDataChannelPacket(frame_as_span);

    // Check that each of the connection queues has the expected number of
    // packets
    for (size_t interface_num = 0; interface_num < kNumTotalInterfaces;
         interface_num++) {
      EXPECT_EQ(interfaces[interface_num].received_packets()->size(),
                expected_packet_count[interface_num]);
    }
  }
}

}  // namespace bt::hci
