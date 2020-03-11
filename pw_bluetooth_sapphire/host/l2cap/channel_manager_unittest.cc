// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel_manager.h"

#include <memory>
#include <type_traits>

#include <fbl/macros.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"

namespace bt {
namespace l2cap {
namespace {

auto MakeExtendedFeaturesInformationRequest(CommandId id, hci::ConnectionHandle handle) {
  return CreateStaticByteBuffer(
      // ACL data header (handle, length: 10)
      LowerBits(handle), UpperBits(handle), 0x0a, 0x00,

      // L2CAP B-frame header (length: 6, chanel-id: 0x0001 (ACL sig))
      0x06, 0x00, 0x01, 0x00,

      // Extended Features Information Request
      // (ID, length: 2, type)
      0x0a, id, 0x02, 0x00,
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));
}

using TestingBase = ::gtest::TestLoopFixture;

constexpr hci::ConnectionHandle kTestHandle1 = 0x0001;
constexpr hci::ConnectionHandle kTestHandle2 = 0x0002;
constexpr PSM kTestPsm = 0x0001;
constexpr ChannelId kLocalId = 0x0040;
constexpr ChannelId kRemoteId = 0x9042;
constexpr CommandId kPeerConfigRequestId = 153;
constexpr hci::ACLDataChannel::PacketPriority kLowPriority =
    hci::ACLDataChannel::PacketPriority::kLow;
constexpr hci::ACLDataChannel::PacketPriority kHighPriority =
    hci::ACLDataChannel::PacketPriority::kHigh;
constexpr ChannelParameters kChannelParams;

void DoNothing() {}
void NopRxCallback(ByteBufferPtr) {}
void NopLeConnParamCallback(const hci::LEPreferredConnectionParameters&) {}
void NopSecurityCallback(hci::ConnectionHandle, sm::SecurityLevel, sm::StatusCallback) {}

// Holds expected outbound data packets including the source location where the expectation is set.
struct PacketExpectation {
  const char* file_name;
  int line_number;
  DynamicByteBuffer data;
  hci::Connection::LinkType ll_type;
  hci::ACLDataChannel::PacketPriority priority;
};

// Helpers to set an outbound packet expectation with the link type and source location
// boilerplate prefilled.
#define EXPECT_LE_PACKET_OUT(packet_buffer, priority)                                         \
  ExpectOutboundPacket(hci::Connection::LinkType::kLE, (priority), (packet_buffer), __FILE__, \
                       __LINE__)
#define EXPECT_ACL_PACKET_OUT(packet_buffer, priority)                                         \
  ExpectOutboundPacket(hci::Connection::LinkType::kACL, (priority), (packet_buffer), __FILE__, \
                       __LINE__)

// Serves as a test double for data transport to the Bluetooth controller beneath ChannelManager.
// Performs injection of inbound data and sets "strict" expectations on outbound data—i.e.
// unexpected outbound packets will cause test failures.
class L2CAP_ChannelManagerTest : public TestingBase {
 public:
  L2CAP_ChannelManagerTest() = default;
  ~L2CAP_ChannelManagerTest() override = default;

  void SetUp() override { SetUp(hci::kMaxACLPayloadSize, hci::kMaxACLPayloadSize); }

  void SetUp(size_t max_acl_payload_size, size_t max_le_payload_size) {
    TestingBase::SetUp();

    auto send_packets = fit::bind_member(this, &L2CAP_ChannelManagerTest::SendPackets);
    auto drop_queued_packets = fit::bind_member(this, &L2CAP_ChannelManagerTest::DropQueuedPackets);
    chanmgr_ = std::make_unique<ChannelManager>(max_acl_payload_size, max_le_payload_size,
                                                std::move(send_packets),
                                                std::move(drop_queued_packets), dispatcher());
    packet_rx_handler_ = chanmgr()->MakeInboundDataHandler();

    drop_queued_packets_cb_ = [](hci::ACLPacketPredicate) {};

    next_command_id_ = 1;
  }

  void TearDown() override {
    while (!expected_packets_.empty()) {
      auto& expected = expected_packets_.front();
      ADD_FAILURE_AT(expected.file_name, expected.line_number)
          << "Didn't receive expected outbound " << expected.data.size() << "-byte packet";
      expected_packets_.pop();
    }
    packet_rx_handler_ = nullptr;
    chanmgr_ = nullptr;
    TestingBase::TearDown();
  }

  // Helper functions for registering logical links with default arguments.
  void RegisterLE(hci::ConnectionHandle handle, hci::Connection::Role role,
                  LinkErrorCallback lec = DoNothing,
                  LEConnectionParameterUpdateCallback cpuc = NopLeConnParamCallback,
                  SecurityUpgradeCallback suc = NopSecurityCallback) {
    chanmgr()->RegisterLE(handle, role, std::move(cpuc), std::move(lec), std::move(suc),
                          dispatcher());
  }

  struct QueueRegisterACLRetVal {
    CommandId extended_features_id;
    CommandId fixed_channels_supported_id;
  };

  QueueRegisterACLRetVal QueueRegisterACL(hci::ConnectionHandle handle, hci::Connection::Role role,
                                          LinkErrorCallback lec = DoNothing,
                                          SecurityUpgradeCallback suc = NopSecurityCallback) {
    QueueRegisterACLRetVal cmd_ids;
    cmd_ids.extended_features_id = NextCommandId();
    cmd_ids.fixed_channels_supported_id = NextCommandId();

    EXPECT_ACL_PACKET_OUT(
        MakeExtendedFeaturesInformationRequest(cmd_ids.extended_features_id, handle),
        kHighPriority);
    EXPECT_ACL_PACKET_OUT(
        testing::AclFixedChannelsSupportedInfoReq(cmd_ids.fixed_channels_supported_id, handle),
        kHighPriority);
    RegisterACL(handle, role, std::move(lec), std::move(suc));
    return cmd_ids;
  }

  void RegisterACL(hci::ConnectionHandle handle, hci::Connection::Role role,
                   LinkErrorCallback lec = DoNothing,
                   SecurityUpgradeCallback suc = NopSecurityCallback) {
    chanmgr()->RegisterACL(handle, role, std::move(lec), std::move(suc), dispatcher());
  }

  fbl::RefPtr<Channel> ActivateNewFixedChannel(ChannelId id,
                                               hci::ConnectionHandle conn_handle = kTestHandle1,
                                               Channel::ClosedCallback closed_cb = DoNothing,
                                               Channel::RxCallback rx_cb = NopRxCallback) {
    auto chan = chanmgr()->OpenFixedChannel(conn_handle, id);
    if (!chan ||
        !chan->ActivateWithDispatcher(std::move(rx_cb), std::move(closed_cb), dispatcher())) {
      return nullptr;
    }

    return chan;
  }

  // |activated_cb| will be called with opened and activated Channel if
  // successful and nullptr otherwise.
  void ActivateOutboundChannel(PSM psm, ChannelParameters chan_params, ChannelCallback activated_cb,
                               hci::ConnectionHandle conn_handle = kTestHandle1,
                               Channel::ClosedCallback closed_cb = DoNothing,
                               Channel::RxCallback rx_cb = NopRxCallback) {
    ChannelCallback open_cb = [this, activated_cb = std::move(activated_cb),
                               rx_cb = std::move(rx_cb),
                               closed_cb = std::move(closed_cb)](auto chan) mutable {
      if (!chan ||
          !chan->ActivateWithDispatcher(std::move(rx_cb), std::move(closed_cb), dispatcher())) {
        activated_cb(nullptr);
      } else {
        activated_cb(std::move(chan));
      }
    };
    chanmgr()->OpenChannel(conn_handle, psm, chan_params, std::move(open_cb), dispatcher());
  }

  // Set an expectation for an outbound ACL data packet. Packets are expected in the order that
  // they're added. The test fails if not all expected packets have been set when the test case
  // completes or if the outbound data doesn't match expectations, including the ordering between
  // LE and ACL packets.
  void ExpectOutboundPacket(hci::Connection::LinkType ll_type,
                            hci::ACLDataChannel::PacketPriority priority, const ByteBuffer& data,
                            const char* file_name = "", int line_number = 0) {
    expected_packets_.push({file_name, line_number, DynamicByteBuffer(data), ll_type, priority});
  }

