// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_sco_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/socket/socket_factory.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/fake_sco_data_channel.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::sco {
namespace {

hci_spec::ConnectionHandle kConnectionHandle = 1u;
hci_spec::ConnectionHandle kConnectionHandle2 = 2u;

constexpr uint16_t kHciScoMtu = 1;

// Default data buffer information used by ScoDataChannel.
static constexpr size_t kMaxScoPacketLength = 255;
static constexpr size_t kMaxScoPacketCount = 1;
const hci::DataBufferInfo kDataBufferInfo(kMaxScoPacketLength, kMaxScoPacketCount);

using TestingBase = testing::ControllerTest<testing::MockController>;
class ScoConnectionTest : public TestingBase {
 public:
  ScoConnectionTest() = default;
  ~ScoConnectionTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();
    InitializeScoDataChannel(kDataBufferInfo);
    StartTestDevice();

    set_configure_sco_cb([](auto, auto, auto, auto cb) { cb(ZX_OK); });
    set_reset_sco_cb([](auto cb) { cb(ZX_OK); });

    auto conn = std::make_unique<hci::ScoConnection>(kConnectionHandle, DeviceAddress(),
                                                     DeviceAddress(), transport()->WeakPtr());
    hci_conn_ = conn->GetWeakPtr();
    deactivated_cb_count_ = 0;
    sco_conn_ = CreateScoConnection(std::move(conn));
  }

  void TearDown() override {
    sco_conn_ = nullptr;
    TestingBase::TearDown();
  }

  virtual std::unique_ptr<ScoConnection> CreateScoConnection(
      std::unique_ptr<hci::Connection> hci_conn) {
    return std::make_unique<ScoConnection>(
        std::move(hci_conn), [this] { OnDeactivated(); },
        hci_spec::SynchronousConnectionParameters(), /*channel=*/nullptr);
  }

  void OnDeactivated() { deactivated_cb_count_++; }

  ScoConnection* sco_conn() { return sco_conn_.get(); }

  auto hci_conn() { return hci_conn_; }

  size_t deactivated_count() const { return deactivated_cb_count_; }

 private:
  size_t deactivated_cb_count_;
  std::unique_ptr<ScoConnection> sco_conn_;
  fxl::WeakPtr<hci::ScoConnection> hci_conn_;
};

class HciScoConnectionTest : public ScoConnectionTest {
 public:
  std::unique_ptr<ScoConnection> CreateScoConnection(
      std::unique_ptr<hci::Connection> hci_conn) override {
    constexpr hci_spec::SynchronousConnectionParameters hci_conn_params{
        .input_data_path = hci_spec::ScoDataPath::kHci,
        .output_data_path = hci_spec::ScoDataPath::kHci,
    };
    return std::make_unique<ScoConnection>(
        std::move(hci_conn), [this] { OnDeactivated(); }, hci_conn_params,
        transport()->sco_data_channel());
  }
};

class HciScoConnectionTestWithFakeScoChannel : public ScoConnectionTest {
 public:
  std::unique_ptr<ScoConnection> CreateScoConnection(
      std::unique_ptr<hci::Connection> hci_conn) override {
    channel_ = std::make_unique<hci::FakeScoDataChannel>(/*mtu=*/kHciScoMtu);

    constexpr hci_spec::SynchronousConnectionParameters hci_conn_params{
        .input_data_path = hci_spec::ScoDataPath::kHci,
        .output_data_path = hci_spec::ScoDataPath::kHci,
    };
    return std::make_unique<ScoConnection>(
        std::move(hci_conn), [this] { OnDeactivated(); }, hci_conn_params, channel_.get());
  }

  hci::FakeScoDataChannel* fake_sco_chan() { return channel_.get(); }

 private:
  std::unique_ptr<hci::FakeScoDataChannel> channel_;
};

