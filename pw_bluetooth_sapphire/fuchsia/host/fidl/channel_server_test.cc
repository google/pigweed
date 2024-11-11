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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/channel_server.h"

#include <gmock/gmock.h>

#include <cstddef>

#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"

using FakeChannel = bt::l2cap::testing::FakeChannel;
using Channel = fuchsia::bluetooth::Channel;
namespace fbt = fuchsia::bluetooth;

namespace bthost {
namespace {

class ChannelServerTest : public bt::testing::TestLoopFixture {
 public:
  ChannelServerTest()
      : fake_chan_(
            /*id=*/1, /*remote_id=*/2, /*handle=*/3, bt::LinkType::kACL) {}

  FakeChannel& fake_chan() { return fake_chan_; }

 private:
  FakeChannel fake_chan_;
};

class ChannelServerChannelActivatedTest : public ChannelServerTest {
 public:
  void SetUp() override {
    ChannelServerTest::SetUp();
    fidl::InterfaceHandle<Channel> handle;
    auto closed_cb = [this]() { server_closed_ = true; };
    server_ = ChannelServer::Create(
        handle.NewRequest(), fake_chan().AsWeakPtr(), std::move(closed_cb));
    ASSERT_TRUE(server_);
    ASSERT_TRUE(fake_chan().activated());
    client_ = handle.Bind();
  }

  void TearDown() override {
    ChannelServerTest::TearDown();
    if (client_.is_bound()) {
      client_.Unbind();
    }
    server_.reset();
    RunLoopUntilIdle();
  }

  fidl::InterfacePtr<Channel>& client() { return client_; }

  bool server_closed() const { return server_closed_; }

  void DestroyServer() { server_.reset(); }

