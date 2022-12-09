// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fidl_controller.h"

#include <fuchsia/hardware/bluetooth/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include "gmock/gmock.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/slab_allocators.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::controllers {

class FakeHciServer final : public fuchsia::hardware::bluetooth::testing::Hci_TestBase {
 public:
  FakeHciServer(fidl::InterfaceRequest<fuchsia::hardware::bluetooth::Hci> request,
                async_dispatcher_t* dispatcher)
      : dispatcher_(dispatcher) {
    binding_.Bind(std::move(request));
  }

  void Unbind() { binding_.Unbind(); }

  zx_status_t SendEvent(BufferView event) {
    return command_channel_.write(/*flags=*/0, event.data(), static_cast<uint32_t>(event.size()),
                                  /*handles=*/nullptr, /*num_handles=*/0);
  }
  zx_status_t SendAcl(BufferView buffer) {
    return acl_channel_.write(/*flags=*/0, buffer.data(), static_cast<uint32_t>(buffer.size()),
                              /*handles=*/nullptr, /*num_handles=*/0);
  }

  const std::vector<bt::DynamicByteBuffer>& commands_received() const { return commands_received_; }
  const std::vector<bt::DynamicByteBuffer>& acl_packets_received() const {
    return acl_packets_received_;
  }

  bool CloseAclChannel() {
    bool was_valid = acl_channel_.is_valid();
    acl_channel_.reset();
    return was_valid;
  }

  bool acl_channel_valid() const { return acl_channel_.is_valid(); }
  bool command_channel_valid() const { return command_channel_.is_valid(); }

 private:
  void OpenCommandChannel(zx::channel channel) override {
    command_channel_ = std::move(channel);
    InitializeWait(command_wait_, command_channel_);
  }

  void OpenAclDataChannel(zx::channel channel) override {
    acl_channel_ = std::move(channel);
    InitializeWait(acl_wait_, acl_channel_);
  }

  void NotImplemented_(const std::string& name) override { FAIL() << name << " not implemented"; }

  void InitializeWait(async::WaitBase& wait, zx::channel& channel) {
    BT_ASSERT(channel.is_valid());
    wait.Cancel();
    wait.set_object(channel.get());
    wait.set_trigger(ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED);
    BT_ASSERT(wait.Begin(dispatcher_) == ZX_OK);
  }

  void OnAclSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                   const zx_packet_signal_t* signal) {
    ASSERT_TRUE(status == ZX_OK);
    if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
      acl_channel_.reset();
      return;
    }
    ASSERT_TRUE(signal->observed & ZX_CHANNEL_READABLE);

    bt::StaticByteBuffer<hci::allocators::kLargeACLDataPacketSize> buffer;
    uint32_t read_size = 0;
    zx_status_t read_status = acl_channel_.read(0u, buffer.mutable_data(), /*handles=*/nullptr,
                                                static_cast<uint32_t>(buffer.size()), 0, &read_size,
                                                /*actual_handles=*/nullptr);
    ASSERT_TRUE(read_status == ZX_OK);
    acl_packets_received_.emplace_back(bt::BufferView(buffer, read_size));
    acl_wait_.Begin(dispatcher_);
  }

  void OnCommandSignal(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                       const zx_packet_signal_t* signal) {
    ASSERT_TRUE(status == ZX_OK);
    if (signal->observed & ZX_CHANNEL_PEER_CLOSED) {
      command_channel_.reset();
      return;
    }
    ASSERT_TRUE(signal->observed & ZX_CHANNEL_READABLE);

    bt::StaticByteBuffer<hci::allocators::kLargeControlPacketSize> buffer;
    uint32_t read_size = 0;
    zx_status_t read_status =
        command_channel_.read(0u, buffer.mutable_data(), /*handles=*/nullptr,
                              static_cast<uint32_t>(buffer.size()), 0, &read_size,
                              /*actual_handles=*/nullptr);
    ASSERT_TRUE(read_status == ZX_OK);
    commands_received_.emplace_back(bt::BufferView(buffer, read_size));
    command_wait_.Begin(dispatcher_);
  }

  fidl::Binding<fuchsia::hardware::bluetooth::Hci> binding_{this};

  zx::channel command_channel_;
  std::vector<bt::DynamicByteBuffer> commands_received_;

  zx::channel acl_channel_;
  std::vector<bt::DynamicByteBuffer> acl_packets_received_;

  async::WaitMethod<FakeHciServer, &FakeHciServer::OnAclSignal> acl_wait_{this};
  async::WaitMethod<FakeHciServer, &FakeHciServer::OnCommandSignal> command_wait_{this};

  async_dispatcher_t* dispatcher_;
};

class FidlControllerTest : public ::gtest::TestLoopFixture {
 public:
  void SetUp() {
    fuchsia::hardware::bluetooth::HciHandle hci;
    fake_hci_server_.emplace(hci.NewRequest(), dispatcher());
    fidl_controller_.emplace(std::move(hci), dispatcher());
  }