TEST_F(ScoConnectionTest, Send) {
  EXPECT_FALSE(sco_conn()->Send(nullptr));
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, MaxTxSduSize) {
  EXPECT_EQ(sco_conn()->max_tx_sdu_size(), 0u);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, ActivateAndDeactivate) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  EXPECT_TRUE(hci_conn());

  sco_conn()->Deactivate();
  EXPECT_EQ(close_count, 0u);
  EXPECT_EQ(deactivated_count(), 1u);
  EXPECT_FALSE(hci_conn());

  // Deactivating should be idempotent.
  sco_conn()->Deactivate();
  EXPECT_EQ(close_count, 0u);
  EXPECT_EQ(deactivated_count(), 1u);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel,
       ActivateAndDeactivateRegistersAndUnregistersConnection) {
  EXPECT_TRUE(fake_sco_chan()->connections().empty());

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);
  EXPECT_EQ(fake_sco_chan()->connections().begin()->first, sco_conn()->handle());

  sco_conn()->Deactivate();
  EXPECT_TRUE(fake_sco_chan()->connections().empty());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, ActivateAndClose) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  EXPECT_TRUE(hci_conn());

  sco_conn()->Close();
  EXPECT_EQ(close_count, 1u);
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_FALSE(hci_conn());

  // Closing should be idempotent.
  sco_conn()->Close();
  EXPECT_EQ(close_count, 1u);
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, ActivateAndCloseRegistersAndUnregistersConnection) {
  EXPECT_TRUE(fake_sco_chan()->connections().empty());

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);
  EXPECT_EQ(fake_sco_chan()->connections().begin()->first, sco_conn()->handle());

  sco_conn()->Close();
  EXPECT_TRUE(fake_sco_chan()->connections().empty());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, UniqueId) {
  EXPECT_EQ(sco_conn()->unique_id(), kConnectionHandle);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, CloseWithoutActivating) {
  EXPECT_TRUE(hci_conn());
  sco_conn()->Close();
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_FALSE(hci_conn());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, CloseWithoutActivatingDoesNotUnregister) {
  sco_conn()->Close();
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(ScoConnectionTest, ActivateAndPeerDisconnectDeactivates) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  ASSERT_TRUE(hci_conn());

  test_device()->SendCommandChannelPacket(
      bt::testing::DisconnectionCompletePacket(kConnectionHandle));
  RunLoopUntilIdle();
  EXPECT_EQ(close_count, 1u);
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_FALSE(hci_conn());
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, ReceiveTwoPackets) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  std::vector<std::unique_ptr<hci::ScoDataPacket>> packets;
  auto rx_callback = [&packets, sco_conn = sco_conn()->GetWeakPtr()]() {
    std::unique_ptr<hci::ScoDataPacket> packet = sco_conn->Read();
    ASSERT_TRUE(packet);
    packets.push_back(std::move(packet));
  };

  EXPECT_TRUE(sco_conn()->Activate(std::move(rx_callback), std::move(closed_cb)));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  EXPECT_FALSE(sco_conn()->Read());

  StaticByteBuffer packet_buffer_0(
      LowerBits(kConnectionHandle),
      UpperBits(kConnectionHandle) | 0x30,  // handle + packet status flag: kDataPartiallyLost
      0x01,                                 // payload length
      0x00                                  // payload
  );
  std::unique_ptr<hci::ScoDataPacket> packet_0 = hci::ScoDataPacket::New(/*payload_size=*/1);
  packet_0->mutable_view()->mutable_data().Write(packet_buffer_0);
  packet_0->InitializeFromBuffer();
  sco_conn()->ReceiveInboundPacket(std::move(packet_0));

  ASSERT_EQ(packets.size(), 1u);
  EXPECT_FALSE(sco_conn()->Read());
  StaticByteBuffer payload_buffer_0(0x00);
  EXPECT_TRUE(ContainersEqual(packets[0]->view().payload_data(), payload_buffer_0));
  EXPECT_EQ(packets[0]->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kDataPartiallyLost);

  StaticByteBuffer packet_buffer_1(
      LowerBits(kConnectionHandle),
      UpperBits(kConnectionHandle),  // handle + packet status flag: kCorrectlyReceived
      0x01,                          // payload length
      0x01                           // payload
  );
  std::unique_ptr<hci::ScoDataPacket> packet_1 = hci::ScoDataPacket::New(/*payload_size=*/1);
  packet_1->mutable_view()->mutable_data().Write(packet_buffer_1);
  packet_1->InitializeFromBuffer();
  sco_conn()->ReceiveInboundPacket(std::move(packet_1));

  ASSERT_EQ(packets.size(), 2u);
  EXPECT_FALSE(sco_conn()->Read());
  auto payload_buffer_1 = StaticByteBuffer(0x01);
  EXPECT_TRUE(ContainersEqual(packets[1]->view().payload_data(), payload_buffer_1));
  EXPECT_EQ(packets[1]->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, SendPackets) {
  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  const auto packet_buffer_0 = StaticByteBuffer(LowerBits(kConnectionHandle),
                                                UpperBits(kConnectionHandle),  // handle
                                                0x01,                          // payload length
                                                0x00                           // payload
  );
  const BufferView payload_view_0 = packet_buffer_0.view(sizeof(hci_spec::SynchronousDataHeader));
  sco_conn()->Send(std::make_unique<DynamicByteBuffer>(payload_view_0));

  auto packet_buffer_1 = StaticByteBuffer(LowerBits(kConnectionHandle),
                                          UpperBits(kConnectionHandle),  // handle
                                          0x01,                          // payload length
                                          0x01                           // payload
  );
  BufferView payload_view_1 = packet_buffer_1.view(sizeof(hci_spec::SynchronousDataHeader));
  sco_conn()->Send(std::make_unique<DynamicByteBuffer>(payload_view_1));

  EXPECT_EQ(fake_sco_chan()->readable_count(), 1u);
  std::unique_ptr<hci::ScoDataPacket> sent_packet = sco_conn()->GetNextOutboundPacket();
  ASSERT_TRUE(sent_packet);
  EXPECT_TRUE(ContainersEqual(packet_buffer_0, sent_packet->view().data()));

  sent_packet = sco_conn()->GetNextOutboundPacket();
  ASSERT_TRUE(sent_packet);
  EXPECT_TRUE(ContainersEqual(packet_buffer_1, sent_packet->view().data()));

  EXPECT_FALSE(sco_conn()->GetNextOutboundPacket());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, SendPacketLargerThanMtuGetsDropped) {
  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  const auto packet_buffer = StaticByteBuffer(LowerBits(kConnectionHandle),
                                              UpperBits(kConnectionHandle),  // handle
                                              0x02,                          // payload length
                                              0x00, 0x01                     // payload
  );
  const BufferView payload_view = packet_buffer.view(sizeof(hci_spec::SynchronousDataHeader));
  EXPECT_GT(payload_view.size(), sco_conn()->max_tx_sdu_size());
  sco_conn()->Send(std::make_unique<DynamicByteBuffer>(payload_view));

  EXPECT_EQ(fake_sco_chan()->readable_count(), 0u);
  EXPECT_FALSE(sco_conn()->GetNextOutboundPacket());
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTestWithFakeScoChannel, OnHciError) {
  int closed_cb_count = 0;
  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/
                                   [&] {
                                     closed_cb_count++;
                                     sco_conn()->Deactivate();
                                   }));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  sco_conn()->OnHciError();
  EXPECT_EQ(closed_cb_count, 1);
  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle));
}

