// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "channel_manager.h"

#include <memory>
#include <type_traits>

#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager_mock_controller_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/mock_acl_data_channel.h"

namespace bt::l2cap {
namespace {

namespace hci_android = bt::hci_spec::vendor::android;
using namespace inspect::testing;
using namespace bt::testing;

using LEFixedChannels = ChannelManager::LEFixedChannels;

using AclPriority = pw::bluetooth::AclPriority;

constexpr hci_spec::ConnectionHandle kTestHandle1 = 0x0001;
constexpr hci_spec::ConnectionHandle kTestHandle2 = 0x0002;
constexpr PSM kTestPsm = 0x0001;
constexpr ChannelId kLocalId = 0x0040;
constexpr ChannelId kRemoteId = 0x9042;
constexpr CommandId kPeerConfigRequestId = 153;
constexpr hci::AclDataChannel::PacketPriority kLowPriority =
    hci::AclDataChannel::PacketPriority::kLow;
constexpr hci::AclDataChannel::PacketPriority kHighPriority =
    hci::AclDataChannel::PacketPriority::kHigh;
constexpr ChannelParameters kChannelParams;

constexpr zx::duration kFlushTimeout = zx::msec(10);
constexpr uint16_t kExpectedFlushTimeoutParam =
    16;  // 10ms * kFlushTimeoutMsToCommandParameterConversionFactor(1.6)

// 2x Information Requests: Extended Features, Fixed Channels Supported
constexpr size_t kConnectionCreationPacketCount = 2;

void DoNothing() {}
void NopRxCallback(ByteBufferPtr) {}
void NopLeConnParamCallback(const hci_spec::LEPreferredConnectionParameters&) {}
void NopSecurityCallback(hci_spec::ConnectionHandle, sm::SecurityLevel, sm::ResultFunction<>) {}

// Holds expected outbound data packets including the source location where the expectation is set.
struct PacketExpectation {
  const char* file_name;
  int line_number;
  DynamicByteBuffer data;
  bt::LinkType ll_type;
  hci::AclDataChannel::PacketPriority priority;
};

// Helpers to set an outbound packet expectation with the link type and source location
// boilerplate prefilled.
#define EXPECT_LE_PACKET_OUT(packet_buffer, priority) \
  ExpectOutboundPacket(bt::LinkType::kLE, (priority), (packet_buffer), __FILE__, __LINE__)

// EXPECT_ACL_PACKET_OUT is already defined by MockController.
#define EXPECT_ACL_PACKET_OUT_(packet_buffer, priority) \
  ExpectOutboundPacket(bt::LinkType::kACL, (priority), (packet_buffer), __FILE__, __LINE__)

auto MakeExtendedFeaturesInformationRequest(CommandId id, hci_spec::ConnectionHandle handle) {
  return StaticByteBuffer(
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

auto ConfigurationRequest(CommandId id, ChannelId dst_id, uint16_t mtu = kDefaultMTU,
                          std::optional<ChannelMode> mode = std::nullopt,
                          uint8_t max_inbound_transmissions = 0) {
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
        max_inbound_transmissions, 0x00, 0x00, 0x00, 0x00, LowerBits(kMaxInboundPduPayloadSize),
        UpperBits(kMaxInboundPduPayloadSize)));
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
                                 std::optional<ChannelMode> mode = std::nullopt,
                                 uint8_t max_inbound_transmissions = 0) {
  return ConfigurationRequest(id, kLocalId, mtu, mode, max_inbound_transmissions);
}

auto InboundConfigurationResponse(CommandId id) {
  return StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 14 bytes)
      0x01, 0x00, 0x0e, 0x00,

      // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
      0x0a, 0x00, 0x01, 0x00,

      // Configuration Response (ID: 2, length: 6, src cid, flags: 0,
      // result: success)
      0x05, id, 0x06, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 0x00, 0x00, 0x00, 0x00);
}

auto InboundConnectionRequest(CommandId id) {
  return StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Connection Request (ID, length: 4, psm, src cid)
      0x02, id, 0x04, 0x00, LowerBits(kTestPsm), UpperBits(kTestPsm), LowerBits(kRemoteId),
      UpperBits(kRemoteId));
}

auto OutboundConnectionRequest(CommandId id) {
  return StaticByteBuffer(
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
  return ConfigurationRequest(id, kRemoteId, mtu, mode, kErtmMaxInboundRetransmissions);
}

// |max_transmissions| is ignored per Core Spec v5.0 Vol 3, Part A, Sec 5.4 but still parameterized
// because this needs to match the value that is sent by our L2CAP configuration logic.
auto OutboundConfigurationResponse(CommandId id, uint16_t mtu = kDefaultMTU,
                                   std::optional<ChannelMode> mode = std::nullopt,
                                   uint8_t max_transmissions = 0) {
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
        0x04, 0x09, static_cast<uint8_t>(*mode), kErtmMaxUnackedInboundFrames, max_transmissions,
        LowerBits(kErtmReceiverReadyPollTimerMsecs), UpperBits(kErtmReceiverReadyPollTimerMsecs),
        LowerBits(kErtmMonitorTimerMsecs), UpperBits(kErtmMonitorTimerMsecs),
        LowerBits(kMaxInboundPduPayloadSize), UpperBits(kMaxInboundPduPayloadSize)));
  }

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

auto OutboundDisconnectionRequest(CommandId id) {
  return StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Request
      // (ID, length: 4, dst cid, src cid)
      0x06, id, 0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), LowerBits(kLocalId),
      UpperBits(kLocalId));
}

Channel::A2dpOffloadConfiguration BuildA2dpOffloadConfiguration(
    hci_android::A2dpCodecType codec = hci_android::A2dpCodecType::kSbc) {
  hci_android::A2dpScmsTEnable scms_t_enable;
  scms_t_enable.enabled = pw::bluetooth::emboss::GenericEnableParam::DISABLE;
  scms_t_enable.header = 0x00;

  hci_android::A2dpOffloadCodecInformation codec_information;
  switch (codec) {
    case hci_android::A2dpCodecType::kSbc:
      codec_information.sbc.blocklen_subbands_alloc_method = 0x00;
      codec_information.sbc.min_bitpool_value = 0x00;
      codec_information.sbc.max_bitpool_value = 0xFF;
      memset(codec_information.sbc.reserved, 0, sizeof(codec_information.sbc.reserved));
      break;
    case hci_android::A2dpCodecType::kAac:
      codec_information.aac.object_type = 0x00;
      codec_information.aac.variable_bit_rate = hci_android::A2dpAacEnableVariableBitRate::kDisable;
      memset(codec_information.aac.reserved, 0, sizeof(codec_information.aac.reserved));
      break;
    case hci_android::A2dpCodecType::kLdac:
      codec_information.ldac.vendor_id = 0x0000012D;
      codec_information.ldac.codec_id = 0x00AA;
      codec_information.ldac.bitrate_index = hci_android::A2dpBitrateIndex::kLow;
      codec_information.ldac.ldac_channel_mode = hci_android::A2dpLdacChannelMode::kStereo;
      memset(codec_information.ldac.reserved, 0, sizeof(codec_information.ldac.reserved));
      break;
    default:
      memset(codec_information.aptx.reserved, 0, sizeof(codec_information.aptx.reserved));
      break;
  }

  Channel::A2dpOffloadConfiguration config;
  config.codec = codec;
  config.max_latency = 0xFFFF;
  config.scms_t_enable = scms_t_enable;
  config.sampling_frequency = hci_android::A2dpSamplingFrequency::k44100Hz;
  config.bits_per_sample = hci_android::A2dpBitsPerSample::k16BitsPerSample;
  config.channel_mode = hci_android::A2dpChannelMode::kMono;
  config.encoded_audio_bit_rate = 0x0;
  config.codec_information = codec_information;

  return config;
}

using TestingBase = ControllerTest<MockController>;

// ChannelManager test fixture that uses MockAclDataChannel to inject inbound data and test
// outbound data. Unexpected outbound packets will cause test failures.
class ChannelManagerMockAclChannelTest : public TestingBase {
 public:
  ChannelManagerMockAclChannelTest() = default;
  ~ChannelManagerMockAclChannelTest() override = default;

  void SetUp() override { SetUp(hci_spec::kMaxACLPayloadSize, hci_spec::kMaxACLPayloadSize); }

  void SetUp(size_t max_acl_payload_size, size_t max_le_payload_size) {
    TestingBase::SetUp();

    acl_data_channel_.set_bredr_buffer_info(
        hci::DataBufferInfo(max_acl_payload_size, /*max_num_packets=*/1));
    acl_data_channel_.set_le_buffer_info(
        hci::DataBufferInfo(max_le_payload_size, /*max_num_packets=*/1));
    acl_data_channel_.set_send_packets_cb(
        fit::bind_member<&ChannelManagerMockAclChannelTest::SendPackets>(this));

    // TODO(63074): Make these tests not depend on strict channel ID ordering.
    chanmgr_ = ChannelManager::Create(&acl_data_channel_, transport()->command_channel(),
                                      /*random_channel_ids=*/false);
    packet_rx_handler_ = [this](std::unique_ptr<hci::ACLDataPacket> packet) {
      acl_data_channel_.ReceivePacket(std::move(packet));
    };

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
  [[nodiscard]] ChannelManager::LEFixedChannels RegisterLE(
      hci_spec::ConnectionHandle handle, pw::bluetooth::emboss::ConnectionRole role,
      LinkErrorCallback link_error_cb = DoNothing,
      LEConnectionParameterUpdateCallback cpuc = NopLeConnParamCallback,
      SecurityUpgradeCallback suc = NopSecurityCallback) {
    return chanmgr()->AddLEConnection(handle, role, std::move(link_error_cb), std::move(cpuc),
                                      std::move(suc));
  }

  struct QueueRegisterACLRetVal {
    CommandId extended_features_id;
    CommandId fixed_channels_supported_id;
  };

  QueueRegisterACLRetVal QueueRegisterACL(hci_spec::ConnectionHandle handle,
                                          pw::bluetooth::emboss::ConnectionRole role,
                                          LinkErrorCallback link_error_cb = DoNothing,
                                          SecurityUpgradeCallback suc = NopSecurityCallback) {
    QueueRegisterACLRetVal cmd_ids;
    cmd_ids.extended_features_id = NextCommandId();
    cmd_ids.fixed_channels_supported_id = NextCommandId();

    EXPECT_ACL_PACKET_OUT_(
        MakeExtendedFeaturesInformationRequest(cmd_ids.extended_features_id, handle),
        kHighPriority);
    EXPECT_ACL_PACKET_OUT_(
        testing::AclFixedChannelsSupportedInfoReq(cmd_ids.fixed_channels_supported_id, handle),
        kHighPriority);
    RegisterACL(handle, role, std::move(link_error_cb), std::move(suc));
    return cmd_ids;
  }

  void RegisterACL(hci_spec::ConnectionHandle handle, pw::bluetooth::emboss::ConnectionRole role,
                   LinkErrorCallback link_error_cb = DoNothing,
                   SecurityUpgradeCallback suc = NopSecurityCallback) {
    chanmgr()->AddACLConnection(handle, role, std::move(link_error_cb), std::move(suc));
  }

  void ReceiveL2capInformationResponses(CommandId extended_features_id,
                                        CommandId fixed_channels_supported_id,
                                        l2cap::ExtendedFeatures features = 0u,
                                        l2cap::FixedChannelsSupported channels = 0u) {
    ReceiveAclDataPacket(
        testing::AclExtFeaturesInfoRsp(extended_features_id, kTestHandle1, features));
    ReceiveAclDataPacket(testing::AclFixedChannelsSupportedInfoRsp(fixed_channels_supported_id,
                                                                   kTestHandle1, channels));
  }

  Channel::WeakPtr ActivateNewFixedChannel(ChannelId id,
                                           hci_spec::ConnectionHandle conn_handle = kTestHandle1,
                                           Channel::ClosedCallback closed_cb = DoNothing,
                                           Channel::RxCallback rx_cb = NopRxCallback) {
    auto chan = chanmgr()->OpenFixedChannel(conn_handle, id);
    if (!chan.is_alive() || !chan->Activate(std::move(rx_cb), std::move(closed_cb))) {
      return Channel::WeakPtr();
    }

    return chan;
  }

  // |activated_cb| will be called with opened and activated Channel if
  // successful and nullptr otherwise.
  void ActivateOutboundChannel(PSM psm, ChannelParameters chan_params, ChannelCallback activated_cb,
                               hci_spec::ConnectionHandle conn_handle = kTestHandle1,
                               Channel::ClosedCallback closed_cb = DoNothing,
                               Channel::RxCallback rx_cb = NopRxCallback) {
    ChannelCallback open_cb = [activated_cb = std::move(activated_cb), rx_cb = std::move(rx_cb),
                               closed_cb = std::move(closed_cb)](auto chan) mutable {
      if (!chan.is_alive() || !chan->Activate(std::move(rx_cb), std::move(closed_cb))) {
        activated_cb(Channel::WeakPtr());
      } else {
        activated_cb(std::move(chan));
      }
    };
    chanmgr()->OpenL2capChannel(conn_handle, psm, chan_params, std::move(open_cb));
  }

  void SetUpOutboundChannelWithCallback(ChannelId local_id, ChannelId remote_id,
                                        Channel::ClosedCallback closed_cb,
                                        ChannelParameters channel_params,
                                        fit::function<void(Channel::WeakPtr)> channel_cb) {
    const auto conn_req_id = NextCommandId();
    const auto config_req_id = NextCommandId();
    EXPECT_ACL_PACKET_OUT_(testing::AclConnectionReq(conn_req_id, kTestHandle1, local_id, kTestPsm),
                           kHighPriority);
    EXPECT_ACL_PACKET_OUT_(
        testing::AclConfigReq(config_req_id, kTestHandle1, remote_id, kChannelParams),
        kHighPriority);
    EXPECT_ACL_PACKET_OUT_(
        testing::AclConfigRsp(kPeerConfigRequestId, kTestHandle1, remote_id, kChannelParams),
        kHighPriority);

    ActivateOutboundChannel(kTestPsm, channel_params, std::move(channel_cb), kTestHandle1,
                            std::move(closed_cb));
    RunLoopUntilIdle();

    ReceiveAclDataPacket(testing::AclConnectionRsp(conn_req_id, kTestHandle1, local_id, remote_id));
    ReceiveAclDataPacket(
        testing::AclConfigReq(kPeerConfigRequestId, kTestHandle1, local_id, kChannelParams));
    ReceiveAclDataPacket(
        testing::AclConfigRsp(config_req_id, kTestHandle1, local_id, kChannelParams));

    RunLoopUntilIdle();
    EXPECT_TRUE(AllExpectedPacketsSent());
  }

  Channel::WeakPtr SetUpOutboundChannel(ChannelId local_id = kLocalId,
                                        ChannelId remote_id = kRemoteId,
                                        Channel::ClosedCallback closed_cb = DoNothing,
                                        ChannelParameters channel_params = kChannelParams) {
    Channel::WeakPtr channel;
    auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
      channel = std::move(activated_chan);
    };

    SetUpOutboundChannelWithCallback(local_id, remote_id, std::move(closed_cb), channel_params,
                                     channel_cb);
    EXPECT_TRUE(channel.is_alive());
    return channel;
  }

