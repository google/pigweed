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

#include <cstdint>
#include <memory>

#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::hci {

constexpr size_t kDefaultMaxDataLength = 128;
constexpr size_t kDefaultMaxNumPackets = 4;

const DataBufferInfo kDefaultIsoBufferInfo(kDefaultMaxDataLength,
                                           kDefaultMaxNumPackets);

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
    size_t sdu_fragment_size;
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
    size_t sdu_fragment_size = test_vector[test_num].sdu_fragment_size;
    size_t connection_num = test_vector[test_num].connection_num;
    ASSERT_TRUE(connection_num < kNumTotalInterfaces);

    std::unique_ptr<std::vector<uint8_t>> sdu_data =
        testing::GenDataBlob(sdu_fragment_size, /*starting_value=*/test_num);
    DynamicByteBuffer frame = testing::IsoDataPacket(
        /*connection_handle=*/connection_handles[connection_num],
        pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*time_stamp=*/std::nullopt,
        /*packet_sequence_number=*/123,
        /*iso_sdu_length=*/sdu_fragment_size,
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        *sdu_data);
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

TEST_F(IsoDataChannelTests, SendData) {
  ASSERT_NE(iso_data_channel(), nullptr);

  std::unique_ptr<std::vector<uint8_t>> blob = testing::GenDataBlob(9, 0);
  pw::span blob_span(blob->data(), blob->size());

  constexpr hci_spec::ConnectionHandle kIsoHandle = 0x123;
  DynamicByteBuffer packet = testing::IsoDataPacket(
      /*handle = */ kIsoHandle,
      /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
      /*timestamp = */ 0x00000000,
      /*sequence_number = */ 0x0000,
      /*sdu_length = */ blob_span.size(),
      /*status_flag = */
      pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
      /*sdu_data = */ blob_span);

  EXPECT_ISO_PACKET_OUT(test_device(), packet);
  iso_data_channel()->SendData(std::move(packet));
  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());
}

TEST_F(IsoDataChannelTests, SendDataExhaustBuffers) {
  ASSERT_NE(iso_data_channel(), nullptr);

  constexpr hci_spec::ConnectionHandle kIsoHandle = 0x123;
  for (size_t i = 0; i < kDefaultMaxNumPackets; ++i) {
    std::unique_ptr<std::vector<uint8_t>> blob = testing::GenDataBlob(10, i);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x00000000,
        /*sequence_number = */ i,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
    iso_data_channel()->SendData(std::move(packet));
  }

  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());
}

TEST_F(IsoDataChannelTests, SendDataExceedBuffers) {
  constexpr size_t kNumExtraPacketsWithValidCompletedEvent = 2;
  constexpr size_t kNumExtraPacketsWithInvalidCompletedEvent = 2;
  constexpr size_t kNumPackets = kDefaultMaxNumPackets +
                                 kNumExtraPacketsWithValidCompletedEvent +
                                 kNumExtraPacketsWithInvalidCompletedEvent;
  constexpr size_t kSduSize = 15;
  ASSERT_NE(iso_data_channel(), nullptr);

  constexpr hci_spec::ConnectionHandle kIsoHandle = 0x123;
  constexpr hci_spec::ConnectionHandle kOtherHandle = 0x456;
  // Mock interface is not used, only registered to make sure the data channel
  // is aware that the connection is in fact an ISO connection.
  IsoMockConnectionInterface mock_iface;
  EXPECT_TRUE(iso_data_channel()->RegisterConnection(kIsoHandle,
                                                     mock_iface.GetWeakPtr()));
  size_t num_sent = 0;
  size_t num_expectations = 0;

  for (; num_sent < kNumPackets; ++num_sent) {
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kSduSize, num_sent);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x00000000,
        /*sequence_number = */ num_sent,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    if (num_sent < kDefaultMaxNumPackets) {
      ++num_expectations;
      EXPECT_ISO_PACKET_OUT(test_device(), packet);
    }
    iso_data_channel()->SendData(std::move(packet));
  }

  EXPECT_EQ(num_sent, kNumPackets);
  EXPECT_EQ(num_expectations, kDefaultIsoBufferInfo.max_num_packets());
  EXPECT_FALSE(test_device()->AllExpectedIsoPacketsSent());

  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());

  for (size_t i = 0; i < kNumExtraPacketsWithValidCompletedEvent; ++i) {
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kSduSize, num_expectations);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x00000000,
        /*sequence_number = */ num_expectations,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
    ++num_expectations;
  }

  EXPECT_EQ(num_expectations,
            kDefaultMaxNumPackets + kNumExtraPacketsWithValidCompletedEvent);
  RunUntilIdle();
  EXPECT_FALSE(test_device()->AllExpectedIsoPacketsSent());

  // Send a Number_Of_Completed_Packets event for a different connection first.
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(kOtherHandle, 4));

  // Ensure that did not affect available buffers in IsoDataChannel.
  RunUntilIdle();
  EXPECT_FALSE(test_device()->AllExpectedIsoPacketsSent());

  // Send the event for the ISO connection.
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacket(
          kIsoHandle, kNumExtraPacketsWithValidCompletedEvent));

  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());

  // Repeat the above with a Number_Of_Completed_Packets event that has a count
  // larger than expected, to ensure it isn't ignored.
  for (; num_expectations < kNumPackets; ++num_expectations) {
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kSduSize, num_expectations);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x00000000,
        /*sequence_number = */ num_expectations,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
  }

  EXPECT_EQ(num_expectations, kNumPackets);

  // Send the event for the ISO connection.
  test_device()->SendCommandChannelPacket(
      testing::NumberOfCompletedPacketsPacketWithInvalidSize(
          kIsoHandle, kNumExtraPacketsWithInvalidCompletedEvent));

  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());
}

