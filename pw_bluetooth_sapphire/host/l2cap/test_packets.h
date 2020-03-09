#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"

namespace bt::l2cap::testing {

// Signaling Packets

DynamicByteBuffer AclCommandRejectNotUnderstoodRsp(l2cap::CommandId id,
                                                   hci::ConnectionHandle handle);
DynamicByteBuffer AclExtFeaturesInfoReq(l2cap::CommandId id, hci::ConnectionHandle handle);
DynamicByteBuffer AclExtFeaturesInfoRsp(l2cap::CommandId id, hci::ConnectionHandle handle,
                                        l2cap::ExtendedFeatures features);
DynamicByteBuffer AclFixedChannelsSupportedInfoReq(l2cap::CommandId id,
                                                   hci::ConnectionHandle handle);
DynamicByteBuffer AclFixedChannelsSupportedInfoRsp(l2cap::CommandId id,
                                                   hci::ConnectionHandle handle,
                                                   l2cap::FixedChannelsSupported chan_mask);
DynamicByteBuffer AclNotSupportedInformationResponse(l2cap::CommandId id,
                                                     hci::ConnectionHandle handle);
DynamicByteBuffer AclConfigReq(l2cap::CommandId id, hci::ConnectionHandle handle,
                               l2cap::ChannelId dst_id, l2cap::ChannelParameters params);
DynamicByteBuffer AclConfigRsp(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                               l2cap::ChannelId src_id, l2cap::ChannelParameters params);
DynamicByteBuffer AclConnectionReq(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                                   l2cap::ChannelId src_id, l2cap::PSM psm);
DynamicByteBuffer AclConnectionRsp(
    l2cap::CommandId id, hci::ConnectionHandle link_handle, l2cap::ChannelId src_id,
    l2cap::ChannelId dst_id, l2cap::ConnectionResult result = l2cap::ConnectionResult::kSuccess);
DynamicByteBuffer AclDisconnectionReq(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                                      l2cap::ChannelId src_id, l2cap::ChannelId dst_id);

// S-Frame Packets

DynamicByteBuffer AclSFrameReceiverReady(hci::ConnectionHandle link_handle,
                                         l2cap::ChannelId channel_id, uint8_t receive_seq_num,
                                         bool is_poll_request, bool is_poll_response);

}  // namespace bt::l2cap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