  // Set an expectation for an outbound ACL data packet. Packets are expected in the order that
  // they're added. The test fails if not all expected packets have been set when the test case
  // completes or if the outbound data doesn't match expectations, including the ordering between
  // LE and ACL packets.
  void ExpectOutboundPacket(bt::LinkType ll_type, hci::AclDataChannel::PacketPriority priority,
                            const ByteBuffer& data, const char* file_name = "",
                            int line_number = 0) {
    expected_packets_.push({file_name, line_number, DynamicByteBuffer(data), ll_type, priority});
  }

  void ActivateOutboundErtmChannel(ChannelCallback activated_cb,
                                   hci_spec::ConnectionHandle conn_handle = kTestHandle1,
                                   uint8_t max_outbound_transmit = 3,
                                   Channel::ClosedCallback closed_cb = DoNothing,
                                   Channel::RxCallback rx_cb = NopRxCallback) {
    l2cap::ChannelParameters chan_params;
    chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;

    const auto conn_req_id = NextCommandId();
    const auto config_req_id = NextCommandId();
    EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
    EXPECT_ACL_PACKET_OUT_(
        OutboundConfigurationRequest(config_req_id, kMaxInboundPduPayloadSize, *chan_params.mode),
        kHighPriority);
    const auto kInboundMtu = kDefaultMTU;
    EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId, kInboundMtu,
                                                         chan_params.mode, max_outbound_transmit),
                           kHighPriority);

    ActivateOutboundChannel(kTestPsm, chan_params, std::move(activated_cb), conn_handle,
                            std::move(closed_cb), std::move(rx_cb));

    ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
    ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId, kInboundMtu,
                                                     chan_params.mode, max_outbound_transmit));
    ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));
  }

  // Returns true if all expected outbound packets up to this call have been sent by the test case.
  [[nodiscard]] bool AllExpectedPacketsSent() const { return expected_packets_.empty(); }

  void ReceiveAclDataPacket(const ByteBuffer& packet) {
    const size_t payload_size = packet.size() - sizeof(hci_spec::ACLDataHeader);
    BT_ASSERT(payload_size <= std::numeric_limits<uint16_t>::max());
    hci::ACLDataPacketPtr acl_packet = hci::ACLDataPacket::New(static_cast<uint16_t>(payload_size));
    auto mutable_acl_packet_data = acl_packet->mutable_view()->mutable_data();
    packet.Copy(&mutable_acl_packet_data);
    packet_rx_handler_(std::move(acl_packet));
  }

  ChannelManager* chanmgr() const { return chanmgr_.get(); }

  hci::testing::MockAclDataChannel* acl_data_channel() { return &acl_data_channel_; }

  CommandId NextCommandId() { return next_command_id_++; }

 private:
  bool SendPackets(std::list<hci::ACLDataPacketPtr> packets, ChannelId channel_id,
                   hci::AclDataChannel::PacketPriority priority) {
    for (const auto& packet : packets) {
      const ByteBuffer& data = packet->view().data();
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
                    << static_cast<std::underlying_type_t<hci::AclDataChannel::PacketPriority>>(
                           expected.priority)
                    << std::endl;
          std::cout << "Found: "
                    << static_cast<std::underlying_type_t<hci::AclDataChannel::PacketPriority>>(
                           priority)
                    << std::endl;
          ADD_FAILURE_AT(expected.file_name, expected.line_number)
              << "Outbound ACL priority doesn't match expected";
        }

        expected_packets_.pop();
      }
    }
    return !packets.empty();
  }

  std::unique_ptr<ChannelManager> chanmgr_;
  hci::ACLPacketHandler packet_rx_handler_;
  hci::testing::MockAclDataChannel acl_data_channel_;

  std::queue<const PacketExpectation> expected_packets_;

  CommandId next_command_id_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelManagerMockAclChannelTest);
};

// ChannelManager test fixture that uses a real AclDataChannel and uses MockController
// for HCI packet expectations.
using ChannelManagerRealAclChannelTest = ChannelManagerMockControllerTest;

TEST_F(ChannelManagerMockAclChannelTest, OpenFixedChannelErrorNoConn) {
  // This should fail as the ChannelManager has no entry for |kTestHandle1|.
  EXPECT_FALSE(ActivateNewFixedChannel(kATTChannelId).is_alive());

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  // This should fail as the ChannelManager has no entry for |kTestHandle2|.
  EXPECT_FALSE(ActivateNewFixedChannel(kATTChannelId, kTestHandle2).is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, OpenFixedChannelErrorDisallowedId) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  // ACL-U link
  QueueRegisterACL(kTestHandle2, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  // This should fail as kSMPChannelId is ACL-U only.
  EXPECT_FALSE(ActivateNewFixedChannel(kSMPChannelId, kTestHandle1).is_alive());

  // This should fail as kATTChannelId is LE-U only.
  EXPECT_FALSE(ActivateNewFixedChannel(kATTChannelId, kTestHandle2).is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, DeactivateDynamicChannelInvalidatesChannelPointer) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);
  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };
  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1, []() {});
  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));
  RunLoopUntilIdle();

  ASSERT_TRUE(channel.is_alive());
  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);
  channel->Deactivate();
  ASSERT_FALSE(channel.is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, DeactivateAttChannelInvalidatesChannelPointer) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));
  fixed_channels.att->Deactivate();
  ASSERT_FALSE(fixed_channels.att.is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, OpenFixedChannelAndUnregisterLink) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };

  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, closed_cb));
  EXPECT_EQ(kTestHandle1, fixed_channels.att->link_handle());

  // This should notify the channel.
  chanmgr()->RemoveConnection(kTestHandle1);

  RunLoopUntilIdle();

  // |closed_cb| will be called synchronously since it was registered using the
  // current thread's task runner.
  EXPECT_TRUE(closed_called);
}

TEST_F(ChannelManagerMockAclChannelTest, OpenFixedChannelAndCloseChannel) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };

  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, closed_cb));

  // Close the channel before unregistering the link. |closed_cb| should not get
  // called.
  fixed_channels.att->Deactivate();
  chanmgr()->RemoveConnection(kTestHandle1);

  RunLoopUntilIdle();

  EXPECT_FALSE(closed_called);
}

TEST_F(ChannelManagerMockAclChannelTest, FixedChannelsUseBasicMode) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));
  EXPECT_EQ(ChannelMode::kBasic, fixed_channels.att->mode());
}

TEST_F(ChannelManagerMockAclChannelTest, OpenAndCloseWithLinkMultipleFixedChannels) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool att_closed = false;
  auto att_closed_cb = [&att_closed] { att_closed = true; };
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, att_closed_cb));

  bool smp_closed = false;
  auto smp_closed_cb = [&smp_closed] { smp_closed = true; };
  ASSERT_TRUE(fixed_channels.smp->Activate(NopRxCallback, smp_closed_cb));

  fixed_channels.smp->Deactivate();
  chanmgr()->RemoveConnection(kTestHandle1);

  RunLoopUntilIdle();

  EXPECT_TRUE(att_closed);
  EXPECT_FALSE(smp_closed);
}

TEST_F(ChannelManagerMockAclChannelTest,
       SendingPacketsBeforeRemoveConnectionAndVerifyChannelClosed) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };
  auto chan = fixed_channels.att;
  ASSERT_TRUE(chan.is_alive());
  ASSERT_TRUE(chan->Activate(NopRxCallback, closed_cb));

  EXPECT_LE_PACKET_OUT(StaticByteBuffer(
                           // ACL data header (handle: 1, length: 6)
                           0x01, 0x00, 0x06, 0x00,

                           // L2CAP B-frame (length: 2, channel-id: ATT)
                           0x02, 0x00, LowerBits(kATTChannelId), UpperBits(kATTChannelId),

                           'h', 'i'),
                       kLowPriority);

  // Send a packet. This should be processed immediately.
  EXPECT_TRUE(chan->Send(NewBuffer('h', 'i')));
  EXPECT_TRUE(AllExpectedPacketsSent());

  chanmgr()->RemoveConnection(kTestHandle1);

  // The L2CAP channel should have been notified of closure immediately.
  EXPECT_TRUE(closed_called);
  EXPECT_FALSE(chan.is_alive());
  RunLoopUntilIdle();
}

// Tests that destroying the ChannelManager cleanly shuts down all channels.
TEST_F(ChannelManagerMockAclChannelTest, DestroyingChannelManagerCleansUpChannels) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };
  auto chan = fixed_channels.att;
  ASSERT_TRUE(chan.is_alive());
  ASSERT_TRUE(chan->Activate(NopRxCallback, closed_cb));

  EXPECT_LE_PACKET_OUT(StaticByteBuffer(
                           // ACL data header (handle: 1, length: 6)
                           0x01, 0x00, 0x06, 0x00,

                           // L2CAP B-frame (length: 2, channel-id: ATT)
                           0x02, 0x00, LowerBits(kATTChannelId), UpperBits(kATTChannelId),

                           'h', 'i'),
                       kLowPriority);

  // Send a packet. This should be processed immediately.
  EXPECT_TRUE(chan->Send(NewBuffer('h', 'i')));
  EXPECT_TRUE(AllExpectedPacketsSent());

  TearDown();

  EXPECT_TRUE(closed_called);
  EXPECT_FALSE(chan.is_alive());
  // No outbound packet expectations were set, so this test will fail if it sends any data.
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, DeactivateDoesNotCrashOrHang) {
  // Tests that the clean up task posted to the LogicalLink does not crash when
  // a dynamic registry is not present (which is the case for LE links).
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att.is_alive());
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));
  fixed_channels.att->Deactivate();

  // Loop until the clean up task runs.
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, CallingDeactivateFromClosedCallbackDoesNotCrashOrHang) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto chan = chanmgr()->OpenFixedChannel(kTestHandle1, kSMPChannelId);
  chan->Activate(NopRxCallback, [chan] { chan->Deactivate(); });
  chanmgr()->RemoveConnection(kTestHandle1);  // Triggers ClosedCallback.
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, ReceiveData) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att.is_alive());
  ASSERT_TRUE(fixed_channels.smp.is_alive());

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  std::vector<std::string> sdus;
  auto att_rx_cb = [&sdus](ByteBufferPtr sdu) {
    BT_DEBUG_ASSERT(sdu);
    sdus.push_back(sdu->ToString());
  };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    BT_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  ASSERT_TRUE(fixed_channels.att->Activate(att_rx_cb, DoNothing));
  ASSERT_TRUE(fixed_channels.smp->Activate(smp_rx_cb, DoNothing));

  // ATT channel
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame
      0x05, 0x00, 0x04, 0x00, 'h', 'e', 'l', 'l', 'o'));
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame (partial)
      0x0C, 0x00, 0x04, 0x00, 'h', 'o', 'w', ' ', 'a'));
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (continuing fragment)
      0x01, 0x10, 0x07, 0x00,

      // L2CAP B-frame (partial)
      'r', 'e', ' ', 'y', 'o', 'u', '?'));

  // SMP channel
  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, ReceiveDataBeforeRegisteringLink) {
  constexpr size_t kPacketCount = 10;

  StaticByteBuffer<255> buffer;

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  size_t packet_count = 0;
  auto att_rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    BT_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  // ATT channel
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(StaticByteBuffer(
        // ACL data header (starting fragment)
        0x01, 0x00, 0x04, 0x00,

        // L2CAP B-frame
        0x00, 0x00, 0x04, 0x00));
  }

  // SMP channel
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, 0x06, 0x00));

  Channel::WeakPtr att_chan, smp_chan;

  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(att_rx_cb, DoNothing));
  ASSERT_TRUE(fixed_channels.smp->Activate(smp_rx_cb, DoNothing));

  RunLoopUntilIdle();
  EXPECT_TRUE(smp_cb_called);
  EXPECT_EQ(kPacketCount, packet_count);
}

// Receive data after registering the link but before creating a fixed channel.
TEST_F(ChannelManagerMockAclChannelTest, ReceiveDataBeforeCreatingFixedChannel) {
  constexpr size_t kPacketCount = 10;

  // Register an ACL connection because LE connections create fixed channels immediately.
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  StaticByteBuffer<255> buffer;

  size_t packet_count = 0;
  auto rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(StaticByteBuffer(
        // ACL data header (starting fragment)
        LowerBits(kTestHandle1), UpperBits(kTestHandle1), 0x04, 0x00,

        // L2CAP B-frame (empty)
        0x00, 0x00, LowerBits(kSMPChannelId), UpperBits(kSMPChannelId)));
  }
  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  auto chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle1, DoNothing, std::move(rx_cb));

  RunLoopUntilIdle();
  EXPECT_EQ(kPacketCount, packet_count);
}