 private:
  bool server_closed_ = false;
  std::unique_ptr<ChannelServer> server_;
  fidl::InterfacePtr<Channel> client_;
};

TEST_F(ChannelServerChannelActivatedTest, SendTwoPackets) {
  std::vector<bt::ByteBufferPtr> sent_packets;
  auto chan_send_cb = [&](bt::ByteBufferPtr buffer) {
    sent_packets.push_back(std::move(buffer));
  };
  fake_chan().SetSendCallback(std::move(chan_send_cb));

  const std::vector<uint8_t> packet_0 = {0x00, 0x01, 0x03};
  const std::vector<uint8_t> packet_1 = {0x04, 0x05, 0x06};
  int send_cb_count = 0;
  std::vector<fbt::Packet> packets{fbt::Packet{packet_0},
                                   fbt::Packet{packet_1}};
  client()->Send(std::move(packets),
                 [&](fuchsia::bluetooth::Channel_Send_Result result) {
                   EXPECT_TRUE(result.is_response());
                   send_cb_count++;
                 });
  RunLoopUntilIdle();
  EXPECT_EQ(send_cb_count, 1);
  ASSERT_EQ(sent_packets.size(), 2u);
  EXPECT_THAT(*sent_packets[0], bt::BufferEq(packet_0));
  EXPECT_THAT(*sent_packets[1], bt::BufferEq(packet_1));
}

TEST_F(ChannelServerChannelActivatedTest, SendTwoPacketsSeparately) {
  std::vector<bt::ByteBufferPtr> sent_packets;
  auto chan_send_cb = [&](bt::ByteBufferPtr buffer) {
    sent_packets.push_back(std::move(buffer));
  };
  fake_chan().SetSendCallback(std::move(chan_send_cb));

  const std::vector<uint8_t> packet_0 = {0x00, 0x01, 0x03};
  int send_cb_count = 0;
  std::vector<fbt::Packet> packets_0{fbt::Packet{packet_0}};
  client()->Send(std::move(packets_0),
                 [&](fuchsia::bluetooth::Channel_Send_Result result) {
                   EXPECT_TRUE(result.is_response());
                   send_cb_count++;
                 });
  RunLoopUntilIdle();
  EXPECT_EQ(send_cb_count, 1);
  ASSERT_EQ(sent_packets.size(), 1u);
  EXPECT_THAT(*sent_packets[0], bt::BufferEq(packet_0));

  const std::vector<uint8_t> packet_1 = {0x04, 0x05, 0x06};
  std::vector<fbt::Packet> packets_1{fbt::Packet{packet_1}};
  client()->Send(std::move(packets_1),
                 [&](fuchsia::bluetooth::Channel_Send_Result result) {
                   EXPECT_TRUE(result.is_response());
                   send_cb_count++;
                 });
  RunLoopUntilIdle();
  EXPECT_EQ(send_cb_count, 2);
  ASSERT_EQ(sent_packets.size(), 2u);
  EXPECT_THAT(*sent_packets[1], bt::BufferEq(packet_1));
}

TEST_F(ChannelServerChannelActivatedTest, SendTooLargePacketDropsPacket) {
  std::vector<bt::ByteBufferPtr> sent_packets;
  auto chan_send_cb = [&](bt::ByteBufferPtr buffer) {
    sent_packets.push_back(std::move(buffer));
  };
  fake_chan().SetSendCallback(std::move(chan_send_cb));

  const std::vector<uint8_t> packet_0(/*count=*/bt::l2cap::kDefaultMTU + 1,
                                      /*value=*/0x03);
  std::vector<fbt::Packet> packets_0{fbt::Packet{packet_0}};
  int send_cb_count = 0;
  client()->Send(packets_0,
                 [&](fuchsia::bluetooth::Channel_Send_Result result) {
                   EXPECT_TRUE(result.is_response());
                   send_cb_count++;
                 });
  RunLoopUntilIdle();
  EXPECT_EQ(send_cb_count, 1);
  ASSERT_EQ(sent_packets.size(), 0u);
}

TEST_F(ChannelServerChannelActivatedTest, ReceiveManyPacketsAndDropSome) {
  for (uint8_t i = 0; i < 2 * ChannelServer::kDefaultReceiveQueueLimit; i++) {
    bt::StaticByteBuffer packet(i, 0x01, 0x02);
    fake_chan().Receive(packet);
  }
  RunLoopUntilIdle();

  std::vector<std::vector<fbt::Packet>> packets;
  for (uint8_t i = 0; i < ChannelServer::kDefaultReceiveQueueLimit; i++) {
    client()->Receive(
        [&packets](::fuchsia::bluetooth::Channel_Receive_Result result) {
          ASSERT_TRUE(result.is_response());
          packets.push_back(std::move(result.response().packets));
        });
    RunLoopUntilIdle();
    ASSERT_EQ(packets.size(), static_cast<size_t>(i + 1));
  }

  // Some packets were dropped, so only the packets under the queue limit should
  // be received.
  size_t first_packet_in_q = ChannelServer::kDefaultReceiveQueueLimit;
  for (size_t i = 0; i < ChannelServer::kDefaultReceiveQueueLimit; i++) {
    std::vector<uint8_t> packet{
        static_cast<uint8_t>(first_packet_in_q + i), 0x01, 0x02};
    ASSERT_EQ(packets[i].size(), 1u);
    EXPECT_EQ(packets[i][0].packet, packet);
  }
}

TEST_F(ChannelServerChannelActivatedTest,
       ReceiveTwiceWithoutResponseClosesConnection) {
  std::optional<zx_status_t> error;
  client().set_error_handler([&](zx_status_t status) { error = status; });
  client()->Receive(
      [](::fuchsia::bluetooth::Channel_Receive_Result result) { FAIL(); });
  client()->Receive(
      [](::fuchsia::bluetooth::Channel_Receive_Result result) { FAIL(); });
  RunLoopUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), ZX_ERR_BAD_STATE);
  client().set_error_handler([](zx_status_t status) {});
}

TEST_F(ChannelServerChannelActivatedTest, ChannelCloses) {
  std::optional<zx_status_t> error;
  client().set_error_handler([&](zx_status_t status) { error = status; });
  fake_chan().Close();
  EXPECT_TRUE(server_closed());
  RunLoopUntilIdle();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error.value(), ZX_ERR_CONNECTION_RESET);
}

TEST_F(ChannelServerChannelActivatedTest, ClientCloses) {
  client().Unbind();
  RunLoopUntilIdle();
  EXPECT_TRUE(server_closed());
  EXPECT_FALSE(fake_chan().activated());
}

TEST_F(ChannelServerTest, ActivateFails) {
  fake_chan().set_activate_fails(true);
  fidl::InterfaceHandle<Channel> handle;
  bool server_closed = false;
  auto closed_cb = [&]() { server_closed = true; };
  std::unique_ptr<ChannelServer> server = ChannelServer::Create(
      handle.NewRequest(), fake_chan().AsWeakPtr(), std::move(closed_cb));
  EXPECT_FALSE(server);
  EXPECT_FALSE(server_closed);
}

TEST_F(ChannelServerChannelActivatedTest, DeactivateOnServerDestruction) {
  EXPECT_TRUE(fake_chan().activated());
  DestroyServer();
  EXPECT_FALSE(fake_chan().activated());
}

}  // namespace
}  // namespace bthost