  // Returns true if all expected outbound packets up to this call have been sent by the test case.
  [[nodiscard]] bool AllExpectedPacketsSent() const { return expected_packets_.empty(); }

  void ReceiveAclDataPacket(const ByteBuffer& packet) {
    const size_t payload_size = packet.size() - sizeof(hci::ACLDataHeader);
    ZX_ASSERT(payload_size <= std::numeric_limits<uint16_t>::max());
    hci::ACLDataPacketPtr acl_packet = hci::ACLDataPacket::New(static_cast<uint16_t>(payload_size));
    auto mutable_acl_packet_data = acl_packet->mutable_view()->mutable_data();
    packet.Copy(&mutable_acl_packet_data);
    packet_rx_handler_(std::move(acl_packet));
  }

  ChannelManager* chanmgr() const { return chanmgr_.get(); }

  void set_drop_queued_packets_cb(ChannelManager::DropQueuedAclCallback cb) {
    drop_queued_packets_cb_ = std::move(cb);
  }

  CommandId NextCommandId() { return next_command_id_++; }

 private:
  bool SendPackets(LinkedList<hci::ACLDataPacket> packets, ChannelId channel_id,
                   hci::ACLDataChannel::PacketPriority priority) {
    for (const auto& packet : packets) {
      const ByteBuffer& data = packet.view().data();
      if (expected_packets_.empty()) {
        ADD_FAILURE() << "Unexpected outbound ACL data";
        std::cout << "{ ";
        PrintByteContainer(data);
        std::cout << " }\n";
      } else {
        const auto& expected = expected_packets_.front();
        // Prints both data in case of mismatch.
        if (!ContainersEqual(expected.data, data)) {
          ADD_FAILURE_AT(expected.file_name, expected.line_number)
              << "Outbound ACL data doesn't match expected";
        }

        if (expected.priority != priority) {
          std::cout << "Expected: "
                    << static_cast<std::underlying_type_t<hci::ACLDataChannel::PacketPriority>>(
                           expected.priority)
                    << std::endl;
          std::cout << "Found: "
                    << static_cast<std::underlying_type_t<hci::ACLDataChannel::PacketPriority>>(
                           priority)
                    << std::endl;
          ADD_FAILURE_AT(expected.file_name, expected.line_number)
              << "Outbound ACL priority doesn't match expected";
        }

        expected_packets_.pop();
      }
    }
    return !packets.is_empty();
  }

  void DropQueuedPackets(hci::ACLPacketPredicate filter) {
    drop_queued_packets_cb_(std::move(filter));
  }

  std::unique_ptr<ChannelManager> chanmgr_;
  hci::ACLPacketHandler packet_rx_handler_;
  ChannelManager::DropQueuedAclCallback drop_queued_packets_cb_;

  std::queue<const PacketExpectation> expected_packets_;

  CommandId next_command_id_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(L2CAP_ChannelManagerTest);
};

TEST_F(L2CAP_ChannelManagerTest, OpenFixedChannelErrorNoConn) {
  // This should fail as the ChannelManager has no entry for |kTestHandle1|.
  EXPECT_EQ(nullptr, ActivateNewFixedChannel(kATTChannelId));

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  // This should fail as the ChannelManager has no entry for |kTestHandle2|.
  EXPECT_EQ(nullptr, ActivateNewFixedChannel(kATTChannelId, kTestHandle2));
}

TEST_F(L2CAP_ChannelManagerTest, OpenFixedChannelErrorDisallowedId) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  // ACL-U link
  QueueRegisterACL(kTestHandle2, hci::Connection::Role::kMaster);
  RunLoopUntilIdle();

  // This should fail as kSMPChannelId is ACL-U only.
  EXPECT_EQ(nullptr, ActivateNewFixedChannel(kSMPChannelId, kTestHandle1));

  // This should fail as kATTChannelId is LE-U only.
  EXPECT_EQ(nullptr, ActivateNewFixedChannel(kATTChannelId, kTestHandle2));
}

TEST_F(L2CAP_ChannelManagerTest, ActivateFailsAfterDeactivate) {
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);

  chan->Deactivate();

  // Activate should fail.
  EXPECT_FALSE(chan->ActivateWithDispatcher(NopRxCallback, DoNothing, dispatcher()));
}

TEST_F(L2CAP_ChannelManagerTest, OpenFixedChannelAndUnregisterLink) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };

  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, closed_cb);
  ASSERT_TRUE(chan);
  EXPECT_EQ(kTestHandle1, chan->link_handle());

  // This should notify the channel.
  chanmgr()->Unregister(kTestHandle1);

  RunLoopUntilIdle();

  // |closed_cb| will be called synchronously since it was registered using the
  // current thread's task runner.
  EXPECT_TRUE(closed_called);
}

TEST_F(L2CAP_ChannelManagerTest, OpenFixedChannelAndCloseChannel) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };

  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, closed_cb);
  ASSERT_TRUE(chan);

  // Close the channel before unregistering the link. |closed_cb| should not get
  // called.
  chan->Deactivate();
  chanmgr()->Unregister(kTestHandle1);

  RunLoopUntilIdle();

  EXPECT_FALSE(closed_called);
}

TEST_F(L2CAP_ChannelManagerTest, FixedChannelsUseBasicMode) {
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);
  EXPECT_EQ(ChannelMode::kBasic, chan->mode());
}

TEST_F(L2CAP_ChannelManagerTest, OpenAndCloseWithLinkMultipleFixedChannels) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  bool att_closed = false;
  auto att_closed_cb = [&att_closed] { att_closed = true; };

  bool smp_closed = false;
  auto smp_closed_cb = [&smp_closed] { smp_closed = true; };

  auto att_chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, att_closed_cb);
  ASSERT_TRUE(att_chan);

  auto smp_chan = ActivateNewFixedChannel(kLESMPChannelId, kTestHandle1, smp_closed_cb);
  ASSERT_TRUE(smp_chan);

  smp_chan->Deactivate();
  chanmgr()->Unregister(kTestHandle1);

  RunLoopUntilIdle();

  EXPECT_TRUE(att_closed);
  EXPECT_FALSE(smp_closed);
}

TEST_F(L2CAP_ChannelManagerTest, SendingPacketDuringCleanUpHasNoEffect) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, closed_cb);
  ASSERT_TRUE(chan);

  // Send a packet. This should be posted on the L2CAP dispatcher but not processed yet.
  EXPECT_TRUE(chan->Send(NewBuffer('h', 'i')));

  chanmgr()->Unregister(kTestHandle1);

  // Once the loop is drained the L2CAP channel should have been notified of
  // closure but the package should not get sent.
  RunLoopUntilIdle();
  EXPECT_TRUE(closed_called);

  // No outbound packet expectations were set, so this test will fail if it sends any data.
}

// Tests that destroying the ChannelManager cleanly shuts down all channels.
TEST_F(L2CAP_ChannelManagerTest, DestroyingChannelManagerCleansUpChannels) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, closed_cb);
  ASSERT_TRUE(chan);

  // Send a packet. This should be posted on the L2CAP dispatcher but not
  // processed yet.
  EXPECT_TRUE(chan->Send(NewBuffer('h', 'i')));

  TearDown();

  // Once the loop is drained the L2CAP channel should have been notified of
  // closure but the package should not get sent.
  RunLoopUntilIdle();
  EXPECT_TRUE(closed_called);

  // No outbound packet expectations were set, so this test will fail if it sends any data.
}

