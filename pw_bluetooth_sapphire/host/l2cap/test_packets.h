#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt::l2cap::testing {

// Signaling Packets

DynamicByteBuffer AclCommandRejectNotUnderstoodRsp(l2cap::CommandId id,
                                                   hci::ConnectionHandle handle);

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
                               l2cap::ChannelId dst_id, uint16_t mtu = l2cap::kDefaultMTU,
                               l2cap::ChannelMode mode = l2cap::ChannelMode::kBasic);
DynamicByteBuffer AclConfigRsp(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                               l2cap::ChannelId src_id);
DynamicByteBuffer AclConnectionReq(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                                   l2cap::ChannelId src_id, l2cap::PSM psm);
DynamicByteBuffer AclConnectionRsp(l2cap::CommandId id, hci::ConnectionHandle link_handle,
                                   l2cap::ChannelId src_id, l2cap::ChannelId dst_id);

}  // namespace bt::l2cap::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TEST_PACKETS_H_