TEST_F(HciScoConnectionTest, ControllerPacketCountClearedOnPeerDisconnect) {
  size_t close_count_0 = 0;
  auto closed_cb_0 = [&] { close_count_0++; };

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, std::move(closed_cb_0)));
  EXPECT_EQ(close_count_0, 0u);
  ASSERT_TRUE(hci_conn());

  // Fill up the controller buffer.
  ASSERT_EQ(kMaxScoPacketCount, 1u);
  const StaticByteBuffer packet_buffer_0(LowerBits(kConnectionHandle),
                                         UpperBits(kConnectionHandle),  // handle
                                         0x01,                          // payload length
                                         0x00                           // payload
  );
  const BufferView packet_0_payload =
      packet_buffer_0.view(/*pos=*/sizeof(hci_spec::SynchronousDataHeader));
  EXPECT_SCO_PACKET_OUT(test_device(), packet_buffer_0);
  sco_conn()->Send(std::make_unique<DynamicByteBuffer>(packet_0_payload));
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedScoPacketsSent());

  // Queue a packet on a second connection.
  auto hci_conn_1 = std::make_unique<hci::ScoConnection>(kConnectionHandle2, DeviceAddress(),
                                                         DeviceAddress(), transport()->WeakPtr());
  std::unique_ptr<ScoConnection> sco_conn_1 = CreateScoConnection(std::move(hci_conn_1));
  size_t close_count_1 = 0;
  auto closed_cb_1 = [&] { close_count_1++; };
  EXPECT_TRUE(sco_conn_1->Activate(/*rx_callback=*/[] {}, std::move(closed_cb_1)));
  const auto packet_buffer_1 = StaticByteBuffer(LowerBits(kConnectionHandle2),
                                                UpperBits(kConnectionHandle2),  // handle
                                                0x01,                           // payload length
                                                0x01                            // payload
  );
  const BufferView packet_1_payload =
      packet_buffer_1.view(/*pos=*/sizeof(hci_spec::SynchronousDataHeader));
  sco_conn_1->Send(std::make_unique<DynamicByteBuffer>(packet_1_payload));
  RunLoopUntilIdle();

  // Disconnecting the first connection should clear the controller packet count and allow
  // packet_buffer_1 to be sent.
  test_device()->SendCommandChannelPacket(
      bt::testing::DisconnectionCompletePacket(kConnectionHandle));
  EXPECT_SCO_PACKET_OUT(test_device(), packet_buffer_1);
  RunLoopUntilIdle();
  EXPECT_EQ(close_count_0, 1u);
  EXPECT_FALSE(hci_conn());
  EXPECT_EQ(close_count_1, 0u);

  EXPECT_CMD_PACKET_OUT(test_device(), testing::DisconnectPacket(kConnectionHandle2));
  sco_conn_1.reset();
  RunLoopUntilIdle();
  EXPECT_EQ(close_count_1, 0u);
}

}  // namespace
}  // namespace bt::sco