TEST_F(L2CAP_ChannelManagerTest, DeactivateDoesNotCrashOrHang) {
  // Tests that the clean up task posted to the LogicalLink does not crash when
  // a dynamic registry is not present (which is the case for LE links).
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);

  chan->Deactivate();

  // Loop until the clean up task runs.
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, CallingDeactivateFromClosedCallbackDoesNotCrashOrHang) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  RunLoopUntilIdle();

  auto chan = chanmgr()->OpenFixedChannel(kTestHandle1, kSMPChannelId);
  chan->ActivateWithDispatcher(
      NopRxCallback, [chan] { chan->Deactivate(); }, dispatcher());
  chanmgr()->Unregister(kTestHandle1);  // Triggers ClosedCallback.
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, ReceiveData) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  std::vector<std::string> sdus;
  auto att_rx_cb = [&sdus](ByteBufferPtr sdu) {
    ZX_DEBUG_ASSERT(sdu);
    sdus.push_back(sdu->ToString());
  };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    ZX_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  auto att_chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1, DoNothing, att_rx_cb);
  auto smp_chan = ActivateNewFixedChannel(kLESMPChannelId, kTestHandle1, DoNothing, smp_rx_cb);
  ASSERT_TRUE(att_chan);
  ASSERT_TRUE(smp_chan);

  // ATT channel
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame
      0x05, 0x00, 0x04, 0x00, 'h', 'e', 'l', 'l', 'o'));
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame (partial)
      0x0C, 0x00, 0x04, 0x00, 'h', 'o', 'w', ' ', 'a'));
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (continuing fragment)
      0x01, 0x10, 0x07, 0x00,

      // L2CAP B-frame (partial)
      'r', 'e', ' ', 'y', 'o', 'u', '?'));

  // SMP channel
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, 0x06, 0x00));

  RunLoopUntilIdle();

  EXPECT_TRUE(smp_cb_called);
  ASSERT_EQ(2u, sdus.size());
  EXPECT_EQ("hello", sdus[0]);
  EXPECT_EQ("how are you?", sdus[1]);
}

TEST_F(L2CAP_ChannelManagerTest, ReceiveDataBeforeRegisteringLink) {
  constexpr size_t kPacketCount = 10;

  StaticByteBuffer<255> buffer;

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  size_t packet_count = 0;
  auto att_rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    ZX_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  // ATT channel
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(CreateStaticByteBuffer(
        // ACL data header (starting fragment)
        0x01, 0x00, 0x04, 0x00,

        // L2CAP B-frame
        0x00, 0x00, 0x04, 0x00));
  }

  // SMP channel
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, 0x06, 0x00));

  fbl::RefPtr<Channel> att_chan, smp_chan;

  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  att_chan = ActivateNewFixedChannel(
      kATTChannelId, kTestHandle1, [] {}, att_rx_cb);
  ZX_DEBUG_ASSERT(att_chan);

  smp_chan = ActivateNewFixedChannel(
      kLESMPChannelId, kTestHandle1, [] {}, smp_rx_cb);
  ZX_DEBUG_ASSERT(smp_chan);

  RunLoopUntilIdle();
  EXPECT_TRUE(smp_cb_called);
  EXPECT_EQ(kPacketCount, packet_count);
}

// Receive data after registering the link but before creating the channel.
TEST_F(L2CAP_ChannelManagerTest, ReceiveDataBeforeCreatingChannel) {
  constexpr size_t kPacketCount = 10;

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  StaticByteBuffer<255> buffer;

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  size_t packet_count = 0;
  auto att_rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    ZX_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  // ATT channel
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(CreateStaticByteBuffer(
        // ACL data header (starting fragment)
        0x01, 0x00, 0x04, 0x00,

        // L2CAP B-frame
        0x00, 0x00, 0x04, 0x00));
  }

  // SMP channel
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, 0x06, 0x00));

  fbl::RefPtr<Channel> att_chan, smp_chan;

  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  att_chan = ActivateNewFixedChannel(
      kATTChannelId, kTestHandle1, [] {}, att_rx_cb);
  ZX_DEBUG_ASSERT(att_chan);

  smp_chan = ActivateNewFixedChannel(
      kLESMPChannelId, kTestHandle1, [] {}, smp_rx_cb);
  ZX_DEBUG_ASSERT(smp_chan);

  RunLoopUntilIdle();

  EXPECT_TRUE(smp_cb_called);
  EXPECT_EQ(kPacketCount, packet_count);
}

// Receive data after registering the link and creating the channel but before
// setting the rx handler.
TEST_F(L2CAP_ChannelManagerTest, ReceiveDataBeforeSettingRxHandler) {
  constexpr size_t kPacketCount = 10;

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto att_chan = chanmgr()->OpenFixedChannel(kTestHandle1, kATTChannelId);
  ZX_DEBUG_ASSERT(att_chan);

  auto smp_chan = chanmgr()->OpenFixedChannel(kTestHandle1, kLESMPChannelId);
  ZX_DEBUG_ASSERT(smp_chan);

  StaticByteBuffer<255> buffer;

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  size_t packet_count = 0;
  auto att_rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    ZX_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  // ATT channel
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(CreateStaticByteBuffer(
        // ACL data header (starting fragment)
        0x01, 0x00, 0x04, 0x00,

        // L2CAP B-frame
        0x00, 0x00, 0x04, 0x00));
  }

  // SMP channel
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, 0x06, 0x00));

  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  att_chan->ActivateWithDispatcher(att_rx_cb, DoNothing, dispatcher());
  smp_chan->ActivateWithDispatcher(smp_rx_cb, DoNothing, dispatcher());

  RunLoopUntilIdle();

  EXPECT_TRUE(smp_cb_called);
  EXPECT_EQ(kPacketCount, packet_count);
}

TEST_F(L2CAP_ChannelManagerTest, ActivateChannelOnDataDomainProcessesCallbacksSynchronously) {
  // LE-U link
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  int att_rx_cb_count = 0;
  int smp_rx_cb_count = 0;

  auto att_chan = chanmgr()->OpenFixedChannel(kTestHandle1, kATTChannelId);
  ASSERT_TRUE(att_chan);
  auto att_rx_cb = [&att_rx_cb_count](ByteBufferPtr sdu) {
    EXPECT_EQ("hello", sdu->AsString());
    att_rx_cb_count++;
  };
  bool att_closed_called = false;
  auto att_closed_cb = [&att_closed_called] { att_closed_called = true; };

  // Activate ATT to run on Data domain, requiring synchronous callback invocation.
  ASSERT_TRUE(att_chan->ActivateOnDataDomain(std::move(att_rx_cb), std::move(att_closed_cb)));

  auto smp_rx_cb = [&smp_rx_cb_count](ByteBufferPtr sdu) {
    EXPECT_EQ(u8"🤨", sdu->AsString());
    smp_rx_cb_count++;
  };
  bool smp_closed_called = false;
  auto smp_closed_cb = [&smp_closed_called] { smp_closed_called = true; };

  // The SMP channel is activated with the test loop dispatcher.
  auto smp_chan = ActivateNewFixedChannel(kLESMPChannelId, kTestHandle1, std::move(smp_closed_cb),
                                          std::move(smp_rx_cb));
  ASSERT_TRUE(smp_chan);

  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame for SMP fixed channel (4-byte payload: U+1F928 in UTF-8)
      0x04, 0x00, 0x06, 0x00, 0xf0, 0x9f, 0xa4, 0xa8));

  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame for ATT fixed channel
      0x05, 0x00, 0x04, 0x00, 'h', 'e', 'l', 'l', 'o'));

  // Receiving data in ChannelManager processes the ATT packet synchronously so it has already
  // routed the data to the Channel.
  EXPECT_EQ(att_rx_cb_count, 1);

  // But the SMP channel won't get anything until we yield to the event loop.
  EXPECT_EQ(smp_rx_cb_count, 0);

  RunLoopUntilIdle();

  EXPECT_EQ(1, att_rx_cb_count);
  EXPECT_EQ(1, smp_rx_cb_count);

  // Link closure synchronously calls the ATT channel close callback.
  chanmgr()->Unregister(kTestHandle1);
  EXPECT_TRUE(att_closed_called);
  EXPECT_FALSE(smp_closed_called);
}

TEST_F(L2CAP_ChannelManagerTest, SendOnClosedLink) {
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto att_chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ZX_DEBUG_ASSERT(att_chan);

  chanmgr()->Unregister(kTestHandle1);

  EXPECT_FALSE(att_chan->Send(NewBuffer('T', 'e', 's', 't')));
}