  void InitializeController() {
    std::optional<pw::Status> complete_status;
    controller()->Initialize(
        [&](pw::Status cb_complete_status) { complete_status = cb_complete_status; },
        [&](pw::Status cb_error) { controller_error_ = cb_error; });
    ASSERT_THAT(complete_status, ::testing::Optional(PW_STATUS_OK));
    ASSERT_FALSE(controller_error_.has_value());
  }

  FidlController* controller() { return &fidl_controller_.value(); }

  FakeHciServer* server() { return &fake_hci_server_.value(); }

  std::optional<pw::Status> controller_error() const { return controller_error_; }

 private:
  std::optional<pw::Status> controller_error_;
  std::optional<FakeHciServer> fake_hci_server_;
  std::optional<FidlController> fidl_controller_;
};

TEST_F(FidlControllerTest, SendAndReceiveAclPackets) {
  RETURN_IF_FATAL(InitializeController());

  const StaticByteBuffer acl_packet_0(0x00, 0x01, 0x02, 0x03);
  controller()->SendAclData(acl_packet_0.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(server()->acl_packets_received().size(), 1u);
  EXPECT_THAT(server()->acl_packets_received()[0], BufferEq(acl_packet_0));

  const StaticByteBuffer acl_packet_1(0x04, 0x05, 0x06, 0x07);
  controller()->SendAclData(acl_packet_1.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(server()->acl_packets_received().size(), 2u);
  EXPECT_THAT(server()->acl_packets_received()[1], BufferEq(acl_packet_1));

  std::vector<DynamicByteBuffer> received_acl;
  controller()->SetReceiveAclFunction([&](pw::span<const std::byte> buffer) {
    received_acl.emplace_back(BufferView(buffer.data(), buffer.size()));
  });

  server()->SendAcl(acl_packet_0.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_acl.size(), 1u);
  EXPECT_THAT(received_acl[0], BufferEq(acl_packet_0));

  server()->SendAcl(acl_packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(received_acl.size(), 2u);
  EXPECT_THAT(received_acl[1], BufferEq(acl_packet_1));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
}

TEST_F(FidlControllerTest, SendCommandsAndReceiveEvents) {
  RETURN_IF_FATAL(InitializeController());

  const StaticByteBuffer packet_0(0x00, 0x01, 0x02, 0x03);
  controller()->SendCommand(packet_0.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(server()->commands_received().size(), 1u);
  EXPECT_THAT(server()->commands_received()[0], BufferEq(packet_0));

  const StaticByteBuffer packet_1(0x04, 0x05, 0x06, 0x07);
  controller()->SendCommand(packet_1.subspan());
  RunLoopUntilIdle();
  ASSERT_EQ(server()->commands_received().size(), 2u);
  EXPECT_THAT(server()->commands_received()[1], BufferEq(packet_1));

  std::vector<DynamicByteBuffer> events;
  controller()->SetEventFunction([&](pw::span<const std::byte> buffer) {
    events.emplace_back(BufferView(buffer.data(), buffer.size()));
  });

  server()->SendEvent(packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_THAT(events[0], BufferEq(packet_1));

  server()->SendEvent(packet_1.view());
  RunLoopUntilIdle();
  ASSERT_EQ(events.size(), 2u);
  EXPECT_THAT(events[1], BufferEq(packet_1));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
}

TEST_F(FidlControllerTest, CloseClosesChannels) {
  RETURN_IF_FATAL(InitializeController());
  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(close_status.has_value());
  EXPECT_EQ(close_status.value(), PW_STATUS_OK);
  EXPECT_FALSE(server()->acl_channel_valid());
  EXPECT_FALSE(server()->command_channel_valid());
}

TEST_F(FidlControllerTest, ServerClosesChannel) {
  RETURN_IF_FATAL(InitializeController());
  RunLoopUntilIdle();

  EXPECT_TRUE(server()->CloseAclChannel());
  RunLoopUntilIdle();
  ASSERT_THAT(controller_error(), ::testing::Optional(pw::Status::Unavailable()));

  std::optional<pw::Status> close_status;
  controller()->Close([&](pw::Status status) { close_status = status; });
  ASSERT_THAT(close_status, ::testing::Optional(PW_STATUS_OK));
}

TEST_F(FidlControllerTest, ServerClosesProtocolBeforeInitialize) {
  server()->Unbind();
  RunLoopUntilIdle();
  RETURN_IF_FATAL(InitializeController());
  RunLoopUntilIdle();
  ASSERT_THAT(controller_error(), ::testing::Optional(pw::Status::Unavailable()));
}

TEST_F(FidlControllerTest, ServerClosesProtocol) {
  RETURN_IF_FATAL(InitializeController());
  RunLoopUntilIdle();
  server()->Unbind();
  RunLoopUntilIdle();
  ASSERT_THAT(controller_error(), ::testing::Optional(pw::Status::Unavailable()));
}

}  // namespace bt::controllers