// Receive data after registering the link and creating the channel but before
// setting the rx handler.
TEST_F(ChannelManagerMockAclChannelTest, ReceiveDataBeforeSettingRxHandler) {
  constexpr size_t kPacketCount = 10;

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  StaticByteBuffer<255> buffer;

  // We use the ATT channel to control incoming packets and the SMP channel to
  // quit the message loop.
  size_t packet_count = 0;
  auto att_rx_cb = [&packet_count](ByteBufferPtr sdu) { packet_count++; };

  bool smp_cb_called = false;
  auto smp_rx_cb = [&smp_cb_called](ByteBufferPtr sdu) {
    BT_DEBUG_ASSERT(sdu);
    EXPECT_EQ(0u, sdu->size());
    smp_cb_called = true;
  };

  // ATT channel
  for (size_t i = 0u; i < kPacketCount; i++) {
    ReceiveAclDataPacket(StaticByteBuffer(
        // ACL data header (starting fragment)
        0x01, 0x00, 0x04, 0x00,

        // L2CAP B-frame
        0x00, 0x00, LowerBits(kATTChannelId), UpperBits(kATTChannelId)));
  }

  // SMP channel
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame (empty)
      0x00, 0x00, LowerBits(kLESMPChannelId), UpperBits(kLESMPChannelId)));

  // Run the loop so all packets are received.
  RunLoopUntilIdle();

  fixed_channels.att->Activate(att_rx_cb, DoNothing);
  fixed_channels.smp->Activate(smp_rx_cb, DoNothing);

  RunLoopUntilIdle();

  EXPECT_TRUE(smp_cb_called);
  EXPECT_EQ(kPacketCount, packet_count);
}

TEST_F(ChannelManagerMockAclChannelTest, ActivateChannelProcessesCallbacksSynchronously) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  int att_rx_cb_count = 0;
  int smp_rx_cb_count = 0;

  auto att_rx_cb = [&att_rx_cb_count](ByteBufferPtr sdu) {
    EXPECT_EQ("hello", sdu->AsString());
    att_rx_cb_count++;
  };
  bool att_closed_called = false;
  auto att_closed_cb = [&att_closed_called] { att_closed_called = true; };

  ASSERT_TRUE(fixed_channels.att->Activate(std::move(att_rx_cb), std::move(att_closed_cb)));

  auto smp_rx_cb = [&smp_rx_cb_count](ByteBufferPtr sdu) {
    EXPECT_EQ("🤨", sdu->AsString());
    smp_rx_cb_count++;
  };
  bool smp_closed_called = false;
  auto smp_closed_cb = [&smp_closed_called] { smp_closed_called = true; };

  ASSERT_TRUE(fixed_channels.smp->Activate(std::move(smp_rx_cb), std::move(smp_closed_cb)));

  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame for SMP fixed channel (4-byte payload: U+1F928 in UTF-8)
      0x04, 0x00, 0x06, 0x00, 0xf0, 0x9f, 0xa4, 0xa8));

  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00, 0x09, 0x00,

      // L2CAP B-frame for ATT fixed channel
      0x05, 0x00, 0x04, 0x00, 'h', 'e', 'l', 'l', 'o'));

  // Receiving data in ChannelManager processes the ATT and SMP packets synchronously so it has
  // already routed the data to the Channels.
  EXPECT_EQ(1, att_rx_cb_count);
  EXPECT_EQ(1, smp_rx_cb_count);
  RunLoopUntilIdle();
  EXPECT_EQ(1, att_rx_cb_count);
  EXPECT_EQ(1, smp_rx_cb_count);

  // Link closure synchronously calls the ATT and SMP channel close callbacks.
  chanmgr()->RemoveConnection(kTestHandle1);
  EXPECT_TRUE(att_closed_called);
  EXPECT_TRUE(smp_closed_called);
}

TEST_F(ChannelManagerMockAclChannelTest, RemovingLinkInvalidatesChannelPointer) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  BT_ASSERT(fixed_channels.att->Activate(NopRxCallback, DoNothing));
  chanmgr()->RemoveConnection(kTestHandle1);
  EXPECT_FALSE(fixed_channels.att.is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, SendBasicSdu) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  BT_ASSERT(fixed_channels.att->Activate(NopRxCallback, DoNothing));

  EXPECT_LE_PACKET_OUT(StaticByteBuffer(
                           // ACL data header (handle: 1, length 7)
                           0x01, 0x00, 0x08, 0x00,

                           // L2CAP B-frame: (length: 3, channel-id: 4)
                           0x04, 0x00, 0x04, 0x00, 'T', 'e', 's', 't'),
                       kLowPriority);

  EXPECT_TRUE(fixed_channels.att->Send(NewBuffer('T', 'e', 's', 't')));
  RunLoopUntilIdle();
}

// Tests that fragmentation of BR/EDR packets uses the BR/EDR buffer size.
TEST_F(ChannelManagerMockAclChannelTest, SendBrEdrFragmentedSdus) {
  constexpr size_t kMaxBrEdrDataSize = 6;
  constexpr size_t kMaxLEDataSize = 5;

  TearDown();
  SetUp(kMaxBrEdrDataSize, kMaxLEDataSize);

  // Send fragmented Extended Features Information Request
  EXPECT_ACL_PACKET_OUT_(StaticByteBuffer(
                             // ACL data header (handle: 2, length: 6)
                             0x02, 0x00, 0x06, 0x00,

                             // L2CAP B-frame (length: 6, channel-id: 1)
                             0x06, 0x00, 0x01, 0x00,

                             // Extended Features Information Request
                             // (code = 0x0A, ID)
                             0x0A, NextCommandId()),
                         kHighPriority);
  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 2, pbf: continuing fr., length: 4)
          0x02, 0x10, 0x04, 0x00,

          // Extended Features Information Request cont.
          // (Length: 2, type)
          0x02, 0x00, LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
          UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported))),
      kHighPriority);

  // Send fragmented Fixed Channels Supported Information Request
  EXPECT_ACL_PACKET_OUT_(StaticByteBuffer(
                             // ACL data header (handle: 2, length: 6)
                             0x02, 0x00, 0x06, 0x00,

                             // L2CAP B-frame (length: 6, channel-id: 1)
                             0x06, 0x00, 0x01, 0x00,

                             // Fixed Channels Supported Information Request
                             // (command code, command ID)
                             l2cap::kInformationRequest, NextCommandId()),
                         kHighPriority);
  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 2, pbf: continuing fr., length: 4)
          0x02, 0x10, 0x04, 0x00,

          // Fixed Channels Supported Information Request cont.
          // (length: 2, type)
          0x02, 0x00, LowerBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported)),
          UpperBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported))),
      kHighPriority);
  RegisterACL(kTestHandle2, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  auto sm_chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle2);
  ASSERT_TRUE(sm_chan.is_alive());

  EXPECT_ACL_PACKET_OUT_(StaticByteBuffer(
                             // ACL data header (handle: 2, length: 6)
                             0x02, 0x00, 0x06, 0x00,

                             // l2cap b-frame: (length: 7, channel-id: 7, partial payload)
                             0x07, 0x00, 0x07, 0x00, 'G', 'o'),
                         kHighPriority);

  EXPECT_ACL_PACKET_OUT_(StaticByteBuffer(
                             // ACL data header (handle: 2, pbf: continuing fr., length: 5)
                             0x02, 0x10, 0x05, 0x00,

                             // continuing payload
                             'o', 'd', 'b', 'y', 'e'),
                         kHighPriority);

  // SDU of length 7 corresponds to a 11-octet B-frame. Due to the BR/EDR buffer size, this should
  // be sent over a 6-byte then a 5-byte fragment.
  EXPECT_TRUE(sm_chan->Send(NewBuffer('G', 'o', 'o', 'd', 'b', 'y', 'e')));

  RunLoopUntilIdle();
}

// Tests that fragmentation of LE packets uses the LE buffer size.
TEST_F(ChannelManagerMockAclChannelTest, SendFragmentedSdus) {
  constexpr size_t kMaxBrEdrDataSize = 6;
  constexpr size_t kMaxLEDataSize = 5;

  TearDown();
  SetUp(kMaxBrEdrDataSize, kMaxLEDataSize);

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));

  EXPECT_LE_PACKET_OUT(StaticByteBuffer(
                           // ACL data header (handle: 1, length: 5)
                           0x01, 0x00, 0x05, 0x00,

                           // L2CAP B-frame: (length: 5, channel-id: 4, partial payload)
                           0x05, 0x00, 0x04, 0x00, 'H'),
                       kLowPriority);

  EXPECT_LE_PACKET_OUT(StaticByteBuffer(
                           // ACL data header (handle: 1, pbf: continuing fr., length: 4)
                           0x01, 0x10, 0x04, 0x00,

                           // Continuing payload
                           'e', 'l', 'l', 'o'),
                       kLowPriority);

  // SDU of length 5 corresponds to a 9-octet B-frame which should be sent over a 5-byte and a 4-
  // byte fragment.
  EXPECT_TRUE(fixed_channels.att->Send(NewBuffer('H', 'e', 'l', 'l', 'o')));

  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, LEChannelSignalLinkError) {
  bool link_error = false;
  auto link_error_cb = [&link_error] { link_error = true; };
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL, link_error_cb);

  // Activate a new Attribute channel to signal the error.
  fixed_channels.att->Activate(NopRxCallback, DoNothing);
  fixed_channels.att->SignalLinkError();

  RunLoopUntilIdle();

  EXPECT_TRUE(link_error);
}

TEST_F(ChannelManagerMockAclChannelTest, ACLChannelSignalLinkError) {
  bool link_error = false;
  auto link_error_cb = [&link_error] { link_error = true; };
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL, link_error_cb);

  // Activate a new Security Manager channel to signal the error.
  auto chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle1);
  chan->SignalLinkError();

  RunLoopUntilIdle();

  EXPECT_TRUE(link_error);
}

TEST_F(ChannelManagerMockAclChannelTest, SignalLinkErrorDisconnectsChannels) {
  bool link_error = false;
  auto link_error_cb = [this, &link_error] {
    // This callback is run after the expectation for OutboundDisconnectionRequest is set, so this
    // tests that L2CAP-level teardown happens before ChannelManager requests a link teardown.
    ASSERT_TRUE(AllExpectedPacketsSent());
    link_error = true;

    // Simulate closing the link.
    chanmgr()->RemoveConnection(kTestHandle1);
  };
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL, link_error_cb);

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  Channel::WeakPtr dynamic_channel;
  auto channel_cb = [&dynamic_channel](l2cap::Channel::WeakPtr activated_chan) {
    dynamic_channel = std::move(activated_chan);
  };

  int dynamic_channel_closed = 0;
  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          /*closed_cb=*/[&dynamic_channel_closed] { dynamic_channel_closed++; });

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(AllExpectedPacketsSent());

  // The channel on kTestHandle1 should be open.
  EXPECT_TRUE(dynamic_channel.is_alive());
  EXPECT_EQ(0, dynamic_channel_closed);

  EXPECT_TRUE(AllExpectedPacketsSent());
  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  // Activate a new Security Manager channel to signal the error on kTestHandle1.
  int fixed_channel_closed = 0;
  auto fixed_channel =
      ActivateNewFixedChannel(kSMPChannelId, kTestHandle1,
                              /*closed_cb=*/[&fixed_channel_closed] { fixed_channel_closed++; });

  ASSERT_FALSE(link_error);
  fixed_channel->SignalLinkError();

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // link_error_cb is not called until Disconnection Response is received for each dynamic channel.
  EXPECT_FALSE(link_error);

  // But channels should be deactivated to prevent any activity.
  EXPECT_EQ(1, fixed_channel_closed);
  EXPECT_EQ(1, dynamic_channel_closed);

  ASSERT_TRUE(AllExpectedPacketsSent());
  const auto disconnection_rsp =
      testing::AclDisconnectionRsp(disconn_req_id, kTestHandle1, kLocalId, kRemoteId);
  ReceiveAclDataPacket(disconnection_rsp);

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_TRUE(link_error);
}

TEST_F(ChannelManagerMockAclChannelTest, LEConnectionParameterUpdateRequest) {
  bool conn_param_cb_called = false;
  auto conn_param_cb = [&conn_param_cb_called](const auto& params) {
    // The parameters should match the payload of the HCI packet seen below.
    EXPECT_EQ(0x0006, params.min_interval());
    EXPECT_EQ(0x0C80, params.max_interval());
    EXPECT_EQ(0x01F3, params.max_latency());
    EXPECT_EQ(0x0C80, params.supervision_timeout());
    conn_param_cb_called = true;
  };

  EXPECT_ACL_PACKET_OUT_(StaticByteBuffer(
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

  LEFixedChannels fixed_channels = RegisterLE(
      kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL, DoNothing, conn_param_cb);

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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

auto OutboundDisconnectionResponse(CommandId id) {
  return StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 12 bytes)
      0x01, 0x00, 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Disconnection Response
      // (ID, length: 4, dst cid, src cid)
      0x07, id, 0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), LowerBits(kRemoteId),
      UpperBits(kRemoteId));
}