TEST_F(L2CAP_ChannelManagerTest, SendBasicSdu) {
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto att_chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ZX_DEBUG_ASSERT(att_chan);

  EXPECT_LE_PACKET_OUT(CreateStaticByteBuffer(
                           // ACL data header (handle: 1, length 7)
                           0x01, 0x00, 0x08, 0x00,

                           // L2CAP B-frame: (length: 3, channel-id: 4)
                           0x04, 0x00, 0x04, 0x00, 'T', 'e', 's', 't'),
                       kLowPriority);

  EXPECT_TRUE(att_chan->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
}

// Tests that fragmentation of LE and BR/EDR packets use the corresponding buffer size.
TEST_F(L2CAP_ChannelManagerTest, SendFragmentedSdus) {
  constexpr size_t kMaxACLDataSize = 6;
  constexpr size_t kMaxLEDataSize = 5;

  TearDown();
  SetUp(kMaxACLDataSize, kMaxLEDataSize);

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);

  // Send fragmented Extended Features Information Request
  EXPECT_ACL_PACKET_OUT(CreateStaticByteBuffer(
                            // ACL data header (handle: 2, length: 6)
                            0x02, 0x00, 0x06, 0x00,

                            // L2CAP B-frame (length: 6, channel-id: 1)
                            0x06, 0x00, 0x01, 0x00,

                            // Extended Features Information Request
                            // (code = 0x0A, ID)
                            0x0A, NextCommandId()),
                        kHighPriority);
  EXPECT_ACL_PACKET_OUT(
      CreateStaticByteBuffer(
          // ACL data header (handle: 2, pbf: continuing fr., length: 4)
          0x02, 0x10, 0x04, 0x00,

          // Extended Features Information Request cont.
          // (Length: 2, type)
          0x02, 0x00, LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
          UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported))),
      kHighPriority);

  // Send fragmented Fixed Channels Supported Information Request
  EXPECT_ACL_PACKET_OUT(StaticByteBuffer(
                            // ACL data header (handle: 2, length: 6)
                            0x02, 0x00, 0x06, 0x00,

                            // L2CAP B-frame (length: 6, channel-id: 1)
                            0x06, 0x00, 0x01, 0x00,

                            // Fixed Channels Supported Information Request
                            // (command code, command ID)
                            l2cap::kInformationRequest, NextCommandId()),
                        kHighPriority);
  EXPECT_ACL_PACKET_OUT(
      StaticByteBuffer(
          // ACL data header (handle: 2, pbf: continuing fr., length: 4)
          0x02, 0x10, 0x04, 0x00,

          // Fixed Channels Supported Information Request cont.
          // (length: 2, type)
          0x02, 0x00, LowerBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported)),
          UpperBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported))),
      kHighPriority);
  RegisterACL(kTestHandle2, hci::Connection::Role::kMaster);

  // We use the ATT fixed-channel for LE and the SM fixed-channel for ACL.
  auto att_chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  auto sm_chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle2);
  ASSERT_TRUE(att_chan);
  ASSERT_TRUE(sm_chan);

  EXPECT_LE_PACKET_OUT(CreateStaticByteBuffer(
                           // ACL data header (handle: 1, length: 5)
                           0x01, 0x00, 0x05, 0x00,

                           // L2CAP B-frame: (length: 5, channel-id: 4, partial payload)
                           0x05, 0x00, 0x04, 0x00, 'H'),
                       kLowPriority);

  EXPECT_LE_PACKET_OUT(CreateStaticByteBuffer(
                           // ACL data header (handle: 1, pbf: continuing fr., length: 4)
                           0x01, 0x10, 0x04, 0x00,

                           // Continuing payload
                           'e', 'l', 'l', 'o'),
                       kLowPriority);

  EXPECT_ACL_PACKET_OUT(CreateStaticByteBuffer(
                            // ACL data header (handle: 2, length: 6)
                            0x02, 0x00, 0x06, 0x00,

                            // l2cap b-frame: (length: 7, channel-id: 7, partial payload)
                            0x07, 0x00, 0x07, 0x00, 'G', 'o'),
                        kHighPriority);

  EXPECT_ACL_PACKET_OUT(CreateStaticByteBuffer(
                            // ACL data header (handle: 2, pbf: continuing fr., length: 5)
                            0x02, 0x10, 0x05, 0x00,

                            // continuing payload
                            'o', 'd', 'b', 'y', 'e'),
                        kHighPriority);

  // SDU of length 5 corresponds to a 9-octet B-frame which should be sent over a 5-byte and a 4-
  // byte fragment.
  EXPECT_TRUE(att_chan->Send(NewBuffer('H', 'e', 'l', 'l', 'o')));

  // SDU of length 7 corresponds to a 11-octet B-frame. Due to the BR/EDR buffer size, this should
  // be sent over a 6-byte then a 5-byte fragment.
  EXPECT_TRUE(sm_chan->Send(NewBuffer('G', 'o', 'o', 'd', 'b', 'y', 'e')));

  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, LEChannelSignalLinkError) {
  bool link_error = false;
  auto link_error_cb = [&link_error] { link_error = true; };
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster, link_error_cb);

  // Activate a new Attribute channel to signal the error.
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  chan->SignalLinkError();

  // The event will run asynchronously.
  EXPECT_FALSE(link_error);

  RunLoopUntilIdle();
  EXPECT_TRUE(link_error);
}

TEST_F(L2CAP_ChannelManagerTest, ACLChannelSignalLinkError) {
  bool link_error = false;
  auto link_error_cb = [&link_error] { link_error = true; };
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster, link_error_cb);

  // Activate a new Security Manager channel to signal the error.
  auto chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle1);
  chan->SignalLinkError();

  // The event will run asynchronously.
  EXPECT_FALSE(link_error);

  RunLoopUntilIdle();
  EXPECT_TRUE(link_error);
}

TEST_F(L2CAP_ChannelManagerTest, LEConnectionParameterUpdateRequest) {
  bool conn_param_cb_called = false;
  auto conn_param_cb = [&conn_param_cb_called](const auto& params) {
    // The parameters should match the payload of the HCI packet seen below.
    EXPECT_EQ(0x0006, params.min_interval());
    EXPECT_EQ(0x0C80, params.max_interval());
    EXPECT_EQ(0x01F3, params.max_latency());
    EXPECT_EQ(0x0C80, params.supervision_timeout());
    conn_param_cb_called = true;
  };

  EXPECT_ACL_PACKET_OUT(CreateStaticByteBuffer(
                            // ACL data header (handle: 0x0001, length: 10 bytes)
                            0x01, 0x00, 0x0a, 0x00,

                            // L2CAP B-frame header (length: 6 bytes, channel-id: 0x0005 (LE sig))
                            0x06, 0x00, 0x05, 0x00,

                            // L2CAP C-frame header
                            // (LE conn. param. update response, id: 1, length: 2 bytes)
                            0x13, 0x01, 0x02, 0x00,

                            // result: accepted
                            0x00, 0x00),
                        kHighPriority);

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster, DoNothing, conn_param_cb);

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 16 bytes)
      0x01, 0x00, 0x10, 0x00,

      // L2CAP B-frame header (length: 12 bytes, channel-id: 0x0005 (LE sig))
      0x0C, 0x00, 0x05, 0x00,

      // L2CAP C-frame header
      // (LE conn. param. update request, id: 1, length: 8 bytes)
      0x12, 0x01, 0x08, 0x00,

      // Connection parameters (hardcoded to match the expections in
      // |conn_param_cb|).
      0x06, 0x00,
      0x80, 0x0C,
      0xF3, 0x01,
      0x80, 0x0C));
  // clang-format on

  RunLoopUntilIdle();
  EXPECT_TRUE(conn_param_cb_called);
}

