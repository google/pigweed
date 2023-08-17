// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_MOCK_CONTROLLER_TEST_FIXTURE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_MOCK_CONTROLLER_TEST_FIXTURE_H_

#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::l2cap {

// ChannelManager test fixture that uses a real AclDataChannel and uses MockController
// for HCI packet expectations.
class ChannelManagerMockControllerTest
    : public bt::testing::ControllerTest<bt::testing::MockController> {
 public:
  static constexpr size_t kMaxDataPacketLength = 64;
  // High enough so that most tests don't need to worry about HCI flow control.
  static constexpr size_t kBufferMaxNumPackets = 10;

  static constexpr l2cap::ChannelParameters kChannelParameters{l2cap::ChannelMode::kBasic,
                                                               l2cap::kMaxMTU, std::nullopt};

  static constexpr l2cap::ExtendedFeatures kExtendedFeatures =
      l2cap::kExtendedFeaturesBitEnhancedRetransmission;

  static void DoNothing() {}
  static void NopRxCallback(ByteBufferPtr) {}

  ChannelManagerMockControllerTest() = default;
  ~ChannelManagerMockControllerTest() override = default;

 protected:
  void SetUp() override {
    bt::testing::ControllerTest<bt::testing::MockController>::SetUp();
    const auto bredr_buffer_info = hci::DataBufferInfo(kMaxDataPacketLength, kBufferMaxNumPackets);
    InitializeACLDataChannel(bredr_buffer_info);

    // TODO(63074): Remove assumptions about channel ordering so we can turn random ids on.
    channel_manager_ =
        ChannelManager::Create(transport()->acl_data_channel(), transport()->command_channel(),
                               /*random_channel_ids=*/false);

    next_command_id_ = 1;
  }

  void SetUp(size_t max_acl_payload_size, size_t max_le_payload_size,
             size_t max_acl_packets = kBufferMaxNumPackets,
             size_t max_le_packets = kBufferMaxNumPackets) {
    bt::testing::ControllerTest<bt::testing::MockController>::SetUp();

    InitializeACLDataChannel(hci::DataBufferInfo(max_acl_payload_size, max_acl_packets),
                             hci::DataBufferInfo(max_le_payload_size, max_le_packets));

    channel_manager_ =
        ChannelManager::Create(transport()->acl_data_channel(), transport()->command_channel(),
                               /*random_channel_ids=*/false);

    next_command_id_ = 1;
  }

  void TearDown() override {
    channel_manager_ = nullptr;
    bt::testing::ControllerTest<bt::testing::MockController>::TearDown();
  }

  l2cap::CommandId NextCommandId() { return next_command_id_++; }

  void QueueConfigNegotiation(hci_spec::ConnectionHandle handle,
                              l2cap::ChannelParameters local_params,
                              l2cap::ChannelParameters peer_params, l2cap::ChannelId local_cid,
                              l2cap::ChannelId remote_cid, l2cap::CommandId local_config_req_id,
                              l2cap::CommandId peer_config_req_id) {
    const auto kPeerConfigRsp =
        l2cap::testing::AclConfigRsp(local_config_req_id, handle, local_cid, local_params);
    const auto kPeerConfigReq =
        l2cap::testing::AclConfigReq(peer_config_req_id, handle, local_cid, peer_params);
    EXPECT_ACL_PACKET_OUT(
        test_device(),
        l2cap::testing::AclConfigReq(local_config_req_id, handle, remote_cid, local_params),
        &kPeerConfigRsp, &kPeerConfigReq);
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigRsp(peer_config_req_id, handle,
                                                                      remote_cid, peer_params));
  }

  void QueueInboundL2capConnection(hci_spec::ConnectionHandle handle, l2cap::PSM psm,
                                   l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid,
                                   l2cap::ChannelParameters local_params = kChannelParameters,
                                   l2cap::ChannelParameters peer_params = kChannelParameters) {
    const l2cap::CommandId kPeerConnReqId = 1;
    const l2cap::CommandId kPeerConfigReqId = kPeerConnReqId + 1;
    const auto kConfigReqId = NextCommandId();
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConnectionRsp(kPeerConnReqId, handle,
                                                                          remote_cid, local_cid));
    QueueConfigNegotiation(handle, local_params, peer_params, local_cid, remote_cid, kConfigReqId,
                           kPeerConfigReqId);

    test_device()->SendACLDataChannelPacket(
        l2cap::testing::AclConnectionReq(kPeerConnReqId, handle, remote_cid, psm));
  }

  void QueueOutboundL2capConnection(hci_spec::ConnectionHandle handle, l2cap::PSM psm,
                                    l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid,
                                    ChannelCallback open_cb,
                                    l2cap::ChannelParameters local_params = kChannelParameters,
                                    l2cap::ChannelParameters peer_params = kChannelParameters) {
    const l2cap::CommandId kPeerConfigReqId = 1;
    const auto kConnReqId = NextCommandId();
    const auto kConfigReqId = NextCommandId();
    const auto kConnRsp =
        l2cap::testing::AclConnectionRsp(kConnReqId, handle, local_cid, remote_cid);
    EXPECT_ACL_PACKET_OUT(test_device(),
                          l2cap::testing::AclConnectionReq(kConnReqId, handle, local_cid, psm),
                          &kConnRsp);
    QueueConfigNegotiation(handle, local_params, peer_params, local_cid, remote_cid, kConfigReqId,
                           kPeerConfigReqId);

    chanmgr()->OpenL2capChannel(handle, psm, local_params, std::move(open_cb));
  }

  struct QueueAclConnectionRetVal {
    l2cap::CommandId extended_features_id;
    l2cap::CommandId fixed_channels_supported_id;
    ChannelManager::BrEdrFixedChannels fixed_channels;
  };

  QueueAclConnectionRetVal QueueAclConnection(
      hci_spec::ConnectionHandle handle,
      pw::bluetooth::emboss::ConnectionRole role = pw::bluetooth::emboss::ConnectionRole::CENTRAL) {
    QueueAclConnectionRetVal return_val;
    return_val.extended_features_id = NextCommandId();
    return_val.fixed_channels_supported_id = NextCommandId();

    const auto kExtFeaturesRsp = l2cap::testing::AclExtFeaturesInfoRsp(
        return_val.extended_features_id, handle, kExtendedFeatures);
    EXPECT_ACL_PACKET_OUT(
        test_device(),
        l2cap::testing::AclExtFeaturesInfoReq(return_val.extended_features_id, handle),
        &kExtFeaturesRsp);
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclFixedChannelsSupportedInfoReq(
                                             return_val.fixed_channels_supported_id, handle));

    return_val.fixed_channels = chanmgr()->AddACLConnection(
        handle, role, /*link_error_callback=*/[]() {},
        /*security_callback=*/[](auto, auto, auto) {});

    return return_val;
  }

  ChannelManager::LEFixedChannels QueueLEConnection(hci_spec::ConnectionHandle handle,
                                                    pw::bluetooth::emboss::ConnectionRole role) {
    return chanmgr()->AddLEConnection(
        handle, role, /*link_error_callback=*/[] {}, /*conn_param_callback=*/[](auto&) {},
        /*security_callback=*/[](auto, auto, auto) {});
  }

  Channel::WeakPtr ActivateNewFixedChannel(ChannelId id,
                                           hci_spec::ConnectionHandle conn_handle = 0x0001,
                                           Channel::ClosedCallback closed_cb = DoNothing,
                                           Channel::RxCallback rx_cb = NopRxCallback) {
    auto chan = chanmgr()->OpenFixedChannel(conn_handle, id);
    if (!chan.is_alive() || !chan->Activate(std::move(rx_cb), std::move(closed_cb))) {
      return Channel::WeakPtr();
    }

    return chan;
  }

  ChannelManager* chanmgr() const { return channel_manager_.get(); }

 private:
  std::unique_ptr<ChannelManager> channel_manager_;
  l2cap::CommandId next_command_id_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(ChannelManagerMockControllerTest);
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_MOCK_CONTROLLER_TEST_FIXTURE_H_
