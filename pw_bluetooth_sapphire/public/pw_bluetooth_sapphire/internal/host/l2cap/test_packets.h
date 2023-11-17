// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/frame_headers.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/types.h"

namespace bt::l2cap::testing {

// Signaling Packets

DynamicByteBuffer AclCommandRejectNotUnderstoodRsp(
    l2cap::CommandId id,
    hci_spec::ConnectionHandle handle,
    ChannelId chan_id = kSignalingChannelId);
DynamicByteBuffer AclExtFeaturesInfoReq(l2cap::CommandId id,
                                        hci_spec::ConnectionHandle handle);
DynamicByteBuffer AclExtFeaturesInfoRsp(l2cap::CommandId id,
                                        hci_spec::ConnectionHandle handle,
                                        l2cap::ExtendedFeatures features);
DynamicByteBuffer AclFixedChannelsSupportedInfoReq(
    l2cap::CommandId id, hci_spec::ConnectionHandle handle);
DynamicByteBuffer AclFixedChannelsSupportedInfoRsp(
    l2cap::CommandId id,
    hci_spec::ConnectionHandle handle,
    l2cap::FixedChannelsSupported chan_mask);
DynamicByteBuffer AclNotSupportedInformationResponse(
    l2cap::CommandId id, hci_spec::ConnectionHandle handle);
DynamicByteBuffer AclConfigReq(l2cap::CommandId id,
                               hci_spec::ConnectionHandle handle,
                               l2cap::ChannelId dst_id,
                               l2cap::ChannelParameters params);
DynamicByteBuffer AclConfigRsp(l2cap::CommandId id,
                               hci_spec::ConnectionHandle link_handle,
                               l2cap::ChannelId src_id,
                               l2cap::ChannelParameters params);
DynamicByteBuffer AclConnectionReq(l2cap::CommandId id,
                                   hci_spec::ConnectionHandle link_handle,
                                   l2cap::ChannelId src_id,
                                   l2cap::Psm psm);
DynamicByteBuffer AclConnectionRsp(
    l2cap::CommandId id,
    hci_spec::ConnectionHandle link_handle,
    l2cap::ChannelId src_id,
    l2cap::ChannelId dst_id,
    ConnectionResult result = ConnectionResult::kSuccess);
DynamicByteBuffer AclDisconnectionReq(l2cap::CommandId id,
                                      hci_spec::ConnectionHandle link_handle,
                                      l2cap::ChannelId src_id,
                                      l2cap::ChannelId dst_id);
DynamicByteBuffer AclDisconnectionRsp(l2cap::CommandId id,
                                      hci_spec::ConnectionHandle link_handle,
                                      l2cap::ChannelId src_id,
                                      l2cap::ChannelId dst_id);
DynamicByteBuffer AclConnectionParameterUpdateReq(
    l2cap::CommandId id,
    hci_spec::ConnectionHandle link_handle,
    uint16_t interval_min,
    uint16_t interval_max,
    uint16_t peripheral_latency,
    uint16_t timeout_multiplier);
DynamicByteBuffer AclConnectionParameterUpdateRsp(
    l2cap::CommandId id,
    hci_spec::ConnectionHandle link_handle,
    ConnectionParameterUpdateResult result);

// S-Frame Packets

DynamicByteBuffer AclSFrame(hci_spec::ConnectionHandle link_handle,
                            l2cap::ChannelId channel_id,
                            internal::SupervisoryFunction function,
                            uint8_t receive_seq_num,
                            bool is_poll_request,
                            bool is_poll_response);

inline DynamicByteBuffer AclSFrameReceiverReady(
    hci_spec::ConnectionHandle link_handle,
    l2cap::ChannelId channel_id,
    uint8_t receive_seq_num,
    bool is_poll_request,
    bool is_poll_response) {
  return AclSFrame(link_handle,
                   channel_id,
                   internal::SupervisoryFunction::ReceiverReady,
                   receive_seq_num,
                   is_poll_request,
                   is_poll_response);
}

inline DynamicByteBuffer AclSFrameReceiverNotReady(
    hci_spec::ConnectionHandle link_handle,
    l2cap::ChannelId channel_id,
    uint8_t receive_seq_num,
    bool is_poll_request,
    bool is_poll_response) {
  return AclSFrame(link_handle,
                   channel_id,
                   internal::SupervisoryFunction::ReceiverNotReady,
                   receive_seq_num,
                   is_poll_request,
                   is_poll_response);
}

// I-Frame Packets

DynamicByteBuffer AclIFrame(hci_spec::ConnectionHandle link_handle,
                            l2cap::ChannelId channel_id,
                            uint8_t receive_seq_num,
                            uint8_t tx_seq,
                            bool is_poll_response,
                            const ByteBuffer& payload);

}  // namespace bt::l2cap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