TEST_F(ChannelManagerMockAclChannelTest, ACLOutboundDynamicChannelLocalDisconnect) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb));
  RunLoopUntilIdle();

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  ASSERT_TRUE(channel.is_alive());
  EXPECT_FALSE(closed_cb_called);
  EXPECT_EQ(kLocalId, channel->id());
  EXPECT_EQ(kRemoteId, channel->remote_id());
  EXPECT_EQ(ChannelMode::kBasic, channel->mode());

  // Test SDU transmission.
  // SDU must have remote channel ID (unlike for fixed channels).
  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, length 8)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 4, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());

  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  // Packets for testing filter against
  constexpr hci_spec::ConnectionHandle kTestHandle2 = 0x02;
  constexpr ChannelId kWrongChannelId = 0x02;
  auto dummy_packet1 =
      hci::ACLDataPacket::New(kTestHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci_spec::ACLBroadcastFlag::kPointToPoint, 0x00);
  auto dummy_packet2 =
      hci::ACLDataPacket::New(kTestHandle2, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci_spec::ACLBroadcastFlag::kPointToPoint, 0x00);
  size_t filter_cb_count = 0;
  auto filter_cb = [&](hci::AclDataChannel::AclPacketPredicate filter) {
    // filter out correct closed channel on correct connection handle
    EXPECT_TRUE(filter(dummy_packet1, kLocalId));
    // do not filter out other channels
    EXPECT_FALSE(filter(dummy_packet1, kWrongChannelId));
    // do not filter out other connections
    EXPECT_FALSE(filter(dummy_packet2, kLocalId));
    filter_cb_count++;
  };
  acl_data_channel()->set_drop_queued_packets_cb(std::move(filter_cb));

  // Explicit deactivation should not result in |closed_cb| being called.
  channel->Deactivate();

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_EQ(1u, filter_cb_count);

  // Ensure callback is not called after the channel has disconnected
  acl_data_channel()->set_drop_queued_packets_cb(nullptr);

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, ACLOutboundDynamicChannelRemoteDisconnect) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  bool channel_closed = false;
  auto closed_cb = [&channel_closed] { channel_closed = true; };

  bool sdu_received = false;
  auto data_rx_cb = [&sdu_received](ByteBufferPtr sdu) {
    sdu_received = true;
    BT_DEBUG_ASSERT(sdu);
    EXPECT_EQ("Test", sdu->AsString());
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb), std::move(data_rx_cb));

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_FALSE(channel_closed);

  // Test SDU reception.
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));

  RunLoopUntilIdle();
  EXPECT_TRUE(sdu_received);

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionResponse(7), kHighPriority);

  // Packets for testing filter against
  constexpr hci_spec::ConnectionHandle kTestHandle2 = 0x02;
  constexpr ChannelId kWrongChannelId = 0x02;
  auto dummy_packet1 =
      hci::ACLDataPacket::New(kTestHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci_spec::ACLBroadcastFlag::kPointToPoint, 0x00);
  auto dummy_packet2 =
      hci::ACLDataPacket::New(kTestHandle2, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                              hci_spec::ACLBroadcastFlag::kPointToPoint, 0x00);
  size_t filter_cb_count = 0;
  auto filter_cb = [&](hci::AclDataChannel::AclPacketPredicate filter) {
    // filter out correct closed channel
    EXPECT_TRUE(filter(dummy_packet1, kLocalId));
    // do not filter out other channels
    EXPECT_FALSE(filter(dummy_packet1, kWrongChannelId));
    // do not filter out other connections
    EXPECT_FALSE(filter(dummy_packet2, kLocalId));
    filter_cb_count++;
  };
  acl_data_channel()->set_drop_queued_packets_cb(std::move(filter_cb));

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (handle: 1, length 5)
      0x01, 0x00, 0x05, 0x00,

      // L2CAP B-frame: (length: 1, channel-id: 0x0040)
      0x01, 0x00, 0x40, 0x00, '!'));

  RunLoopUntilIdle();

  EXPECT_TRUE(channel_closed);
  EXPECT_FALSE(sdu_received);
  EXPECT_EQ(1u, filter_cb_count);

  // Ensure callback is not called after the channel has disconnected
  acl_data_channel()->set_drop_queued_packets_cb(nullptr);
}

TEST_F(ChannelManagerMockAclChannelTest, ACLOutboundDynamicChannelDataNotBuffered) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  bool channel_closed = false;
  auto closed_cb = [&channel_closed] { channel_closed = true; };

  auto data_rx_cb = [](ByteBufferPtr sdu) { FAIL() << "Unexpected data reception"; };

  // Receive SDU for the channel about to be opened. It should be ignored.
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1,
                          std::move(closed_cb), std::move(data_rx_cb));
  RunLoopUntilIdle();

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));

  // The channel is connected but not configured, so no data should flow on the
  // channel. Test that this received data is also ignored.
  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id)
      0x04, 0x00, LowerBits(kLocalId), UpperBits(kLocalId), 'T', 'e', 's', 't'));
  // clang-format on

  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_FALSE(channel_closed);

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionResponse(7), kHighPriority);

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, ACLOutboundDynamicChannelRemoteRefused) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool channel_cb_called = false;
  auto channel_cb = [&channel_cb_called](l2cap::Channel::WeakPtr channel) {
    channel_cb_called = true;
    EXPECT_FALSE(channel.is_alive());
  };

  const CommandId conn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb));

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, ACLOutboundDynamicChannelFailedConfiguration) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool channel_cb_called = false;
  auto channel_cb = [&channel_cb_called](l2cap::Channel::WeakPtr channel) {
    channel_cb_called = true;
    EXPECT_FALSE(channel.is_alive());
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb));

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (handle: 0x0001, length: 14 bytes)
      0x01, 0x00, 0x0e, 0x00,

      // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
      0x0a, 0x00, 0x01, 0x00,

      // Configuration Response (ID, length: 6, src cid, flags: 0,
      // result: 0x0002 (Rejected; no reason provided))
      0x05, config_req_id, 0x06, 0x00,
      LowerBits(kLocalId), UpperBits(kLocalId), 0x00, 0x00,
      0x02, 0x00));

  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, ACLInboundDynamicChannelLocalDisconnect) {
  constexpr PSM kBadPsm0 = 0x0004;
  constexpr PSM kBadPsm1 = 0x0103;

  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };

  Channel::WeakPtr channel;
  auto channel_cb = [&channel,
                     closed_cb = std::move(closed_cb)](l2cap::Channel::WeakPtr opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->Activate(NopRxCallback, std::move(closed_cb)));
  };

  EXPECT_FALSE(chanmgr()->RegisterService(kBadPsm0, ChannelParameters(), channel_cb));
  EXPECT_FALSE(chanmgr()->RegisterService(kBadPsm1, ChannelParameters(), channel_cb));
  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, ChannelParameters(), std::move(channel_cb)));

  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionResponse(1), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(1));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  ASSERT_TRUE(channel.is_alive());
  EXPECT_FALSE(closed_cb_called);
  EXPECT_EQ(kLocalId, channel->id());
  EXPECT_EQ(kRemoteId, channel->remote_id());

  // Test SDU transmission.
  // SDU must have remote channel ID (unlike for fixed channels).
  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, length 7)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 3, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());

  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);

  // Explicit deactivation should not result in |closed_cb| being called.
  channel->Deactivate();

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());

  // clang-format off
  ReceiveAclDataPacket(StaticByteBuffer(
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

TEST_F(ChannelManagerMockAclChannelTest, LinkSecurityProperties) {
  sm::SecurityProperties security(sm::SecurityLevel::kEncrypted, 16, /*secure_connections=*/false);

  // Has no effect.
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);

  // Register a link and open a channel. The security properties should be
  // accessible using the channel.
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));

  // The channel should start out at the lowest level of security.
  EXPECT_EQ(sm::SecurityProperties(), fixed_channels.att->security());

  // Assign a new security level.
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);

  // Channel should return the new security level.
  EXPECT_EQ(security, fixed_channels.att->security());
}

TEST_F(ChannelManagerMockAclChannelTest, AssignLinkSecurityPropertiesOnClosedLinkDoesNothing) {
  // Register a link and open a channel. The security properties should be
  // accessible using the channel.
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ASSERT_TRUE(fixed_channels.att->Activate(NopRxCallback, DoNothing));

  chanmgr()->RemoveConnection(kTestHandle1);
  RunLoopUntilIdle();
  EXPECT_FALSE(fixed_channels.att.is_alive());

  // Assign a new security level.
  sm::SecurityProperties security(sm::SecurityLevel::kEncrypted, 16, /*secure_connections=*/false);
  chanmgr()->AssignLinkSecurityProperties(kTestHandle1, security);
}

TEST_F(ChannelManagerMockAclChannelTest, UpgradeSecurity) {
  // The callback passed to to Channel::UpgradeSecurity().
  sm::Result<> received_status = fit::ok();
  int security_status_count = 0;
  auto status_callback = [&](sm::Result<> status) {
    received_status = status;
    security_status_count++;
  };

  // The security handler callback assigned when registering a link.
  sm::Result<> delivered_status = fit::ok();
  sm::SecurityLevel last_requested_level = sm::SecurityLevel::kNoSecurity;
  int security_request_count = 0;
  auto security_handler = [&](hci_spec::ConnectionHandle handle, sm::SecurityLevel level,
                              auto callback) {
    EXPECT_EQ(kTestHandle1, handle);
    last_requested_level = level;
    security_request_count++;

    callback(delivered_status);
  };

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL, DoNothing,
                 NopLeConnParamCallback, std::move(security_handler));
  l2cap::Channel::WeakPtr att = std::move(fixed_channels.att);
  ASSERT_TRUE(att->Activate(NopRxCallback, DoNothing));

  // Requesting security at or below the current level should succeed without
  // doing anything.
  att->UpgradeSecurity(sm::SecurityLevel::kNoSecurity, status_callback);
  RunLoopUntilIdle();
  EXPECT_EQ(0, security_request_count);
  EXPECT_EQ(1, security_status_count);
  EXPECT_EQ(fit::ok(), received_status);

  // Test reporting an error.
  delivered_status = ToResult(HostError::kNotSupported);
  att->UpgradeSecurity(sm::SecurityLevel::kEncrypted, status_callback);
  RunLoopUntilIdle();
  EXPECT_EQ(1, security_request_count);
  EXPECT_EQ(2, security_status_count);
  EXPECT_EQ(delivered_status, received_status);
  EXPECT_EQ(sm::SecurityLevel::kEncrypted, last_requested_level);

  chanmgr()->RemoveConnection(kTestHandle1);
  RunLoopUntilIdle();
  EXPECT_FALSE(att.is_alive());
  EXPECT_EQ(1, security_request_count);
  EXPECT_EQ(2, security_status_count);
}

TEST_F(ChannelManagerMockAclChannelTest, SignalingChannelDataPrioritizedOverDynamicChannelData) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  // Signaling channel packets should be sent with high priority.
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());

  // Packet sent on dynamic channel should be sent with low priority.
  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, length 8)
          0x01, 0x00, 0x08, 0x00,

          // L2CAP B-frame: (length: 4, channel-id)
          0x04, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId), 'T', 'e', 's', 't'),
      kLowPriority);

  EXPECT_TRUE(channel->Send(NewBuffer('T', 'e', 's', 't')));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
}

TEST_F(ChannelManagerMockAclChannelTest, MtuOutboundChannelConfiguration) {
  constexpr uint16_t kRemoteMtu = kDefaultMTU - 1;
  constexpr uint16_t kLocalMtu = kMaxMTU;

  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();

  // Signaling channel packets should be sent with high priority.
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId, kRemoteMtu),
                         kHighPriority);

  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId, kRemoteMtu));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_EQ(kRemoteMtu, channel->max_tx_sdu_size());
  EXPECT_EQ(kLocalMtu, channel->max_rx_sdu_size());
}

TEST_F(ChannelManagerMockAclChannelTest, MtuInboundChannelConfiguration) {
  constexpr uint16_t kRemoteMtu = kDefaultMTU - 1;
  constexpr uint16_t kLocalMtu = kMaxMTU;

  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->Activate(NopRxCallback, DoNothing));
  };

  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, kChannelParams, std::move(channel_cb)));

  CommandId kPeerConnectionRequestId = 3;
  const auto config_req_id = NextCommandId();

  EXPECT_ACL_PACKET_OUT_(OutboundConnectionResponse(kPeerConnectionRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId, kRemoteMtu),
                         kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnectionRequestId));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId, kRemoteMtu));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_EQ(kRemoteMtu, channel->max_tx_sdu_size());
  EXPECT_EQ(kLocalMtu, channel->max_rx_sdu_size());
}