TEST_F(IsoDataChannelTests, OversizedPackets) {
  ASSERT_NE(iso_data_channel(), nullptr);

  constexpr hci_spec::ConnectionHandle kIsoHandle = 0x42;
  constexpr size_t kTimestampSize = 4;
  constexpr size_t kSduHeaderSize = 4;

  constexpr size_t kMaxSizeWithOptional =
      kDefaultMaxDataLength - kTimestampSize - kSduHeaderSize;
  constexpr size_t kMaxSizeNoTimestamp = kDefaultMaxDataLength - kSduHeaderSize;
  constexpr size_t kMaxSizeNoOptional = kDefaultMaxDataLength;

  {
    // Create a packet that is as large as possible and ensure it can be sent.
    // With all possible optional fields.
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeWithOptional, 100);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x12345678,
        /*sequence_number = */ 0,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
    iso_data_channel()->SendData(std::move(packet));
  }

  {
    // Create a packet that is as large as possible and ensure it can be sent.
    // Without timestamp.
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeNoTimestamp, 107);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ std::nullopt,
        /*sequence_number = */ 0,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
    iso_data_channel()->SendData(std::move(packet));
  }

  {
    // Create a packet that is as large as possible and ensure it can be sent.
    // Without any optional field (non-first/complete fragment).
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeNoOptional, 106);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::LAST_FRAGMENT,
        /*timestamp = */ std::nullopt,
        /*sequence_number = */ std::nullopt,
        /*sdu_length = */ std::nullopt,
        /*status_flag = */ std::nullopt,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_ISO_PACKET_OUT(test_device(), packet);
    iso_data_channel()->SendData(std::move(packet));
  }

  {
    // Create a packet that is one byte too large.
    // With all possible optional fields.
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeWithOptional + 1, 54);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ 0x12345678,
        /*sequence_number = */ 0,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_DEATH_IF_SUPPORTED(iso_data_channel()->SendData(std::move(packet)),
                              "Unfragmented packet");
  }

  {
    // Create a packet that is one byte too large.
    // Without timestamp.
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeNoTimestamp + 1, 55);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::COMPLETE_SDU,
        /*timestamp = */ std::nullopt,
        /*sequence_number = */ 0,
        /*sdu_length = */ blob->size(),
        /*status_flag = */
        pw::bluetooth::emboss::IsoDataPacketStatus::VALID_DATA,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_DEATH_IF_SUPPORTED(iso_data_channel()->SendData(std::move(packet)),
                              "Unfragmented packet");
  }

  {
    // Create a packet that is one byte too large.
    // Without any optional field (non-first/complete fragment).
    std::unique_ptr<std::vector<uint8_t>> blob =
        testing::GenDataBlob(kMaxSizeNoOptional + 1, 56);
    DynamicByteBuffer packet = testing::IsoDataPacket(
        /*handle = */ kIsoHandle,
        /*pb_flag = */ pw::bluetooth::emboss::IsoDataPbFlag::LAST_FRAGMENT,
        /*timestamp = */ std::nullopt,
        /*sequence_number = */ std::nullopt,
        /*sdu_length = */ std::nullopt,
        /*status_flag = */ std::nullopt,
        /*sdu_data = */ pw::span(blob->data(), blob->size()));
    EXPECT_DEATH_IF_SUPPORTED(iso_data_channel()->SendData(std::move(packet)),
                              "Unfragmented packet");
  }

  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedIsoPacketsSent());
}

}  // namespace bt::hci