auto ConfigurationRequest(CommandId id, ChannelId dst_id, uint16_t mtu = kDefaultMTU,
                          std::optional<ChannelMode> mode = std::nullopt) {
  if (mode.has_value()) {
    return DynamicByteBuffer(StaticByteBuffer(
        // ACL data header (handle: 0x0001, length: 27 bytes)
        0x01, 0x00, 0x1b, 0x00,

        // L2CAP B-frame header (length: 23 bytes, channel-id: 0x0001 (ACL sig))
        0x17, 0x00, 0x01, 0x00,

        // Configuration Request (ID, length: 19, dst cid, flags: 0)
        0x04, id, 0x13, 0x00, LowerBits(dst_id), UpperBits(dst_id), 0x00, 0x00,

        // Mtu option (ID, Length, MTU)
        0x01, 0x02, LowerBits(mtu), UpperBits(mtu),

        // Retransmission & Flow Control option (type, length: 9, mode, tx_window: 63,
        // max_retransmit: 0, retransmit timeout: 0 ms, monitor timeout: 0 ms, mps: 65535)
        0x04, 0x09, static_cast<uint8_t>(*mode), kErtmMaxUnackedInboundFrames,
        kErtmMaxInboundRetransmissions, 0x00, 0x00, 0x00, 0x00,
        LowerBits(kMaxInboundPduPayloadSize), UpperBits(kMaxInboundPduPayloadSize)));
  }
  return DynamicByteBuffer(StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 16 bytes)
      0x01, 0x00, 0x10, 0x00,

      // L2CAP B-frame header (length: 12 bytes, channel-id: 0x0001 (ACL sig))
      0x0c, 0x00, 0x01, 0x00,

      // Configuration Request (ID, length: 8, dst cid, flags: 0)
      0x04, id, 0x08, 0x00, LowerBits(dst_id), UpperBits(dst_id), 0x00, 0x00,

      // Mtu option (ID, Length, MTU)
      0x01, 0x02, LowerBits(mtu), UpperBits(mtu)));
}

auto OutboundConnectionResponse(CommandId id) {
  return testing::AclConnectionRsp(id, kTestHandle1, kRemoteId, kLocalId);
}

auto InboundConnectionResponse(CommandId id) {
  return testing::AclConnectionRsp(id, kTestHandle1, kLocalId, kRemoteId);
}

auto InboundConfigurationRequest(CommandId id, uint16_t mtu = kDefaultMTU,
                                 std::optional<ChannelMode> mode = std::nullopt) {
  return ConfigurationRequest(id, kLocalId, mtu, mode);
}

auto InboundConfigurationResponse(CommandId id) {
  return CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 14 bytes)
      0x01, 0x00, 0x0e, 0x00,

      // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
      0x0a, 0x00, 0x01, 0x00,

      // Configuration Response (ID: 2, length: 6, src cid, flags: 0,
      // result: success)
      0x05, id, 0x06, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 0x00, 0x00, 0x00, 0x00);
}

auto InboundConnectionRequest(CommandId id) {
  return CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Connection Request (ID, length: 4, psm, src cid)
      0x02, id, 0x04, 0x00, LowerBits(kTestPsm), UpperBits(kTestPsm), LowerBits(kRemoteId),
      UpperBits(kRemoteId));
}

auto OutboundConnectionRequest(CommandId id) {
  return CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Connection Request (ID, length: 4, psm, src cid)
      0x02, id, 0x04, 0x00, LowerBits(kTestPsm), UpperBits(kTestPsm), LowerBits(kLocalId),
      UpperBits(kLocalId));
}

auto OutboundConfigurationRequest(CommandId id, uint16_t mtu = kMaxMTU,
                                  std::optional<ChannelMode> mode = std::nullopt) {
  return ConfigurationRequest(id, kRemoteId, mtu, mode);
}

auto OutboundConfigurationResponse(CommandId id, uint16_t mtu = kDefaultMTU,
                                   std::optional<ChannelMode> mode = std::nullopt) {
  const uint8_t kConfigLength = 10 + (mode.has_value() ? 11 : 0);
  const uint16_t kL2capLength = kConfigLength + 4;
  const uint16_t kAclLength = kL2capLength + 4;
  const uint16_t kErtmReceiverReadyPollTimerMsecs = kErtmReceiverReadyPollTimerDuration.to_msecs();
  const uint16_t kErtmMonitorTimerMsecs = kErtmMonitorTimerDuration.to_msecs();

  if (mode.has_value()) {
    return DynamicByteBuffer(StaticByteBuffer(
        // ACL data header (handle: 0x0001, length: 14 bytes)
        0x01, 0x00, LowerBits(kAclLength), UpperBits(kAclLength),

        // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
        LowerBits(kL2capLength), UpperBits(kL2capLength), 0x01, 0x00,

        // Configuration Response (ID, length, src cid, flags: 0, result: success)
        0x05, id, kConfigLength, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 0x00, 0x00, 0x00,
        0x00,

        // MTU option (ID, Length, MTU)
        0x01, 0x02, LowerBits(mtu), UpperBits(mtu),

        // Retransmission & Flow Control option (type, length: 9, mode, TxWindow, MaxTransmit, rtx
        // timeout: 2 secs, monitor timeout: 12 secs, mps)
        0x04, 0x09, static_cast<uint8_t>(*mode), kErtmMaxUnackedInboundFrames,
        kErtmMaxInboundRetransmissions, LowerBits(kErtmReceiverReadyPollTimerMsecs),
        UpperBits(kErtmReceiverReadyPollTimerMsecs), LowerBits(kErtmMonitorTimerMsecs),
        UpperBits(kErtmMonitorTimerMsecs), LowerBits(kMaxInboundPduPayloadSize),
        UpperBits(kMaxInboundPduPayloadSize)));
  } else {
    return DynamicByteBuffer(StaticByteBuffer(
        // ACL data header (handle: 0x0001, length: 14 bytes)
        0x01, 0x00, LowerBits(kAclLength), UpperBits(kAclLength),

        // L2CAP B-frame header (length, channel-id: 0x0001 (ACL sig))
        LowerBits(kL2capLength), UpperBits(kL2capLength), 0x01, 0x00,

        // Configuration Response (ID, length, src cid, flags: 0, result: success)
        0x05, id, kConfigLength, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 0x00, 0x00, 0x00,
        0x00,

        // MTU option (ID, Length, MTU)
        0x01, 0x02, LowerBits(mtu), UpperBits(mtu)));
  }
}

auto OutboundDisconnectionRequest(CommandId id) {
  return CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Request
      // (ID, length: 4, dst cid, src cid)
      0x06, id, 0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), LowerBits(kLocalId),
      UpperBits(kLocalId));
}

auto OutboundDisconnectionResponse(CommandId id) {
  return CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Response
      // (ID, length: 4, dst cid, src cid)
      0x07, id, 0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), LowerBits(kRemoteId),
      UpperBits(kRemoteId));
}

TEST_F(L2CAP_ChannelManagerTest, ACLOutboundDynamicChannelLocalDisconnect) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  RunLoopUntilIdle();

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<l2cap::Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb));
  RunLoopUntilIdle();

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  ASSERT_TRUE(channel);
  EXPECT_FALSE(closed_cb_called);
  EXPECT_EQ(kLocalId, channel->id());
  EXPECT_EQ(kRemoteId, channel->remote_id());
  EXPECT_EQ(ChannelMode::kBasic, channel->mode());

  // Test SDU transmission.
  // SDU must have remote channel ID (unlike for fixed channels).
  EXPECT_ACL_PACKET_OUT(
      CreateStaticByteBuffer(
          // ACL data header (handle: 1, length 8)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 4, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());

  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  // Packets for testing filter against
  constexpr hci::ConnectionHandle kTestHandle2 = 0x02;
  constexpr ChannelId kWrongChannelId = 0x02;
  auto dummy_packet1 =
      hci::ACLDataPacket::New(kTestHandle1, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 0x00);
  auto dummy_packet2 =
      hci::ACLDataPacket::New(kTestHandle2, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 0x00);
  size_t filter_cb_count = 0;
  auto filter_cb = [&](hci::ACLPacketPredicate filter) {
    // filter out correct closed channel on correct connection handle
    EXPECT_TRUE(filter(dummy_packet1, kLocalId));
    // do not filter out other channels
    EXPECT_FALSE(filter(dummy_packet1, kWrongChannelId));
    // do not filter out other connections
    EXPECT_FALSE(filter(dummy_packet2, kLocalId));
    filter_cb_count++;
  };
  set_drop_queued_packets_cb(std::move(filter_cb));

  // Explicit deactivation should not result in |closed_cb| being called.
  channel->Deactivate();

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_EQ(1u, filter_cb_count);

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Response
      // (ID, length: 4, dst cid, src cid)
      0x07, disconn_req_id, 0x04, 0x00,
      LowerBits(kRemoteId), UpperBits(kRemoteId), LowerBits(kLocalId), UpperBits(kLocalId)));
  // clang-format on

  RunLoopUntilIdle();

  EXPECT_FALSE(closed_cb_called);
}