TEST_F(ChannelManagerMockAclChannelTest, OutboundChannelConfigurationUsesChannelParameters) {
  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ReceiveAclDataPacket(testing::AclExtFeaturesInfoRsp(cmd_ids.extended_features_id, kTestHandle1,
                                                      kExtendedFeaturesBitEnhancedRetransmission));

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(
      OutboundConfigurationRequest(config_req_id, *chan_params.max_rx_sdu_size, *chan_params.mode),
      kHighPriority);
  const auto kInboundMtu = kDefaultMTU;
  EXPECT_ACL_PACKET_OUT_(
      OutboundConfigurationResponse(kPeerConfigRequestId, kInboundMtu, chan_params.mode),
      kHighPriority);

  ActivateOutboundChannel(kTestPsm, chan_params, std::move(channel_cb), kTestHandle1);

  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(
      InboundConfigurationRequest(kPeerConfigRequestId, kInboundMtu, chan_params.mode));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_EQ(*chan_params.max_rx_sdu_size, channel->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, channel->mode());

  // Receiver Ready poll request should elicit a response if ERTM has been set up.
  EXPECT_ACL_PACKET_OUT_(
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

TEST_F(ChannelManagerMockAclChannelTest, InboundChannelConfigurationUsesChannelParameters) {
  CommandId kPeerConnReqId = 3;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  ReceiveAclDataPacket(testing::AclExtFeaturesInfoRsp(cmd_ids.extended_features_id, kTestHandle1,
                                                      kExtendedFeaturesBitEnhancedRetransmission));
  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->Activate(NopRxCallback, DoNothing));
  };

  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, chan_params, std::move(channel_cb)));

  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionResponse(kPeerConnReqId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(
      OutboundConfigurationRequest(config_req_id, *chan_params.max_rx_sdu_size, *chan_params.mode),
      kHighPriority);
  const auto kInboundMtu = kDefaultMTU;
  EXPECT_ACL_PACKET_OUT_(
      OutboundConfigurationResponse(kPeerConfigRequestId, kInboundMtu, chan_params.mode),
      kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnReqId));
  ReceiveAclDataPacket(
      InboundConfigurationRequest(kPeerConfigRequestId, kInboundMtu, chan_params.mode));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());
  EXPECT_EQ(*chan_params.max_rx_sdu_size, channel->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, channel->mode());

  // Receiver Ready poll request should elicit a response if ERTM has been set up.
  EXPECT_ACL_PACKET_OUT_(
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

TEST_F(ChannelManagerMockAclChannelTest,
       UnregisteringUnknownHandleClearsPendingPacketsAndDoesNotCrash) {
  // Packet for unregistered handle should be queued.
  ReceiveAclDataPacket(testing::AclConnectionReq(1, kTestHandle1, kRemoteId, kTestPsm));
  chanmgr()->RemoveConnection(kTestHandle1);

  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  // Since pending connection request packet was cleared, no response should be sent.
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest,
       PacketsReceivedAfterChannelDeactivatedAndBeforeRemoveChannelCalledAreDropped) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->Activate(NopRxCallback, DoNothing));
  };

  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, kChannelParams, std::move(channel_cb)));

  CommandId kPeerConnectionRequestId = 3;
  CommandId kLocalConfigRequestId = NextCommandId();

  EXPECT_ACL_PACKET_OUT_(OutboundConnectionResponse(kPeerConnectionRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(kLocalConfigRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnectionRequestId));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(kLocalConfigRequestId));

  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(channel.is_alive());

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);

  // channel marked inactive & LogicalLink::RemoveChannel called.
  channel->Deactivate();
  EXPECT_TRUE(AllExpectedPacketsSent());

  StaticByteBuffer kPacket(
      // ACL data header (handle: 0x0001, length: 4 bytes)
      0x01, 0x00, 0x04, 0x00,

      // L2CAP B-frame header (length: 0 bytes, channel-id)
      0x00, 0x00, LowerBits(kLocalId), UpperBits(kLocalId));

  // Packet for removed channel should be dropped by LogicalLink.
  ReceiveAclDataPacket(kPacket);
}

TEST_F(ChannelManagerMockAclChannelTest,
       ReceiveFixedChannelsInformationResponseWithNotSupportedResult) {
  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  // Handler should check for result and not crash from reading mask or type.
  ReceiveAclDataPacket(testing::AclNotSupportedInformationResponse(
      cmd_ids.fixed_channels_supported_id, kTestHandle1));
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, ReceiveFixedChannelsInformationResponseWithInvalidResult) {
  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  // Handler should check for result and not crash from reading mask or type.
  StaticByteBuffer kPacket(
      // ACL data header (handle: |link_handle|, length: 12 bytes)
      LowerBits(kTestHandle1), UpperBits(kTestHandle1), 0x0c, 0x00,
      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,
      // Information Response (type, ID, length: 4)
      l2cap::kInformationResponse, cmd_ids.fixed_channels_supported_id, 0x04, 0x00,
      // Type = Fixed Channels Supported
      LowerBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kFixedChannelsSupported)),
      // Invalid Result
      0xFF, 0xFF);
  ReceiveAclDataPacket(kPacket);
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, ReceiveFixedChannelsInformationResponseWithIncorrectType) {
  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  // Handler should check type and not attempt to read fixed channel mask.
  ReceiveAclDataPacket(
      testing::AclExtFeaturesInfoRsp(cmd_ids.fixed_channels_supported_id, kTestHandle1, 0));
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, ReceiveFixedChannelsInformationResponseWithRejectStatus) {
  const auto cmd_ids =
      QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  // Handler should check status and not attempt to read fields.
  ReceiveAclDataPacket(
      testing::AclCommandRejectNotUnderstoodRsp(cmd_ids.fixed_channels_supported_id, kTestHandle1));
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest,
       ReceiveValidConnectionParameterUpdateRequestAsCentralAndRespondWithAcceptedResult) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  std::optional<hci_spec::LEPreferredConnectionParameters> params;
  LEConnectionParameterUpdateCallback param_cb =
      [&params](const hci_spec::LEPreferredConnectionParameters& cb_params) { params = cb_params; };

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL,
                 /*link_error_cb=*/DoNothing, std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  EXPECT_LE_PACKET_OUT(testing::AclConnectionParameterUpdateRsp(
                           kParamReqId, kTestHandle1, ConnectionParameterUpdateResult::kAccepted),
                       kHighPriority);

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateReq(
      kParamReqId, kTestHandle1, kIntervalMin, kIntervalMax, kPeripheralLatency, kTimeoutMult));
  RunLoopUntilIdle();

  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(kIntervalMin, params->min_interval());
  EXPECT_EQ(kIntervalMax, params->max_interval());
  EXPECT_EQ(kPeripheralLatency, params->max_latency());
  EXPECT_EQ(kTimeoutMult, params->supervision_timeout());
}

// If an LE Peripheral host receives a Connection Parameter Update Request, it should reject it.
TEST_F(ChannelManagerMockAclChannelTest,
       ReceiveValidConnectionParameterUpdateRequestAsPeripheralAndRespondWithReject) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  std::optional<hci_spec::LEPreferredConnectionParameters> params;
  LEConnectionParameterUpdateCallback param_cb =
      [&params](const hci_spec::LEPreferredConnectionParameters& cb_params) { params = cb_params; };

  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::PERIPHERAL,
                 /*link_error_cb=*/DoNothing, std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  EXPECT_LE_PACKET_OUT(
      testing::AclCommandRejectNotUnderstoodRsp(kParamReqId, kTestHandle1, kLESignalingChannelId),
      kHighPriority);

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateReq(
      kParamReqId, kTestHandle1, kIntervalMin, kIntervalMax, kPeripheralLatency, kTimeoutMult));
  RunLoopUntilIdle();

  ASSERT_FALSE(params.has_value());
}

TEST_F(ChannelManagerMockAclChannelTest,
       ReceiveInvalidConnectionParameterUpdateRequestsAndRespondWithRejectedResult) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;

  // Callback should not be called for request with invalid parameters.
  LEConnectionParameterUpdateCallback param_cb = [](auto /*params*/) { ADD_FAILURE(); };
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL,
                 /*link_error_cb=*/DoNothing, std::move(param_cb));

  constexpr CommandId kParamReqId = 4;  // random

  std::array invalid_requests = {
      // interval min > interval max
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, /*interval_min=*/7,
                                               /*interval_max=*/6, kPeripheralLatency,
                                               kTimeoutMult),
      // interval_min too small
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1,
                                               hci_spec::kLEConnectionIntervalMin - 1, kIntervalMax,
                                               kPeripheralLatency, kTimeoutMult),
      // interval max too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               hci_spec::kLEConnectionIntervalMax + 1,
                                               kPeripheralLatency, kTimeoutMult),
      // latency too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, hci_spec::kLEConnectionLatencyMax + 1,
                                               kTimeoutMult),
      // timeout multiplier too small
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kPeripheralLatency,
                                               hci_spec::kLEConnectionSupervisionTimeoutMin - 1),
      // timeout multiplier too large
      testing::AclConnectionParameterUpdateReq(kParamReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kPeripheralLatency,
                                               hci_spec::kLEConnectionSupervisionTimeoutMax + 1)};

  for (auto& req : invalid_requests) {
    EXPECT_LE_PACKET_OUT(testing::AclConnectionParameterUpdateRsp(
                             kParamReqId, kTestHandle1, ConnectionParameterUpdateResult::kRejected),
                         kHighPriority);
    ReceiveAclDataPacket(req);
  }
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest, RequestConnParamUpdateForUnknownLinkIsNoOp) {
  auto update_cb = [](auto) { ADD_FAILURE(); };
  chanmgr()->RequestConnectionParameterUpdate(
      kTestHandle1, hci_spec::LEPreferredConnectionParameters(), std::move(update_cb));
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerMockAclChannelTest,
       RequestConnParamUpdateAsPeripheralAndReceiveAcceptedAndRejectedResponses) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::PERIPHERAL);

  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;
  const hci_spec::LEPreferredConnectionParameters kParams(kIntervalMin, kIntervalMax,
                                                          kPeripheralLatency, kTimeoutMult);

  std::optional<bool> accepted;
  auto request_cb = [&accepted](bool cb_accepted) { accepted = cb_accepted; };

  // Receive "Accepted" Response:

  CommandId param_update_req_id = NextCommandId();
  EXPECT_LE_PACKET_OUT(
      testing::AclConnectionParameterUpdateReq(param_update_req_id, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kPeripheralLatency, kTimeoutMult),
      kHighPriority);
  chanmgr()->RequestConnectionParameterUpdate(kTestHandle1, kParams, request_cb);
  RunLoopUntilIdle();
  EXPECT_FALSE(accepted.has_value());

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateRsp(
      param_update_req_id, kTestHandle1, ConnectionParameterUpdateResult::kAccepted));
  RunLoopUntilIdle();
  ASSERT_TRUE(accepted.has_value());
  EXPECT_TRUE(accepted.value());
  accepted.reset();

  // Receive "Rejected" Response:

  param_update_req_id = NextCommandId();
  EXPECT_LE_PACKET_OUT(
      testing::AclConnectionParameterUpdateReq(param_update_req_id, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kPeripheralLatency, kTimeoutMult),
      kHighPriority);
  chanmgr()->RequestConnectionParameterUpdate(kTestHandle1, kParams, std::move(request_cb));
  RunLoopUntilIdle();
  EXPECT_FALSE(accepted.has_value());

  ReceiveAclDataPacket(testing::AclConnectionParameterUpdateRsp(
      param_update_req_id, kTestHandle1, ConnectionParameterUpdateResult::kRejected));
  RunLoopUntilIdle();
  ASSERT_TRUE(accepted.has_value());
  EXPECT_FALSE(accepted.value());
}

TEST_F(ChannelManagerMockAclChannelTest, ConnParamUpdateRequestRejected) {
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::PERIPHERAL);

  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;
  const hci_spec::LEPreferredConnectionParameters kParams(kIntervalMin, kIntervalMax,
                                                          kPeripheralLatency, kTimeoutMult);

  std::optional<bool> accepted;
  auto request_cb = [&accepted](bool cb_accepted) { accepted = cb_accepted; };

  const CommandId kParamUpdateReqId = NextCommandId();
  EXPECT_LE_PACKET_OUT(
      testing::AclConnectionParameterUpdateReq(kParamUpdateReqId, kTestHandle1, kIntervalMin,
                                               kIntervalMax, kPeripheralLatency, kTimeoutMult),
      kHighPriority);
  chanmgr()->RequestConnectionParameterUpdate(kTestHandle1, kParams, request_cb);
  RunLoopUntilIdle();
  EXPECT_FALSE(accepted.has_value());

  ReceiveAclDataPacket(testing::AclCommandRejectNotUnderstoodRsp(kParamUpdateReqId, kTestHandle1,
                                                                 kLESignalingChannelId));
  RunLoopUntilIdle();
  ASSERT_TRUE(accepted.has_value());
  EXPECT_FALSE(accepted.value());
}

TEST_F(ChannelManagerMockAclChannelTest,
       DestroyingChannelManagerReleasesLogicalLinkAndClosesChannels) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();
  auto link = chanmgr()->LogicalLinkForTesting(kTestHandle1);
  ASSERT_TRUE(link.is_alive());

  bool closed = false;
  auto closed_cb = [&] { closed = true; };

  auto chan = ActivateNewFixedChannel(kSMPChannelId, kTestHandle1, closed_cb);
  ASSERT_TRUE(chan.is_alive());
  ASSERT_FALSE(closed);

  TearDown();  // Destroys channel manager
  RunLoopUntilIdle();
  EXPECT_TRUE(closed);
  // If link is still valid, there may be a memory leak.
  EXPECT_FALSE(link.is_alive());

  // If the above fails, check if the channel was holding a strong reference to the link.
  chan = Channel::WeakPtr();
  RunLoopUntilIdle();
  EXPECT_TRUE(closed);
  EXPECT_FALSE(link.is_alive());
}

TEST_F(ChannelManagerMockAclChannelTest, RequestAclPriorityNormal) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb(
      [&requested_priority](auto priority, auto handle, auto cb) {
        EXPECT_EQ(handle, kTestHandle1);
        requested_priority = priority;
        cb(fit::ok());
      });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kNormal, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_FALSE(requested_priority.has_value());

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  // Closing channel should not request normal priority because it is already the current priority.
  channel->Deactivate();
  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_FALSE(requested_priority.has_value());
}

TEST_F(ChannelManagerMockAclChannelTest, RequestAclPrioritySinkThenNormal) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb(
      [&requested_priority](auto priority, auto handle, auto cb) {
        EXPECT_EQ(handle, kTestHandle1);
        requested_priority = priority;
        cb(fit::ok());
      });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kSink);
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kSink);

  channel->RequestAclPriority(AclPriority::kNormal, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 2u);
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kNormal);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kNormal);

  requested_priority.reset();

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  // Closing channel should not request normal priority because it is already the current priority.
  channel->Deactivate();
  EXPECT_FALSE(requested_priority.has_value());
}

TEST_F(ChannelManagerMockAclChannelTest, RequestAclPrioritySinkThenDeactivateChannelAfterResult) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb(
      [&requested_priority](auto priority, auto handle, auto cb) {
        EXPECT_EQ(handle, kTestHandle1);
        requested_priority = priority;
        cb(fit::ok());
      });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 1u);
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kSink);

  requested_priority.reset();

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  channel->Deactivate();
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kNormal);
}

