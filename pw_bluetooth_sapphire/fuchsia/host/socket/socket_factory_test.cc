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

#include "pw_bluetooth_sapphire/fuchsia/host/socket/socket_factory.h"

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/l2cap/channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_channel.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/testing/loop_fixture.h"
#include "pw_unit_test/framework.h"

namespace bt::socket {
namespace {

// We'll test the template just for L2CAP channels.
using FactoryT = SocketFactory<l2cap::Channel>;

constexpr l2cap::ChannelId kDynamicChannelIdMin = 0x0040;
constexpr l2cap::ChannelId kRemoteChannelId = 0x0050;
constexpr hci_spec::ConnectionHandle kDefaultConnectionHandle = 0x0001;
constexpr hci_spec::ConnectionHandle kAnotherConnectionHandle = 0x0002;

class SocketFactoryTest : public testing::TestLoopFixture {
 public:
  SocketFactoryTest() {
    channel_ =
        std::make_unique<l2cap::testing::FakeChannel>(kDynamicChannelIdMin,
                                                      kRemoteChannelId,
                                                      kDefaultConnectionHandle,
                                                      bt::LinkType::kACL);
  }

  void TearDown() override {
    // Process any pending events, to tickle any use-after-free bugs.
    RunLoopUntilIdle();
  }

 protected:
  l2cap::Channel::WeakPtr channel() { return channel_->GetWeakPtr(); }
  l2cap::testing::FakeChannel::WeakPtr fake_channel() {
    return channel_->AsWeakPtr();
  }

 private:
  std::unique_ptr<l2cap::testing::FakeChannel> channel_;
};

TEST_F(SocketFactoryTest, TemplatesCompile) {
  socket::SocketFactory<l2cap::Channel> l2cap_factory;
}

TEST_F(SocketFactoryTest, CanCreateSocket) {
  FactoryT socket_factory;
  EXPECT_TRUE(socket_factory.MakeSocketForChannel(channel()));
}

TEST_F(SocketFactoryTest, SocketCreationFailsIfChannelIsNullptr) {
  FactoryT socket_factory;
  EXPECT_FALSE(socket_factory.MakeSocketForChannel(l2cap::Channel::WeakPtr()));
}

TEST_F(SocketFactoryTest, SocketCreationFailsIfChannelAlreadyHasASocket) {
  FactoryT socket_factory;
  zx::socket socket = socket_factory.MakeSocketForChannel(channel());
  ASSERT_TRUE(socket);

  EXPECT_FALSE(socket_factory.MakeSocketForChannel(channel()));
}

TEST_F(SocketFactoryTest, SocketCreationFailsIfChannelActivationFails) {
  fake_channel()->set_activate_fails(true);
  EXPECT_FALSE(FactoryT().MakeSocketForChannel(channel()));
}

TEST_F(SocketFactoryTest, CanCreateSocketForNewChannelWithRecycledId) {
  FactoryT socket_factory;
  auto original_channel =
      std::make_unique<l2cap::testing::FakeChannel>(kDynamicChannelIdMin + 1,
                                                    kRemoteChannelId,
                                                    kDefaultConnectionHandle,
                                                    bt::LinkType::kACL);
  zx::socket socket =
      socket_factory.MakeSocketForChannel(original_channel->GetWeakPtr());
  ASSERT_TRUE(socket);
  original_channel->Close();
  original_channel.reset();
  RunLoopUntilIdle();  // Process any events related to channel closure.

  auto new_channel =
      std::make_unique<l2cap::testing::FakeChannel>(kDynamicChannelIdMin + 1,
                                                    kRemoteChannelId,
                                                    kDefaultConnectionHandle,
                                                    bt::LinkType::kACL);
  EXPECT_TRUE(socket_factory.MakeSocketForChannel(new_channel->GetWeakPtr()));
  new_channel->Close();
  original_channel.reset();
}

TEST_F(SocketFactoryTest, DestructionWithActiveRelayDoesNotCrash) {
  FactoryT socket_factory;
  zx::socket socket = socket_factory.MakeSocketForChannel(channel());
  ASSERT_TRUE(socket);
  // |socket_factory| is destroyed implicitly.
}

TEST_F(SocketFactoryTest, DestructionAfterDeactivatingRelayDoesNotCrash) {
  FactoryT socket_factory;
  zx::socket socket = socket_factory.MakeSocketForChannel(channel());
  ASSERT_TRUE(socket);
  fake_channel()->Close();
  RunLoopUntilIdle();  // Process any events related to channel closure.
  // |socket_factory| is destroyed implicitly.
}

TEST_F(SocketFactoryTest, SameChannelIdDifferentHandles) {
  FactoryT socket_factory;
  EXPECT_TRUE(socket_factory.MakeSocketForChannel(channel()));
  auto another_channel =
      std::make_unique<l2cap::testing::FakeChannel>(kDynamicChannelIdMin,
                                                    kRemoteChannelId,
                                                    kAnotherConnectionHandle,
                                                    bt::LinkType::kACL);
  EXPECT_TRUE(
      socket_factory.MakeSocketForChannel(another_channel->GetWeakPtr()));
  another_channel->Close();
}

TEST_F(SocketFactoryTest, ClosedCallbackCalledOnChannelClosure) {
  FactoryT socket_factory;
  int closed_cb_count = 0;
  auto closed_cb = [&]() { closed_cb_count++; };
  zx::socket sock =
      socket_factory.MakeSocketForChannel(channel(), std::move(closed_cb));
  EXPECT_TRUE(sock);
  fake_channel()->Close();
  EXPECT_EQ(closed_cb_count, 1);
}

TEST_F(SocketFactoryTest, ClosedCallbackCalledOnSocketClosure) {
  FactoryT socket_factory;
  int closed_cb_count = 0;
  auto closed_cb = [&]() { closed_cb_count++; };
  zx::socket sock =
      socket_factory.MakeSocketForChannel(channel(), std::move(closed_cb));
  EXPECT_TRUE(sock);
  sock.reset();
  RunLoopUntilIdle();
  EXPECT_EQ(closed_cb_count, 1);
}

}  // namespace
}  // namespace bt::socket