TEST_F(L2CAP_ChannelManagerTest, ACLOutboundDynamicChannelRemoteDisconnect) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<l2cap::Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  bool channel_closed = false;
  auto closed_cb = [&channel_closed] { channel_closed = true; };

  bool sdu_received = false;
  auto data_rx_cb = [&sdu_received](ByteBufferPtr sdu) {
    sdu_received = true;
    ZX_DEBUG_ASSERT(sdu);
    EXPECT_EQ("Test", sdu->AsString());
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb), std::move(data_rx_cb));

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);
  EXPECT_FALSE(channel_closed);

  // Test SDU reception.
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));

  RunLoopUntilIdle();
  EXPECT_TRUE(sdu_received);

  EXPECT_ACL_PACKET_OUT(OutboundDisconnectionResponse(7), kHighPriority);

  // Packets for testing filter against
  constexpr hci::ConnectionHandle kTestHandle2 = 0x02;
  constexpr ChannelId kWrongChannelId = 0x02;
  auto dummy_packet1 =
      hci::ACLDataPacket::New(kTestHandle1, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 0x00);
  auto dummy_packet2 =
      hci::ACLDataPacket::New(kTestHandle2, hci::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci::ACLBroadcastFlag::kPointToPoint, 0x00);
  size_t filter_cb_count = 0;
  auto filter_cb = [&](hci::ACLPacketPredicate filter) {
    // filter out correct closed channel
    EXPECT_TRUE(filter(dummy_packet1, kLocalId));
    // do not filter out other channels
    EXPECT_FALSE(filter(dummy_packet1, kWrongChannelId));
    // do not filter out other connections
    EXPECT_FALSE(filter(dummy_packet2, kLocalId));
    filter_cb_count++;
  };
  set_drop_queued_packets_cb(std::move(filter_cb));

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Request
      // (ID: 7, length: 4, dst cid, src cid)
      0x06, 0x07, 0x04, 0x00,
      LowerBits(kLocalId), UpperBits(kLocalId), LowerBits(kRemoteId), UpperBits(kRemoteId)));
  // clang-format on

  // The preceding peer disconnection should have immediately destroyed the route to the channel.
  // L2CAP will process it and this following SDU back-to-back. The latter should be dropped.
  sdu_received = false;
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 5)
      0x01, 0x00, 0x05, 0x00,

      // L2CAP B-frame: (length: 1, channel-id: 0x0040)
      0x01, 0x00, 0x40, 0x00, '!'));

  RunLoopUntilIdle();

  EXPECT_TRUE(channel_closed);
  EXPECT_FALSE(sdu_received);
  EXPECT_EQ(1u, filter_cb_count);
}

TEST_F(L2CAP_ChannelManagerTest, ACLOutboundDynamicChannelDataNotBuffered) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<l2cap::Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  bool channel_closed = false;
  auto closed_cb = [&channel_closed] { channel_closed = true; };

  auto data_rx_cb = [](ByteBufferPtr sdu) { FAIL() << "Unexpected data reception"; };

  // Receive SDU for the channel about to be opened. It should be ignored.
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb), std::move(data_rx_cb));
  RunLoopUntilIdle();

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));

  // The channel is connected but not configured, so no data should flow on the
  // channel. Test that this received data is also ignored.
  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));
  // clang-format on

  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_NE(nullptr, channel);
  EXPECT_FALSE(channel_closed);

  EXPECT_ACL_PACKET_OUT(OutboundDisconnectionResponse(7), kHighPriority);

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Request
      // (ID: 7, length: 4, dst cid, src cid)
      0x06, 0x07, 0x04, 0x00,
      LowerBits(kLocalId), UpperBits(kLocalId), LowerBits(kRemoteId), UpperBits(kRemoteId)));
  // clang-format on

  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, ACLOutboundDynamicChannelRemoteRefused) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  bool channel_cb_called = false;
  auto channel_cb = [&channel_cb_called](fbl::RefPtr<l2cap::Channel> channel) {
    channel_cb_called = true;
    EXPECT_FALSE(channel);
  };

  const CommandId conn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb));

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 16 bytes)
      0x01, 0x00, 0x10, 0x00,

      // L2CAP B-frame header (length: 12 bytes, channel-id: 0x0001 (ACL sig))
      0x0c, 0x00, 0x01, 0x00,

      // Connection Response (ID, length: 8, dst cid: 0x0000 (invalid),
      // src cid, result: 0x0004 (Refused; no resources available),
      // status: none)
      0x03, conn_req_id, 0x08, 0x00,
      0x00, 0x00, LowerBits(kLocalId), UpperBits(kLocalId),
      0x04, 0x00, 0x00, 0x00));
  // clang-format on

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel_cb_called);
}

TEST_F(L2CAP_ChannelManagerTest, ACLOutboundDynamicChannelFailedConfiguration) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  bool channel_cb_called = false;
  auto channel_cb = [&channel_cb_called](fbl::RefPtr<l2cap::Channel> channel) {
    channel_cb_called = true;
    EXPECT_FALSE(channel);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb));

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 14 bytes)
      0x01, 0x00, 0x0e, 0x00,

      // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
      0x0a, 0x00, 0x01, 0x00,

      // Configuration Response (ID, length: 6, src cid, flags: 0,
      // result: 0x0002 (Rejected; no reason provided))
      0x05, config_req_id, 0x06, 0x00,
      LowerBits(kLocalId), UpperBits(kLocalId), 0x00, 0x00,
      0x02, 0x00));

  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Response
      // (ID, length: 4, dst cid, src cid)
      0x07, disconn_req_id, 0x04, 0x00,
      LowerBits(kRemoteId), UpperBits(kRemoteId), LowerBits(kLocalId), UpperBits(kLocalId)));
  // clang-format on

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel_cb_called);
}

TEST_F(L2CAP_ChannelManagerTest, ACLInboundDynamicChannelLocalDisconnect) {
  constexpr PSM kBadPsm0 = 0x0004;
  constexpr PSM kBadPsm1 = 0x0103;

  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [this, &channel,
                     closed_cb = std::move(closed_cb)](fbl::RefPtr<l2cap::Channel> opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->ActivateWithDispatcher(NopRxCallback, DoNothing, dispatcher()));
  };

  EXPECT_FALSE(chanmgr()->RegisterService(kBadPsm0, ChannelParameters(), channel_cb, dispatcher()));
  EXPECT_FALSE(chanmgr()->RegisterService(kBadPsm1, ChannelParameters(), channel_cb, dispatcher()));
  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, ChannelParameters(), std::move(channel_cb),
                                         dispatcher()));

  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionResponse(1), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(1));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  ASSERT_TRUE(channel);
  EXPECT_FALSE(closed_cb_called);
  EXPECT_EQ(kLocalId, channel->id());
  EXPECT_EQ(kRemoteId, channel->remote_id());

  // Test SDU transmission.
  // SDU must have remote channel ID (unlike for fixed channels).
  EXPECT_ACL_PACKET_OUT(
      CreateStaticByteBuffer(
          // ACL data header (handle: 1, length 7)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 3, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());

  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  // Explicit deactivation should not result in |closed_cb| being called.
  channel->Deactivate();

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());

  // clang-format off
  ReceiveAclDataPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Response
      // (ID, length: 4, dst cid, src cid)
      0x07, disconn_req_id, 0x04, 0x00,
      LowerBits(kRemoteId), UpperBits(kRemoteId), LowerBits(kLocalId), UpperBits(kLocalId)));
  // clang-format on

  RunLoopUntilIdle();

  EXPECT_FALSE(closed_cb_called);
}

TEST_F(L2CAP_ChannelManagerTest, LinkSecurityProperties) {
  sm::SecurityProperties security(sm::SecurityLevel::kEncrypted, 16, false);

  // Has no effect.
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);

  // Register a link and open a channel. The security properties should be
  // accessible using the channel.
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);

  // The channel should start out at the lowest level of security.
  EXPECT_EQ(sm::SecurityProperties(), chan->security());

  // Assign a new security level.
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);

  // Channel should return the new security level.
  EXPECT_EQ(security, chan->security());
}