TEST_F(ChannelManagerMockAclChannelTest, RequestAclPrioritySinkThenReceiveDisconnectRequest) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb(
      [&requested_priority](auto priority, auto handle, auto cb) {
        EXPECT_EQ(handle, kTestHandle1);
        requested_priority = priority;
        cb(fit::ok());
      });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 1u);
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kSink);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kSink);

  requested_priority.reset();

  const auto kPeerDisconReqId = 1;
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionResponse(kPeerDisconReqId), kHighPriority);
  ReceiveAclDataPacket(
      testing::AclDisconnectionReq(kPeerDisconReqId, kTestHandle1, kRemoteId, kLocalId));
  RunLoopUntilIdle();
  ASSERT_TRUE(requested_priority.has_value());
  EXPECT_EQ(*requested_priority, AclPriority::kNormal);
}

TEST_F(ChannelManagerMockAclChannelTest,
       RequestAclPrioritySinkThenDeactivateChannelBeforeResultShouldResetPriorityOnDeactivate) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::vector<std::pair<AclPriority, fit::callback<void(fit::result<fit::failed>)>>> requests;
  acl_data_channel()->set_request_acl_priority_cb([&](auto priority, auto handle, auto cb) {
    EXPECT_EQ(handle, kTestHandle1);
    requests.push_back({priority, std::move(cb)});
  });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kNormal);
  EXPECT_EQ(result_cb_count, 0u);
  EXPECT_EQ(requests.size(), 1u);

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  // Should queue kNormal ACL priority request.
  channel->Deactivate();
  ASSERT_EQ(requests.size(), 1u);

  requests[0].second(fit::ok());
  EXPECT_EQ(result_cb_count, 1u);
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(requests[1].first, AclPriority::kNormal);

  requests[1].second(fit::ok());
}

TEST_F(ChannelManagerMockAclChannelTest, RequestAclPrioritySinkFails) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  acl_data_channel()->set_request_acl_priority_cb([](auto priority, auto handle, auto cb) {
    EXPECT_EQ(handle, kTestHandle1);
    cb(fit::failed());
  });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_TRUE(result.is_error());
    result_cb_count++;
  });

  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kNormal);

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  channel->Deactivate();
}

TEST_F(ChannelManagerMockAclChannelTest, TwoChannelsRequestAclPrioritySinkAndDeactivate) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto kChannelIds0 = std::make_pair(ChannelId(0x40), ChannelId(0x41));
  const auto kChannelIds1 = std::make_pair(ChannelId(0x41), ChannelId(0x42));

  auto channel_0 = SetUpOutboundChannel(kChannelIds0.first, kChannelIds0.second);
  auto channel_1 = SetUpOutboundChannel(kChannelIds1.first, kChannelIds1.second);

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb([&](auto priority, auto handle, auto cb) {
    EXPECT_EQ(handle, kTestHandle1);
    requested_priority = priority;
    cb(fit::ok());
  });

  size_t result_cb_count = 0;
  channel_0->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  ASSERT_TRUE(requested_priority);
  EXPECT_EQ(*requested_priority, AclPriority::kSink);
  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_EQ(channel_0->requested_acl_priority(), AclPriority::kSink);
  requested_priority.reset();

  channel_1->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  // Priority is already sink. No additional request should be sent.
  EXPECT_FALSE(requested_priority);
  EXPECT_EQ(result_cb_count, 2u);
  EXPECT_EQ(channel_1->requested_acl_priority(), AclPriority::kSink);

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds0.first, kChannelIds0.second),
                         kHighPriority);
  channel_0->Deactivate();
  // Because channel_1 is still using sink priority, no command should be sent.
  EXPECT_FALSE(requested_priority);

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds1.first, kChannelIds1.second),
                         kHighPriority);
  channel_1->Deactivate();
  ASSERT_TRUE(requested_priority);
  EXPECT_EQ(*requested_priority, AclPriority::kNormal);
}

TEST_F(ChannelManagerMockAclChannelTest, TwoChannelsRequestConflictingAclPriorities) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto kChannelIds0 = std::make_pair(ChannelId(0x40), ChannelId(0x41));
  const auto kChannelIds1 = std::make_pair(ChannelId(0x41), ChannelId(0x42));

  auto channel_0 = SetUpOutboundChannel(kChannelIds0.first, kChannelIds0.second);
  auto channel_1 = SetUpOutboundChannel(kChannelIds1.first, kChannelIds1.second);

  std::optional<AclPriority> requested_priority;
  acl_data_channel()->set_request_acl_priority_cb([&](auto priority, auto handle, auto cb) {
    EXPECT_EQ(handle, kTestHandle1);
    requested_priority = priority;
    cb(fit::ok());
  });

  size_t result_cb_count = 0;
  channel_0->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  ASSERT_TRUE(requested_priority);
  EXPECT_EQ(*requested_priority, AclPriority::kSink);
  EXPECT_EQ(result_cb_count, 1u);
  requested_priority.reset();

  channel_1->RequestAclPriority(AclPriority::kSource, [&](auto result) {
    EXPECT_TRUE(result.is_error());
    result_cb_count++;
  });
  // Priority conflict should prevent priority request.
  EXPECT_FALSE(requested_priority);
  EXPECT_EQ(result_cb_count, 2u);
  EXPECT_EQ(channel_1->requested_acl_priority(), AclPriority::kNormal);

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds0.first, kChannelIds0.second),
                         kHighPriority);
  channel_0->Deactivate();
  ASSERT_TRUE(requested_priority);
  EXPECT_EQ(*requested_priority, AclPriority::kNormal);
  requested_priority.reset();

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds1.first, kChannelIds1.second),
                         kHighPriority);
  channel_1->Deactivate();
  EXPECT_FALSE(requested_priority);
}

// If two channels request ACL priorities before the first command completes, they should receive
// responses as if they were handled strictly sequentially.
TEST_F(ChannelManagerMockAclChannelTest, TwoChannelsRequestAclPrioritiesAtSameTime) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto kChannelIds0 = std::make_pair(ChannelId(0x40), ChannelId(0x41));
  const auto kChannelIds1 = std::make_pair(ChannelId(0x41), ChannelId(0x42));

  auto channel_0 = SetUpOutboundChannel(kChannelIds0.first, kChannelIds0.second);
  auto channel_1 = SetUpOutboundChannel(kChannelIds1.first, kChannelIds1.second);

  std::vector<fit::callback<void(fit::result<fit::failed>)>> command_callbacks;
  acl_data_channel()->set_request_acl_priority_cb(
      [&](auto priority, auto handle, auto cb) { command_callbacks.push_back(std::move(cb)); });

  size_t result_cb_count_0 = 0;
  channel_0->RequestAclPriority(AclPriority::kSink, [&](auto result) { result_cb_count_0++; });
  EXPECT_EQ(command_callbacks.size(), 1u);
  EXPECT_EQ(result_cb_count_0, 0u);

  size_t result_cb_count_1 = 0;
  channel_1->RequestAclPriority(AclPriority::kSource, [&](auto result) { result_cb_count_1++; });
  EXPECT_EQ(result_cb_count_1, 0u);
  ASSERT_EQ(command_callbacks.size(), 1u);

  command_callbacks[0](fit::ok());
  EXPECT_EQ(result_cb_count_0, 1u);
  // Second request should be notified of conflict error.
  EXPECT_EQ(result_cb_count_1, 1u);
  EXPECT_EQ(command_callbacks.size(), 1u);

  // Because requests should be handled sequentially, the second request should have failed.
  EXPECT_EQ(channel_0->requested_acl_priority(), AclPriority::kSink);
  EXPECT_EQ(channel_1->requested_acl_priority(), AclPriority::kNormal);

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds0.first, kChannelIds0.second),
                         kHighPriority);
  channel_0->Deactivate();

  EXPECT_ACL_PACKET_OUT_(testing::AclDisconnectionReq(NextCommandId(), kTestHandle1,
                                                      kChannelIds1.first, kChannelIds1.second),
                         kHighPriority);
  channel_1->Deactivate();
}

TEST_F(ChannelManagerMockAclChannelTest, QueuedSinkAclPriorityForClosedChannelIsIgnored) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  auto channel = SetUpOutboundChannel();

  std::vector<std::pair<AclPriority, fit::callback<void(fit::result<fit::failed>)>>> requests;
  acl_data_channel()->set_request_acl_priority_cb([&](auto priority, auto handle, auto cb) {
    EXPECT_EQ(handle, kTestHandle1);
    requests.push_back({priority, std::move(cb)});
  });

  size_t result_cb_count = 0;
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  ASSERT_EQ(requests.size(), 1u);
  requests[0].second(fit::ok());
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kSink);

  // Source request is queued and request is sent.
  channel->RequestAclPriority(AclPriority::kSource, [&](auto result) {
    EXPECT_EQ(fit::ok(), result);
    result_cb_count++;
  });
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kSink);

  // Sink request is queued. It should receive an error since it is handled after the channel is
  // closed.
  channel->RequestAclPriority(AclPriority::kSink, [&](auto result) {
    EXPECT_TRUE(result.is_error());
    result_cb_count++;
  });
  ASSERT_EQ(requests.size(), 2u);
  EXPECT_EQ(result_cb_count, 1u);
  EXPECT_EQ(channel->requested_acl_priority(), AclPriority::kSink);

  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(NextCommandId()), kHighPriority);
  // Closing channel will queue normal request.
  channel->Deactivate();
  EXPECT_FALSE(channel.is_alive());

  // Send result to source request. Second sink request should receive error result too.
  requests[1].second(fit::ok());
  EXPECT_EQ(result_cb_count, 3u);
  ASSERT_EQ(requests.size(), 3u);
  EXPECT_EQ(requests[2].first, AclPriority::kNormal);

  // Send response to kNormal request sent on Deactivate().
  requests[2].second(fit::ok());
}

#ifndef NINSPECT
TEST_F(ChannelManagerMockAclChannelTest, InspectHierarchy) {
  inspect::Inspector inspector;
  chanmgr()->AttachInspect(inspector.GetRoot(), "l2cap");

  chanmgr()->RegisterService(kSDP, kChannelParams, [](auto) {});
  auto services_matcher =
      AllOf(NodeMatches(NameMatches("services")),
            ChildrenMatch(ElementsAre(NodeMatches(AllOf(
                NameMatches("service_0x0"), PropertyList(ElementsAre(StringIs("psm", "SDP"))))))));

  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto conn_req_id = NextCommandId();
  const auto config_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundConnectionRequest(conn_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);
  Channel::WeakPtr dynamic_channel;
  auto channel_cb = [&dynamic_channel](l2cap::Channel::WeakPtr activated_chan) {
    dynamic_channel = std::move(activated_chan);
  };
  ActivateOutboundChannel(kTestPsm, kChannelParams, std::move(channel_cb), kTestHandle1, []() {});
  ReceiveAclDataPacket(InboundConnectionResponse(conn_req_id));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  auto signaling_chan_matcher =
      NodeMatches(AllOf(NameMatches("channel_0x2"),
                        PropertyList(UnorderedElementsAre(StringIs("local_id", "0x0001"),
                                                          StringIs("remote_id", "0x0001")))));
  auto dyn_chan_matcher = NodeMatches(AllOf(
      NameMatches("channel_0x3"),
      PropertyList(UnorderedElementsAre(StringIs("local_id", "0x0040"),
                                        StringIs("remote_id", "0x9042"), StringIs("psm", "SDP")))));
  auto channels_matcher =
      AllOf(NodeMatches(NameMatches("channels")),
            ChildrenMatch(UnorderedElementsAre(signaling_chan_matcher, dyn_chan_matcher)));
  auto link_matcher = AllOf(
      NodeMatches(NameMatches("logical_links")),
      ChildrenMatch(ElementsAre(AllOf(
          NodeMatches(AllOf(NameMatches("logical_link_0x1"),
                            PropertyList(UnorderedElementsAre(
                                StringIs("handle", "0x0001"), StringIs("link_type", "ACL"),
                                UintIs("flush_timeout_ms", zx::duration::infinite().to_msecs()))))),
          ChildrenMatch(ElementsAre(channels_matcher))))));

  auto l2cap_node_matcher =
      AllOf(NodeMatches(NameMatches("l2cap")),
            ChildrenMatch(UnorderedElementsAre(link_matcher, services_matcher)));

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy, ChildrenMatch(ElementsAre(l2cap_node_matcher)));

  // inspector must outlive ChannelManager
  chanmgr()->RemoveConnection(kTestHandle1);
}
#endif  // NINSPECT

TEST_F(ChannelManagerMockAclChannelTest,
       OutboundChannelWithFlushTimeoutInChannelParametersAndDelayedFlushTimeoutCallback) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  EXPECT_CMD_PACKET_OUT(test_device(),
                        WriteAutomaticFlushTimeoutPacket(kTestHandle1, kExpectedFlushTimeoutParam));

  ChannelParameters chan_params;
  chan_params.flush_timeout = kFlushTimeout;

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr activated_chan) {
    channel = std::move(activated_chan);
  };
  SetUpOutboundChannelWithCallback(kLocalId, kRemoteId, /*closed_cb=*/DoNothing, chan_params,
                                   channel_cb);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  // Channel should not be returned yet because setting flush timeout has not completed yet.
  EXPECT_FALSE(channel.is_alive());

  // Completing the command should cause the channel to be returned.
  const auto kCommandComplete = CommandCompletePacket(hci_spec::kWriteAutomaticFlushTimeout,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  test_device()->SendCommandChannelPacket(kCommandComplete);
  RunLoopUntilIdle();
  ASSERT_TRUE(channel.is_alive());
  ASSERT_TRUE(channel->info().flush_timeout.has_value());
  EXPECT_EQ(channel->info().flush_timeout.value(), kFlushTimeout);

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstFlushable, length: 6)
          0x01, 0x20, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,  // length: 2
          LowerBits(kRemoteId),
          UpperBits(kRemoteId),  // remote id
          'h', 'i'),             // payload
      kLowPriority);
  EXPECT_TRUE(channel->Send(NewBuffer('h', 'i')));
}

