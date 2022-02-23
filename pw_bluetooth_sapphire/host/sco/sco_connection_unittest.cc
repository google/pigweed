// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection.h"

#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/socket/socket_factory.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/fake_sco_data_channel.h"

namespace bt::sco {
namespace {

hci_spec::ConnectionHandle kConnectionHandle = 1u;

constexpr uint16_t kHciScoMtu = 1;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;
class ScoConnectionTest : public TestingBase {
 public:
  ScoConnectionTest() = default;
  ~ScoConnectionTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    auto fake_conn = std::make_unique<hci::testing::FakeConnection>(
        kConnectionHandle, bt::LinkType::kSCO, hci::Connection::Role::kCentral, DeviceAddress(),
        DeviceAddress());
    hci_conn_ = fake_conn->WeakPtr();
    fake_conn_ = fake_conn.get();
    deactivated_cb_count_ = 0;
    sco_conn_ = CreateScoConnection(std::move(fake_conn));
  }

  void TearDown() override {
    sco_conn_ = nullptr;
    TestingBase::TearDown();
  }

  virtual fbl::RefPtr<ScoConnection> CreateScoConnection(
      std::unique_ptr<hci::Connection> hci_conn) {
    return ScoConnection::Create(
        std::move(hci_conn), [this] { OnDeactivated(); },
        hci_spec::SynchronousConnectionParameters(), /*channel=*/nullptr);
  }

  void OnDeactivated() { deactivated_cb_count_++; }

  auto sco_conn() { return sco_conn_; }

  auto hci_conn() { return hci_conn_; }

  auto fake_conn() { return fake_conn_; }

  size_t deactivated_count() const { return deactivated_cb_count_; }

 private:
  size_t deactivated_cb_count_;
  fbl::RefPtr<ScoConnection> sco_conn_;
  fxl::WeakPtr<hci::Connection> hci_conn_;
  hci::testing::FakeConnection* fake_conn_;
};

class HciScoConnectionTest : public ScoConnectionTest {
 public:
  fbl::RefPtr<ScoConnection> CreateScoConnection(
      std::unique_ptr<hci::Connection> hci_conn) override {
    channel_ = std::make_unique<hci::FakeScoDataChannel>(/*mtu=*/kHciScoMtu);

    constexpr hci_spec::SynchronousConnectionParameters hci_conn_params{
        .input_data_path = hci_spec::ScoDataPath::kHci,
        .output_data_path = hci_spec::ScoDataPath::kHci,
    };
    return ScoConnection::Create(
        std::move(hci_conn), [this] { OnDeactivated(); }, hci_conn_params, channel_.get());
  }

  hci::FakeScoDataChannel* fake_sco_chan() { return channel_.get(); }

 private:
  std::unique_ptr<hci::FakeScoDataChannel> channel_;
};

TEST_F(ScoConnectionTest, Send) { EXPECT_FALSE(sco_conn()->Send(nullptr)); }

TEST_F(ScoConnectionTest, MaxTxSduSize) { EXPECT_EQ(sco_conn()->max_tx_sdu_size(), 0u); }

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
}

TEST_F(HciScoConnectionTest, ActivateAndDeactivateRegistersAndUnregistersConnection) {
  EXPECT_TRUE(fake_sco_chan()->connections().empty());

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);
  EXPECT_EQ(fake_sco_chan()->connections().begin()->first, sco_conn()->handle());

  sco_conn()->Deactivate();
  EXPECT_TRUE(fake_sco_chan()->connections().empty());
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
}

TEST_F(HciScoConnectionTest, ActivateAndCloseRegistersAndUnregistersConnection) {
  EXPECT_TRUE(fake_sco_chan()->connections().empty());

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/[] {}));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);
  EXPECT_EQ(fake_sco_chan()->connections().begin()->first, sco_conn()->handle());

  sco_conn()->Close();
  EXPECT_TRUE(fake_sco_chan()->connections().empty());
}

TEST_F(ScoConnectionTest, UniqueId) { EXPECT_EQ(sco_conn()->unique_id(), kConnectionHandle); }

TEST_F(ScoConnectionTest, CloseWithoutActivating) {
  EXPECT_TRUE(hci_conn());
  sco_conn()->Close();
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_FALSE(hci_conn());
}

TEST_F(HciScoConnectionTest, CloseWithoutActivatingDoesNotUnregister) { sco_conn()->Close(); }

TEST_F(ScoConnectionTest, ActivateAndPeerDisconnectDeactivates) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  ASSERT_TRUE(hci_conn());

  fake_conn()->TriggerPeerDisconnectCallback();
  EXPECT_EQ(close_count, 1u);
  EXPECT_EQ(deactivated_count(), 0u);
  EXPECT_FALSE(hci_conn());
}

TEST_F(HciScoConnectionTest, ReceiveTwoPackets) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  std::vector<std::unique_ptr<hci::ScoDataPacket>> packets;
  auto rx_callback = [&packets, sco_conn = sco_conn().get()]() {
    std::unique_ptr<hci::ScoDataPacket> packet = sco_conn->Read();
    ASSERT_TRUE(packet);
    packets.push_back(std::move(packet));
  };

  EXPECT_TRUE(sco_conn()->Activate(std::move(rx_callback), std::move(closed_cb)));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  EXPECT_FALSE(sco_conn()->Read());

  auto packet_buffer_0 = StaticByteBuffer(
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
  auto payload_buffer_0 = StaticByteBuffer(0x00);
  EXPECT_TRUE(ContainersEqual(packets[0]->view().payload_data(), payload_buffer_0));
  EXPECT_EQ(packets[0]->packet_status_flag(),
            hci_spec::SynchronousDataPacketStatusFlag::kDataPartiallyLost);

  auto packet_buffer_1 = StaticByteBuffer(
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
}

TEST_F(HciScoConnectionTest, SendPackets) {
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
}

TEST_F(HciScoConnectionTest, SendPacketLargerThanMtuGetsDropped) {
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
}

TEST_F(HciScoConnectionTest, OnHciError) {
  int closed_cb_count = 0;
  EXPECT_TRUE(sco_conn()->Activate(/*rx_callback=*/[]() {}, /*closed_callback=*/
                                   [&] {
                                     closed_cb_count++;
                                     sco_conn()->Deactivate();
                                   }));
  ASSERT_EQ(fake_sco_chan()->connections().size(), 1u);

  sco_conn()->OnHciError();
  EXPECT_EQ(closed_cb_count, 1);
}

}  // namespace
}  // namespace bt::sco