// Tests that assigning a new security level on a closed link does nothing.
TEST_F(L2CAP_ChannelManagerTest, AssignLinkSecurityPropertiesOnClosedLink) {
  // Register a link and open a channel. The security properties should be
  // accessible using the channel.
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster);
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);

  chanmgr()->Unregister(kTestHandle1);
  RunLoopUntilIdle();

  // Assign a new security level.
  sm::SecurityProperties security(sm::SecurityLevel::kEncrypted, 16, false);
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);

  // Channel should return the old security level.
  EXPECT_EQ(sm::SecurityProperties(), chan->security());
}

TEST_F(L2CAP_ChannelManagerTest, UpgradeSecurity) {
  // The callback passed to to Channel::UpgradeSecurity().
  sm::Status received_status;
  int security_status_count = 0;
  auto status_callback = [&](sm::Status status) {
    received_status = status;
    security_status_count++;
  };

  // The security handler callback assigned when registering a link.
  sm::Status delivered_status;
  sm::SecurityLevel last_requested_level = sm::SecurityLevel::kNoSecurity;
  int security_request_count = 0;
  auto security_handler = [&](hci::ConnectionHandle handle, sm::SecurityLevel level,
                              auto callback) {
    EXPECT_EQ(kTestHandle1, handle);
    last_requested_level = level;
    security_request_count++;

    callback(delivered_status);
  };

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster, DoNothing, NopLeConnParamCallback,
             std::move(security_handler));
  auto chan = ActivateNewFixedChannel(kATTChannelId, kTestHandle1);
  ASSERT_TRUE(chan);

  // Requesting security at or below the current level should succeed without
  // doing anything.
  chan->UpgradeSecurity(sm::SecurityLevel::kNoSecurity, status_callback, dispatcher());
  RunLoopUntilIdle();
  EXPECT_EQ(0, security_request_count);
  EXPECT_EQ(1, security_status_count);
  EXPECT_TRUE(received_status);

  // Test reporting an error.
  delivered_status = sm::Status(HostError::kNotSupported);
  chan->UpgradeSecurity(sm::SecurityLevel::kEncrypted, status_callback, dispatcher());
  RunLoopUntilIdle();
  EXPECT_EQ(1, security_request_count);
  EXPECT_EQ(2, security_status_count);
  EXPECT_EQ(delivered_status, received_status);
  EXPECT_EQ(sm::SecurityLevel::kEncrypted, last_requested_level);

  // Close the link. Future security requests should have no effect.
  chanmgr()->Unregister(kTestHandle1);
  RunLoopUntilIdle();

  chan->UpgradeSecurity(sm::SecurityLevel::kAuthenticated, status_callback, dispatcher());
  chan->UpgradeSecurity(sm::SecurityLevel::kAuthenticated, status_callback, dispatcher());
  chan->UpgradeSecurity(sm::SecurityLevel::kAuthenticated, status_callback, dispatcher());
  RunLoopUntilIdle();
  EXPECT_EQ(1, security_request_count);
  EXPECT_EQ(2, security_status_count);
}

TEST_F(L2CAP_ChannelManagerTest, SignalingChannelDataPrioritizedOverDynamicChannelData) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<l2cap::Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  // Signaling channel packets should be sent with high priority.
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);

  // Packet sent on dynamic channel should be sent with low priority.
  EXPECT_ACL_PACKET_OUT(
      CreateStaticByteBuffer(
          // ACL data header (handle: 1, length 8)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 4, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
}

#define EXPECT_HIGH_PRIORITY(channel_id)                   \
  EXPECT_EQ(ChannelManager::ChannelPriority((channel_id)), \
            hci::ACLDataChannel::PacketPriority::kHigh)
#define EXPECT_LOW_PRIORITY(channel_id)                    \
  EXPECT_EQ(ChannelManager::ChannelPriority((channel_id)), \
            hci::ACLDataChannel::PacketPriority::kLow)

TEST_F(L2CAP_ChannelManagerTest, ChannelPriority) {
  EXPECT_HIGH_PRIORITY(kSignalingChannelId);
  EXPECT_HIGH_PRIORITY(kLESignalingChannelId);
  EXPECT_HIGH_PRIORITY(kSMPChannelId);
  EXPECT_HIGH_PRIORITY(kLESMPChannelId);

  EXPECT_LOW_PRIORITY(kFirstDynamicChannelId);
  EXPECT_LOW_PRIORITY(kLastACLDynamicChannelId);
  EXPECT_LOW_PRIORITY(kATTChannelId);
}

TEST_F(L2CAP_ChannelManagerTest, MtuOutboundChannelConfiguration) {
  constexpr uint16_t kRemoteMtu = kDefaultMTU - 1;
  constexpr uint16_t kLocalMtu = kMaxMTU;

  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<l2cap::Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  // Signaling channel packets should be sent with high priority.
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId, kRemoteMtu),
                        kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId, kRemoteMtu));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);
  EXPECT_EQ(kRemoteMtu, channel->max_tx_sdu_size());
  EXPECT_EQ(kLocalMtu, channel->max_rx_sdu_size());
}

TEST_F(L2CAP_ChannelManagerTest, MtuInboundChannelConfiguration) {
  constexpr uint16_t kRemoteMtu = kDefaultMTU - 1;
  constexpr uint16_t kLocalMtu = kMaxMTU;

  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [this, &channel](fbl::RefPtr<l2cap::Channel> opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->ActivateWithDispatcher(NopRxCallback, DoNothing, dispatcher()));
  };

  EXPECT_TRUE(
      chanmgr()->RegisterService(kTestPsm, kChannelParams, std::move(channel_cb), dispatcher()));

  CommandId kPeerConnectionRequestId = 3;
  const auto config_req_id = NextCommandId();

  EXPECT_ACL_PACKET_OUT(OutboundConnectionResponse(kPeerConnectionRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId, kRemoteMtu),
                        kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnectionRequestId));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId, kRemoteMtu));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);
  EXPECT_EQ(kRemoteMtu, channel->max_tx_sdu_size());
  EXPECT_EQ(kLocalMtu, channel->max_rx_sdu_size());
}

TEST_F(L2CAP_ChannelManagerTest, OutboundChannelConfigurationUsesChannelParameters) {
  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  const auto cmd_ids = QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  ReceiveAclDataPacket(testing::AclExtFeaturesInfoRsp(cmd_ids.extended_features_id, kTestHandle1,
                                                      kExtendedFeaturesBitEnhancedRetransmission));

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [&channel](fbl::RefPtr<Channel> activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT(
      OutboundConfigurationRequest(config_req_id, *chan_params.max_rx_sdu_size, *chan_params.mode),
      kHighPriority);
  const auto kInboundMtu = kDefaultMTU;
  EXPECT_ACL_PACKET_OUT(
      OutboundConfigurationResponse(kPeerConfigRequestId, kInboundMtu, chan_params.mode),
      kHighPriority);

  ActivateOutboundChannel(kTestPsm, chan_params, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(
      InboundConfigurationRequest(kPeerConfigRequestId, kInboundMtu, chan_params.mode));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);
  EXPECT_EQ(*chan_params.max_rx_sdu_size, channel->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, channel->mode());

  // Receiver Ready poll request should elicit a response if ERTM has been set up.
  EXPECT_ACL_PACKET_OUT(
      testing::AclSFrameReceiverReady(kTestHandle1, kRemoteId, /*receive_seq_num=*/0,
                                      /*is_poll_request=*/false, /*is_poll_response=*/true),
      kLowPriority);
  ReceiveAclDataPacket(testing::AclSFrameReceiverReady(kTestHandle1, kLocalId,
                                                       /*receive_seq_num=*/0,
                                                       /*is_poll_request=*/true,
                                                       /*is_poll_response=*/false));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
}

TEST_F(L2CAP_ChannelManagerTest, InboundChannelConfigurationUsesChannelParameters) {
  CommandId kPeerConnReqId = 3;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  const auto cmd_ids = QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  ReceiveAclDataPacket(testing::AclExtFeaturesInfoRsp(cmd_ids.extended_features_id, kTestHandle1,
                                                      kExtendedFeaturesBitEnhancedRetransmission));
  fbl::RefPtr<Channel> channel;
  auto channel_cb = [this, &channel](fbl::RefPtr<l2cap::Channel> opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->ActivateWithDispatcher(NopRxCallback, DoNothing, dispatcher()));
  };

  EXPECT_TRUE(
      chanmgr()->RegisterService(kTestPsm, chan_params, std::move(channel_cb), dispatcher()));

  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(OutboundConnectionResponse(kPeerConnReqId), kHighPriority);
  EXPECT_ACL_PACKET_OUT(
      OutboundConfigurationRequest(config_req_id, *chan_params.max_rx_sdu_size, *chan_params.mode),
      kHighPriority);
  const auto kInboundMtu = kDefaultMTU;
  EXPECT_ACL_PACKET_OUT(
      OutboundConfigurationResponse(kPeerConfigRequestId, kInboundMtu, chan_params.mode),
      kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnReqId));
  ReceiveAclDataPacket(
      InboundConfigurationRequest(kPeerConfigRequestId, kInboundMtu, chan_params.mode));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);
  EXPECT_EQ(*chan_params.max_rx_sdu_size, channel->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, channel->mode());

  // Receiver Ready poll request should elicit a response if ERTM has been set up.
  EXPECT_ACL_PACKET_OUT(
      testing::AclSFrameReceiverReady(kTestHandle1, kRemoteId, /*receive_seq_num=*/0,
                                      /*is_poll_request=*/false, /*is_poll_response=*/true),
      kLowPriority);
  ReceiveAclDataPacket(testing::AclSFrameReceiverReady(kTestHandle1, kLocalId,
                                                       /*receive_seq_num=*/0,
                                                       /*is_poll_request=*/true,
                                                       /*is_poll_response=*/false));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
}