TEST_F(ChannelManagerMockAclChannelTest,
       OutboundChannelWithFlushTimeoutInChannelParametersFailure) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto kCommandCompleteError = CommandCompletePacket(
      hci_spec::kWriteAutomaticFlushTimeout, pw::bluetooth::emboss::StatusCode::UNSPECIFIED_ERROR);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        WriteAutomaticFlushTimeoutPacket(kTestHandle1, kExpectedFlushTimeoutParam),
                        &kCommandCompleteError);

  ChannelParameters chan_params;
  chan_params.flush_timeout = kFlushTimeout;

  auto channel = SetUpOutboundChannel(kLocalId, kRemoteId, /*closed_cb=*/DoNothing, chan_params);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  // Flush timeout should not be set in channel info because setting a flush timeout failed.
  EXPECT_FALSE(channel->info().flush_timeout.has_value());

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstNonFlushable, length: 6)
          0x01, 0x00, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,  // length: 2
          LowerBits(kRemoteId),
          UpperBits(kRemoteId),  // remote id
          'h', 'i'),             // payload
      kLowPriority);
  EXPECT_TRUE(channel->Send(NewBuffer('h', 'i')));
}

TEST_F(ChannelManagerMockAclChannelTest, InboundChannelWithFlushTimeoutInChannelParameters) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const auto kCommandComplete = CommandCompletePacket(hci_spec::kWriteAutomaticFlushTimeout,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        WriteAutomaticFlushTimeoutPacket(kTestHandle1, kExpectedFlushTimeoutParam),
                        &kCommandComplete);

  ChannelParameters chan_params;
  chan_params.flush_timeout = kFlushTimeout;

  Channel::WeakPtr channel;
  auto channel_cb = [&channel](l2cap::Channel::WeakPtr opened_chan) {
    channel = std::move(opened_chan);
    EXPECT_TRUE(channel->Activate(NopRxCallback, DoNothing));
  };

  EXPECT_TRUE(chanmgr()->RegisterService(kTestPsm, chan_params, std::move(channel_cb)));

  CommandId kPeerConnectionRequestId = 3;
  const auto config_req_id = NextCommandId();

  EXPECT_ACL_PACKET_OUT_(OutboundConnectionResponse(kPeerConnectionRequestId), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationRequest(config_req_id), kHighPriority);
  EXPECT_ACL_PACKET_OUT_(OutboundConfigurationResponse(kPeerConfigRequestId), kHighPriority);

  ReceiveAclDataPacket(InboundConnectionRequest(kPeerConnectionRequestId));
  ReceiveAclDataPacket(InboundConfigurationRequest(kPeerConfigRequestId));
  ReceiveAclDataPacket(InboundConfigurationResponse(config_req_id));

  RunLoopUntilIdle();
  EXPECT_TRUE(AllExpectedPacketsSent());
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(channel.is_alive());
  ASSERT_TRUE(channel->info().flush_timeout.has_value());
  EXPECT_EQ(channel->info().flush_timeout.value(), kFlushTimeout);

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstFlushable, length: 6)
          0x01, 0x20, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,  // length: 2
          LowerBits(kRemoteId),
          UpperBits(kRemoteId),  // remote id
          'h', 'i'),             // payload
      kLowPriority);
  EXPECT_TRUE(channel->Send(NewBuffer('h', 'i')));
}

TEST_F(ChannelManagerMockAclChannelTest, FlushableChannelAndNonFlushableChannelOnSameLink) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();
  auto nonflushable_channel = SetUpOutboundChannel();
  auto flushable_channel = SetUpOutboundChannel(kLocalId + 1, kRemoteId + 1);

  const auto kCommandComplete = CommandCompletePacket(hci_spec::kWriteAutomaticFlushTimeout,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        WriteAutomaticFlushTimeoutPacket(kTestHandle1, kExpectedFlushTimeoutParam),
                        &kCommandComplete);

  flushable_channel->SetBrEdrAutomaticFlushTimeout(
      kFlushTimeout, [](auto result) { EXPECT_EQ(fit::ok(), result); });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  EXPECT_FALSE(nonflushable_channel->info().flush_timeout.has_value());
  ASSERT_TRUE(flushable_channel->info().flush_timeout.has_value());
  EXPECT_EQ(flushable_channel->info().flush_timeout.value(), kFlushTimeout);

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstFlushable, length: 6)
          0x01, 0x20, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,  // length: 2
          LowerBits(flushable_channel->remote_id()),
          UpperBits(flushable_channel->remote_id()),  // remote id
          'h', 'i'),                                  // payload
      kLowPriority);
  EXPECT_TRUE(flushable_channel->Send(NewBuffer('h', 'i')));

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstNonFlushable, length: 6)
          0x01, 0x00, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,  // length: 2
          LowerBits(nonflushable_channel->remote_id()),
          UpperBits(nonflushable_channel->remote_id()),  // remote id
          'h', 'i'),                                     // payload
      kLowPriority);
  EXPECT_TRUE(nonflushable_channel->Send(NewBuffer('h', 'i')));
}

TEST_F(ChannelManagerMockAclChannelTest, SettingFlushTimeoutFails) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();
  auto channel = SetUpOutboundChannel();

  const auto kCommandComplete =
      CommandCompletePacket(hci_spec::kWriteAutomaticFlushTimeout,
                            pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        WriteAutomaticFlushTimeoutPacket(kTestHandle1, kExpectedFlushTimeoutParam),
                        &kCommandComplete);

  channel->SetBrEdrAutomaticFlushTimeout(kFlushTimeout, [](auto result) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::UNKNOWN_CONNECTION_ID), result);
  });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());

  EXPECT_ACL_PACKET_OUT_(
      StaticByteBuffer(
          // ACL data header (handle: 1, packet boundary flag: kFirstNonFlushable, length: 6)
          0x01, 0x00, 0x06, 0x00,
          // L2CAP B-frame
          0x02, 0x00,                                  // length: 2
          LowerBits(kRemoteId), UpperBits(kRemoteId),  // remote id
          'h', 'i'),                                   // payload
      kLowPriority);
  EXPECT_TRUE(channel->Send(NewBuffer('h', 'i')));
}

class StartA2dpOffloadTest : public ChannelManagerMockAclChannelTest,
                             public ::testing::WithParamInterface<hci_android::A2dpCodecType> {};

TEST_P(StartA2dpOffloadTest, StartA2dpOffloadSuccess) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  const hci_android::A2dpCodecType codec = GetParam();
  Channel::A2dpOffloadConfiguration config = BuildA2dpOffloadConfiguration(codec);

  Channel::WeakPtr channel = SetUpOutboundChannel();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, channel->link_handle(),
                                                channel->remote_id(), channel->max_tx_sdu_size()),
                        &command_complete);

  std::optional<hci::Result<>> result_;
  channel->StartA2dpOffload(&config, [&result_](auto result) { result_ = result; });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(result_.has_value());
  EXPECT_TRUE(result_->is_ok());
}

const std::vector<hci_android::A2dpCodecType> kA2dpCodecTypeParams = {
    hci_android::A2dpCodecType::kSbc, hci_android::A2dpCodecType::kAac,
    hci_android::A2dpCodecType::kLdac, hci_android::A2dpCodecType::kAptx};
INSTANTIATE_TEST_SUITE_P(ChannelManagerTest, StartA2dpOffloadTest,
                         ::testing::ValuesIn(kA2dpCodecTypeParams));

TEST_F(ChannelManagerMockAclChannelTest, StartA2dpOffloadInvalidConfiguration) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  Channel::A2dpOffloadConfiguration config = BuildA2dpOffloadConfiguration();
  Channel::WeakPtr channel = SetUpOutboundChannel();

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, channel->link_handle(),
                                                channel->remote_id(), channel->max_tx_sdu_size()),
                        &command_complete);

  std::optional<hci::Result<>> result_;
  channel->StartA2dpOffload(&config, [&result_](auto result) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS), result);
    result_ = result;
  });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(result_.has_value());
  EXPECT_TRUE(result_->is_error());
}

TEST_F(ChannelManagerMockAclChannelTest, StartA2dpOffloadAlreadyStarted) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  Channel::A2dpOffloadConfiguration config = BuildA2dpOffloadConfiguration();
  Channel::WeakPtr channel = SetUpOutboundChannel();

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, channel->link_handle(),
                                                channel->remote_id(), channel->max_tx_sdu_size()),
                        &command_complete);

  std::optional<hci::Result<>> result_;
  channel->StartA2dpOffload(&config, [&result_](auto result) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS), result);
    result_ = result;
  });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  EXPECT_TRUE(result_.has_value());
  EXPECT_TRUE(result_->is_error());
}

TEST_F(ChannelManagerMockAclChannelTest, StartA2dpOffloadStatusStarted) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  Channel::A2dpOffloadConfiguration config = BuildA2dpOffloadConfiguration();
  Channel::WeakPtr channel = SetUpOutboundChannel();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, channel->link_handle(),
                                                channel->remote_id(), channel->max_tx_sdu_size()),
                        &command_complete);

  std::optional<hci::Result<>> result_;
  channel->StartA2dpOffload(&config, [&result_](auto result) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), result);
    result_ = result;
  });
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  EXPECT_TRUE(result_->is_ok());

  Channel::A2dpOffloadConfiguration new_config = BuildA2dpOffloadConfiguration();

  channel->StartA2dpOffload(&new_config, [&result_](auto result) {
    EXPECT_EQ(ToResult(HostError::kInProgress), result);
    result_ = result;
  });
  RunLoopUntilIdle();
  ASSERT_TRUE(result_.has_value());
  EXPECT_TRUE(result_->is_error());
}

TEST_F(ChannelManagerMockAclChannelTest, StartA2dpOffloadChannelDisconnected) {
  QueueRegisterACL(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);
  RunLoopUntilIdle();

  Channel::A2dpOffloadConfiguration config = BuildA2dpOffloadConfiguration();
  Channel::WeakPtr channel = SetUpOutboundChannel();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, channel->link_handle(),
                                                channel->remote_id(), channel->max_tx_sdu_size()),
                        &command_complete);

  std::optional<hci::Result<>> result_;
  channel->StartA2dpOffload(&config, [&result_](auto result) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), result);
    result_ = result;
  });

  ASSERT_TRUE(channel.is_alive());
  const auto disconn_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT_(OutboundDisconnectionRequest(disconn_req_id), kHighPriority);
  channel->Deactivate();
  ASSERT_FALSE(channel.is_alive());

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  EXPECT_FALSE(result_.has_value());
}

TEST_F(ChannelManagerMockAclChannelTest, SignalLinkErrorStopsDeliveryOfBufferedRxPackets) {
  // LE-U link
  LEFixedChannels fixed_channels =
      RegisterLE(kTestHandle1, pw::bluetooth::emboss::ConnectionRole::CENTRAL);

  // Queue 2 packets to be delivers on channel activation.
  StaticByteBuffer payload_0(0x00);
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00,  // connection handle + flags
      0x05, 0x00,  // Length
      // L2CAP B-frame
      0x01, 0x00,  // Length
      LowerBits(kATTChannelId), UpperBits(kATTChannelId),
      // Payload
      payload_0[0]));
  ReceiveAclDataPacket(StaticByteBuffer(
      // ACL data header (starting fragment)
      0x01, 0x00,  // connection handle + flags
      0x05, 0x00,  // Length
      // L2CAP B-frame
      0x01, 0x00,  // Length
      LowerBits(kATTChannelId), UpperBits(kATTChannelId),
      // Payload
      0x01));
  RunLoopUntilIdle();

  bool closed_called = false;
  auto closed_cb = [&closed_called] { closed_called = true; };

  int rx_count = 0;
  auto rx_callback = [&](ByteBufferPtr payload) {
    rx_count++;
    if (rx_count == 1) {
      EXPECT_THAT(*payload, BufferEq(payload_0));
      // This should stop delivery of the second packet.
      fixed_channels.att->SignalLinkError();
      return;
    }
  };
  ASSERT_TRUE(fixed_channels.att->Activate(std::move(rx_callback), closed_cb));
  RunLoopUntilIdle();
  EXPECT_EQ(rx_count, 1);
  EXPECT_TRUE(closed_called);

  // Ensure the link is removed.
  chanmgr()->RemoveConnection(kTestHandle1);
  RunLoopUntilIdle();
}

TEST_F(ChannelManagerRealAclChannelTest, InboundRfcommChannelFailsWithPsmNotSupported) {
  constexpr l2cap::PSM kPSM = l2cap::kRFCOMM;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  RunLoopUntilIdle();

  const l2cap::CommandId kPeerConnReqId = 1;

  // Incoming connection refused, RFCOMM is not routed.
  EXPECT_ACL_PACKET_OUT(
      test_device(),
      l2cap::testing::AclConnectionRsp(kPeerConnReqId, kLinkHandle, kRemoteId, 0x0000 /*dest id*/,
                                       l2cap::ConnectionResult::kPSMNotSupported));

  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kPeerConnReqId, kLinkHandle, kRemoteId, kPSM));

  RunLoopUntilIdle();
}