TEST_F(L2CAP_ChannelManagerTest, UnregisteringUnknownHandleClearsPendingPacketsAndDoesNotCrash) {
  // Packet for unregistered handle should be queued.
  ReceiveAclDataPacket(testing::AclConnectionReq(1, kTestHandle1, kRemoteId, kTestPsm));
  chanmgr()->Unregister(kTestHandle1);

  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  // Since pending connection request packet was cleared, no response should be sent.
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest,
       PacketsRecievedAfterChannelDeactivatedAndBeforeRemoveChannelCalledAreDropped) {
  QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);

  fbl::RefPtr<Channel> channel;
  auto channel_cb = [this, &channel](fbl::RefPtr<l2cap::Channel> opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->ActivateWithDispatcher(NopRxCallback, DoNothing, dispatcher()));
  };

  EXPECT_TRUE(
      chanmgr()->RegisterService(kTestPsm, kChannelParams, std::move(channel_cb), dispatcher()));

  CommandId kPeerConnectionRequestId = 3;
  CommandId kLocalConfigRequestId = NextCommandId();

  EXPECT_ACL_PACKET_OUT(OutboundConnectionResponse(kPeerConnectionRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationRequest(kLocalConfigRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnectionRequestId));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(kLocalConfigRequestId));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel);

  auto kPacket = StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 4 bytes)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame header (length: 0 bytes, channel-id)
      0x00, 0x00, LowerBits(kLocalId), UpperBits(kLocalId));

  // channel marked inactive & LogicalLink::RemoveChannel added to dispatch loop.
  channel->Deactivate();
  // LogicalLink::RemoveChannel not dispatched yet, so ChannelImpl::HandleRxPdu will be called.
  // Inactive channel should drop packet.
  ReceiveAclDataPacket(kPacket);
}

TEST_F(L2CAP_ChannelManagerTest, ReceiveFixedChannelsInformationResponseWithNotSupportedResult) {
  const auto cmd_ids = QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  // Handler should check for result and not crash from reading mask or type.
  ReceiveAclDataPacket(testing::AclNotSupportedInformationResponse(
      cmd_ids.fixed_channels_supported_id, kTestHandle1));
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, ReceiveFixedChannelsInformationResponseWithIncorrectType) {
  const auto cmd_ids = QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  // Handler should check type and not attempt to read fixed channel mask.
  ReceiveAclDataPacket(
      testing::AclExtFeaturesInfoRsp(cmd_ids.fixed_channels_supported_id, kTestHandle1, 0));
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest, ReceiveFixedChannelsInformationResponseWithRejectStatus) {
  const auto cmd_ids = QueueRegisterACL(kTestHandle1, hci::Connection::Role::kMaster);
  // Handler should check status and not attempt to read fields.
  ReceiveAclDataPacket(
      testing::AclCommandRejectNotUnderstoodRsp(cmd_ids.fixed_channels_supported_id, kTestHandle1));
  RunLoopUntilIdle();
}

TEST_F(L2CAP_ChannelManagerTest,
       ReceiveValidConnectionParameterUpdateRequestAsMasterAndRespondWithAcceptedResult) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kSlaveLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  std::optional<hci::LEPreferredConnectionParameters> params;
  LEConnectionParameterUpdateCallback param_cb =
      [&params](const hci::LEPreferredConnectionParameters& cb_params) { params = cb_params; };

  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster, /*LinkErrorCallback=*/DoNothing,
             std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  EXPECT_LE_PACKET_OUT(testing::AclConnectionParameterUpdateRsp(
                           kParamReqId, kTestHandle1, ConnectionParameterUpdateResult::kAccepted),
                       kHighPriority);

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateReq(
      kParamReqId, kTestHandle1, kIntervalMin, kIntervalMax, kSlaveLatency, kTimeoutMult));
  RunLoopUntilIdle();

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kIntervalMin, params->min_interval());
  EXPECT_EQ(kIntervalMax, params->max_interval());
  EXPECT_EQ(kSlaveLatency, params->max_latency());
  EXPECT_EQ(kTimeoutMult, params->supervision_timeout());
}

// If an LE Slave host receives a Connection Parameter Update Request, it should reject it.
TEST_F(L2CAP_ChannelManagerTest,
       ReceiveValidConnectionParameterUpdateRequestAsSlaveAndRespondWithReject) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kSlaveLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  std::optional<hci::LEPreferredConnectionParameters> params;
  LEConnectionParameterUpdateCallback param_cb =
      [&params](const hci::LEPreferredConnectionParameters& cb_params) { params = cb_params; };

  RegisterLE(kTestHandle1, hci::Connection::Role::kSlave, /*LinkErrorCallback=*/DoNothing,
             std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  EXPECT_LE_PACKET_OUT(
      testing::AclCommandRejectNotUnderstoodRsp(kParamReqId, kTestHandle1, kLESignalingChannelId),
      kHighPriority);

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateReq(
      kParamReqId, kTestHandle1, kIntervalMin, kIntervalMax, kSlaveLatency, kTimeoutMult));
  RunLoopUntilIdle();

  ASSERT_FALSE(params.has_value());
}

TEST_F(L2CAP_ChannelManagerTest,
       ReceiveInvalidConnectionParameterUpdateRequestsAndRespondWithRejectedResult) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kSlaveLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  // Callback should not be called for request with invalid parameters.
  LEConnectionParameterUpdateCallback param_cb = [](auto /*params*/) { ADD_FAILURE(); };
  RegisterLE(kTestHandle1, hci::Connection::Role::kMaster, /*LinkErrorCallback=*/DoNothing,
             std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  std::array invalid_requests = {
      // interval min > interval max
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, /*interval_min=*/7,
                                               /*interval_max=*/6, kSlaveLatency, kTimeoutMult),
      // interval_min too small
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1,
                                               hci::kLEConnectionIntervalMin - 1, kIntervalMax,
                                               kSlaveLatency, kTimeoutMult),
      // interval max too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               hci::kLEConnectionIntervalMax + 1, kSlaveLatency,
                                               kTimeoutMult),
      // latency too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, hci::kLEConnectionLatencyMax + 1,
                                               kTimeoutMult),
      // timeout multiplier too small
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kSlaveLatency,
                                               hci::kLEConnectionSupervisionTimeoutMin - 1),
      // timeout multiplier too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kSlaveLatency,
                                               hci::kLEConnectionSupervisionTimeoutMax + 1)};

  for (auto& req : invalid_requests) {
    EXPECT_LE_PACKET_OUT(testing::AclConnectionParameterUpdateRsp(
                             kParamReqId, kTestHandle1, ConnectionParameterUpdateResult::kRejected),
                         kHighPriority);
    ReceiveAclDataPacket(req);
  }
  RunLoopUntilIdle();
}

}  // namespace
}  // namespace l2cap
}  // namespace bt