TEST_F(ChannelManagerRealAclChannelTest, InboundPacketQueuedAfterChannelOpenIsNotDropped) {
  constexpr l2cap::PSM kPSM = l2cap::kSDP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  l2cap::Channel::WeakPtr chan;
  auto chan_cb = [&](auto cb_chan) {
    EXPECT_EQ(kLinkHandle, cb_chan->link_handle());
    chan = std::move(cb_chan);
  };

  chanmgr()->RegisterService(kPSM, kChannelParameters, std::move(chan_cb));
  RunLoopUntilIdle();

  constexpr l2cap::CommandId kConnectionReqId = 1;
  constexpr l2cap::CommandId kPeerConfigReqId = 6;
  const l2cap::CommandId kConfigReqId = NextCommandId();
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConnectionRsp(
                                           kConnectionReqId, kLinkHandle, kRemoteId, kLocalId));
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigReq(kConfigReqId, kLinkHandle,
                                                                    kRemoteId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kConnectionReqId, kLinkHandle, kRemoteId, kPSM));

  // Config negotiation will not complete yet.
  RunLoopUntilIdle();

  // Remaining config negotiation will be added to dispatch loop.
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigRsp(kPeerConfigReqId, kLinkHandle,
                                                                    kRemoteId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConfigReq(kPeerConfigReqId, kLinkHandle, kLocalId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConfigRsp(kConfigReqId, kLinkHandle, kLocalId, kChannelParameters));

  // Queue up a data packet for the new channel before the channel configuration has been
  // processed.
  ASSERT_FALSE(chan.is_alive());
  test_device()->SendACLDataChannelPacket(StaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id: 0x0040 (kLocalId))
      0x04, 0x00, 0x40, 0x00, 0xf0, 0x9f, 0x94, 0xb0));

  // Run until the channel opens and the packet is written to the socket buffer.
  RunLoopUntilIdle();
  ASSERT_TRUE(chan.is_alive());

  std::vector<ByteBufferPtr> rx_packets;
  auto rx_cb = [&rx_packets](ByteBufferPtr sdu) { rx_packets.push_back(std::move(sdu)); };
  ASSERT_TRUE(chan->Activate(rx_cb, DoNothing));
  RunLoopUntilIdle();
  ASSERT_EQ(rx_packets.size(), 1u);
  ASSERT_EQ(rx_packets[0]->size(), 4u);
  EXPECT_EQ("🔰", rx_packets[0]->view(0, 4u).AsString());
}

class AclPriorityTest : public ChannelManagerRealAclChannelTest,
                        public ::testing::WithParamInterface<std::pair<AclPriority, bool>> {};

TEST_P(AclPriorityTest, OutboundConnectAndSetPriority) {
  const auto kPriority = GetParam().first;
  const bool kExpectSuccess = GetParam().second;

  // Arbitrary command payload larger than CommandHeader.
  const auto op_code = hci_spec::VendorOpCode(0x01);
  const StaticByteBuffer kEncodedCommand(LowerBits(op_code), UpperBits(op_code),  // op code
                                         0x04,                                    // parameter size
                                         0x00, 0x01, 0x02, 0x03);                 // test parameter

  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  std::optional<hci_spec::ConnectionHandle> connection_handle_from_encode_cb;
  std::optional<AclPriority> priority_from_encode_cb;
  test_device()->set_encode_vendor_command_cb(
      [&](pw::bluetooth::VendorCommandParameters vendor_params,
          fit::callback<void(pw::Result<pw::span<const std::byte>>)> callback) {
        ASSERT_TRUE(
            std::holds_alternative<pw::bluetooth::SetAclPriorityCommandParameters>(vendor_params));
        auto& params = std::get<pw::bluetooth::SetAclPriorityCommandParameters>(vendor_params);
        connection_handle_from_encode_cb = params.connection_handle;
        priority_from_encode_cb = params.priority;
        callback(kEncodedCommand.subspan());
      });

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();

  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  l2cap::Channel::WeakPtr channel;
  auto chan_cb = [&](auto chan) { channel = std::move(chan); };

  QueueOutboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, std::move(chan_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  // We should have opened a channel successfully.
  ASSERT_TRUE(channel.is_alive());
  channel->Activate([](auto) {}, []() {});

  if (kPriority != AclPriority::kNormal) {
    auto cmd_complete = CommandCompletePacket(
        op_code, kExpectSuccess ? pw::bluetooth::emboss::StatusCode::SUCCESS
                                : pw::bluetooth::emboss::StatusCode::UNKNOWN_COMMAND);
    EXPECT_CMD_PACKET_OUT(test_device(), kEncodedCommand, &cmd_complete);
  }

  size_t request_cb_count = 0;
  channel->RequestAclPriority(kPriority, [&](auto result) {
    request_cb_count++;
    EXPECT_EQ(kExpectSuccess, result.is_ok());
  });

  RunLoopUntilIdle();
  EXPECT_EQ(request_cb_count, 1u);
  if (kPriority == AclPriority::kNormal) {
    EXPECT_FALSE(connection_handle_from_encode_cb);
  } else {
    ASSERT_TRUE(connection_handle_from_encode_cb);
    EXPECT_EQ(connection_handle_from_encode_cb.value(), kLinkHandle);
    ASSERT_TRUE(priority_from_encode_cb);
    EXPECT_EQ(priority_from_encode_cb.value(), kPriority);
  }
  connection_handle_from_encode_cb.reset();
  priority_from_encode_cb.reset();

  if (kPriority != AclPriority::kNormal && kExpectSuccess) {
    auto cmd_complete = CommandCompletePacket(op_code, pw::bluetooth::emboss::StatusCode::SUCCESS);
    EXPECT_CMD_PACKET_OUT(test_device(), kEncodedCommand, &cmd_complete);
  }

  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclDisconnectionReq(
                                           NextCommandId(), kLinkHandle, kLocalId, kRemoteId));

  // Deactivating channel should send priority command to revert priority back to normal if it was
  // changed.
  channel->Deactivate();
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  if (kPriority != AclPriority::kNormal && kExpectSuccess) {
    ASSERT_TRUE(connection_handle_from_encode_cb);
    EXPECT_EQ(connection_handle_from_encode_cb.value(), kLinkHandle);
    ASSERT_TRUE(priority_from_encode_cb);
    EXPECT_EQ(priority_from_encode_cb.value(), AclPriority::kNormal);
  } else {
    EXPECT_FALSE(connection_handle_from_encode_cb);
  }
}

const std::array<std::pair<AclPriority, bool>, 4> kPriorityParams = {
    {{AclPriority::kSource, false},
     {AclPriority::kSource, true},
     {AclPriority::kSink, true},
     {AclPriority::kNormal, true}}};
INSTANTIATE_TEST_SUITE_P(ChannelManagerMockControllerTest, AclPriorityTest,
                         ::testing::ValuesIn(kPriorityParams));

#ifndef NINSPECT
TEST_F(ChannelManagerRealAclChannelTest, InspectHierarchy) {
  inspect::Inspector inspector;
  chanmgr()->AttachInspect(inspector.GetRoot(), ChannelManager::kInspectNodeName);
  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy);
  auto l2cap_matcher =
      AllOf(NodeMatches(PropertyList(::testing::IsEmpty())),
            ChildrenMatch(UnorderedElementsAre(NodeMatches(NameMatches("logical_links")),
                                               NodeMatches(NameMatches("services")))));
  EXPECT_THAT(hierarchy.value(), AllOf(ChildrenMatch(UnorderedElementsAre(l2cap_matcher))));
}
#endif  // NINSPECT

TEST_F(ChannelManagerRealAclChannelTest, NegotiateChannelParametersOnOutboundL2capChannel) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;
  constexpr uint16_t kMtu = l2cap::kMinACLMTU;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = kMtu;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  l2cap::Channel::WeakPtr chan;
  auto chan_cb = [&](l2cap::Channel::WeakPtr cb_chan) { chan = std::move(cb_chan); };

  QueueOutboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, chan_cb, chan_params,
                               chan_params);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(chan.is_alive());
  EXPECT_EQ(kLinkHandle, chan->link_handle());
  EXPECT_EQ(*chan_params.max_rx_sdu_size, chan->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, chan->mode());
}

TEST_F(ChannelManagerRealAclChannelTest, NegotiateChannelParametersOnInboundChannel) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  l2cap::Channel::WeakPtr chan;
  auto chan_cb = [&](auto cb_chan) { chan = std::move(cb_chan); };
  chanmgr()->RegisterService(kPSM, chan_params, chan_cb);

  QueueInboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, chan_params, chan_params);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(chan.is_alive());
  EXPECT_EQ(*chan_params.max_rx_sdu_size, chan->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, chan->mode());
}

TEST_F(ChannelManagerRealAclChannelTest, RequestConnectionParameterUpdateAndReceiveResponse) {
  // Valid parameter values
  constexpr uint16_t kIntervalMin = 6;
  constexpr uint16_t kIntervalMax = 7;
  constexpr uint16_t kPeripheralLatency = 1;
  constexpr uint16_t kTimeoutMult = 10;
  const hci_spec::LEPreferredConnectionParameters kParams(kIntervalMin, kIntervalMax,
                                                          kPeripheralLatency, kTimeoutMult);

  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;
  QueueLEConnection(kLinkHandle, pw::bluetooth::emboss::ConnectionRole::PERIPHERAL);

  std::optional<bool> accepted;
  auto request_cb = [&accepted](bool cb_accepted) { accepted = cb_accepted; };

  // Receive "Accepted" Response:

  l2cap::CommandId param_update_req_id = NextCommandId();
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConnectionParameterUpdateReq(
                                           param_update_req_id, kLinkHandle, kIntervalMin,
                                           kIntervalMax, kPeripheralLatency, kTimeoutMult));
  chanmgr()->RequestConnectionParameterUpdate(kLinkHandle, kParams, request_cb);
  RunLoopUntilIdle();
  EXPECT_FALSE(accepted.has_value());

  test_device()->SendACLDataChannelPacket(l2cap::testing::AclConnectionParameterUpdateRsp(
      param_update_req_id, kLinkHandle, l2cap::ConnectionParameterUpdateResult::kAccepted));
  RunLoopUntilIdle();
  ASSERT_TRUE(accepted.has_value());
  EXPECT_TRUE(accepted.value());
  accepted.reset();
}

TEST_F(ChannelManagerRealAclChannelTest, AddLEConnectionReturnsFixedChannels) {
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;
  auto channels = QueueLEConnection(kLinkHandle, pw::bluetooth::emboss::ConnectionRole::PERIPHERAL);
  ASSERT_TRUE(channels.att.is_alive());
  EXPECT_EQ(l2cap::kATTChannelId, channels.att->id());
  ASSERT_TRUE(channels.smp.is_alive());
  EXPECT_EQ(l2cap::kLESMPChannelId, channels.smp->id());
}

// Queue dynamic channel packets, then open a new dynamic channel.
// The signaling channel packets should be sent before the queued dynamic channel packets.
TEST_F(ChannelManagerRealAclChannelTest, ChannelCreationPrioritizedOverDynamicChannelData) {
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  constexpr l2cap::PSM kPSM0 = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId0 = 0x0040;
  constexpr l2cap::ChannelId kRemoteId0 = 0x9042;

  constexpr l2cap::PSM kPSM1 = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId1 = 0x0041;
  constexpr l2cap::ChannelId kRemoteId1 = 0x9043;

  // l2cap connection request (or response), config request, config response
  constexpr size_t kChannelCreationPacketCount = 3;

  QueueAclConnection(kLinkHandle);

  l2cap::Channel::WeakPtr chan0;
  auto chan_cb0 = [&](auto cb_chan) {
    EXPECT_EQ(kLinkHandle, cb_chan->link_handle());
    chan0 = std::move(cb_chan);
  };
  chanmgr()->RegisterService(kPSM0, kChannelParameters, chan_cb0);

  QueueInboundL2capConnection(kLinkHandle, kPSM0, kLocalId0, kRemoteId0);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(chan0.is_alive());
  ASSERT_TRUE(chan0->Activate(NopRxCallback, DoNothing));

  test_device()->SendCommandChannelPacket(NumberOfCompletedPacketsPacket(
      kLinkHandle, kConnectionCreationPacketCount + kChannelCreationPacketCount));

  // Dummy dynamic channel packet
  const StaticByteBuffer kPacket0(
      // ACL data header (handle: 1, length 5)
      0x01, 0x00, 0x05, 0x00,

      // L2CAP B-frame: (length: 1, channel-id: 0x9042 (kRemoteId))
      0x01, 0x00, 0x42, 0x90,

      // L2CAP payload
      0x01);

  // |kMaxPacketCount| packets should be sent to the controller,
  // and 1 packet should be left in the queue
  const StaticByteBuffer write_data(0x01);
  for (size_t i = 0; i < kMaxPacketCount + 1; i++) {
    if (i != kMaxPacketCount) {
      EXPECT_ACL_PACKET_OUT(test_device(), kPacket0);
    }
    chan0->Send(std::make_unique<DynamicByteBuffer>(write_data));
  }

  EXPECT_FALSE(test_device()->AllExpectedDataPacketsSent());
  // Run until the data is flushed out to the MockController.
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  l2cap::Channel::WeakPtr chan1;
  auto chan_cb1 = [&](auto cb_chan) {
    EXPECT_EQ(kLinkHandle, cb_chan->link_handle());
    chan1 = std::move(cb_chan);
  };

  QueueOutboundL2capConnection(kLinkHandle, kPSM1, kLocalId1, kRemoteId1, std::move(chan_cb1));

  for (size_t i = 0; i < kChannelCreationPacketCount; i++) {
    test_device()->SendCommandChannelPacket(NumberOfCompletedPacketsPacket(kLinkHandle, 1));
    // Wait for next connection creation packet to be queued (eg. configuration request/response).
    RunLoopUntilIdle();
  }

  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  EXPECT_TRUE(chan1.is_alive());

  // Make room in buffer for queued dynamic channel packet.
  test_device()->SendCommandChannelPacket(NumberOfCompletedPacketsPacket(kLinkHandle, 1));

  EXPECT_ACL_PACKET_OUT(test_device(), kPacket0);
  RunLoopUntilIdle();
  // 1 Queued dynamic channel data packet should have been sent.
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
}

TEST_F(ChannelManagerRealAclChannelTest, OutboundChannelIsInvalidWhenL2capFailsToOpenChannel) {
  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  // Don't register any links. This should cause outbound channels to fail.
  bool chan_cb_called = false;
  auto chan_cb = [&chan_cb_called](auto chan) {
    chan_cb_called = true;
    EXPECT_FALSE(chan.is_alive());
  };

  chanmgr()->OpenL2capChannel(kLinkHandle, kPSM, kChannelParameters, std::move(chan_cb));

  RunLoopUntilIdle();

  EXPECT_TRUE(chan_cb_called);
}

}  // namespace
}  // namespace bt::l2cap
