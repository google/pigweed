// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_

#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>
#include <zircon/compiler.h>

#include <list>
#include <map>
#include <queue>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/data_buffer_info.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/hci_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"

namespace bt::hci {

// Our ACL implementation allows specifying a Unique ChannelId for purposes of grouping packets so
// they can be dropped together when necessary. In practice, this channel id will always be equal
// to a given L2CAP ChannelId, as specified in the l2cap library
using UniqueChannelId = uint16_t;

class Transport;
// Represents the Bluetooth ACL Data channel and manages the Host<->Controller
// ACL data flow control.
//
// This currently only supports the Packet-based Data Flow Control as defined in
// Core Spec v5.0, Vol 2, Part E, Section 4.1.1.
class AclDataChannel {
 public:
  enum class PacketPriority { kHigh, kLow };

  using AclPacketPredicate =
      fit::function<bool(const ACLDataPacketPtr& packet, UniqueChannelId channel_id)>;

  static constexpr size_t kMaxAclPacketsPerChannel = 32;

  // Starts listening on the HCI ACL data channel and starts handling data flow
  // control. |bredr_buffer_info| represents the controller's data buffering
  // capacity for the BR/EDR transport and the |le_buffer_info| represents Low
  // Energy buffers. At least one of these (BR/EDR vs LE) must contain non-zero
  // values per Core Spec v5.0 Vol 2, Part E, Sec 4.1.1:
  //
  //   - A LE only controller will have LE buffers only.
  //   - A BR/EDR-only controller will have BR/EDR buffers only.
  //   - A dual-mode controller will have BR/EDR buffers and MAY have LE buffers
  //     if the BR/EDR buffer is not shared between the transports.
  //
  // As this class is intended to support flow-control for both, this function
  // should be called based on what is reported by the controller.
  static std::unique_ptr<AclDataChannel> Create(Transport* transport, HciWrapper* hci,
                                                const DataBufferInfo& bredr_buffer_info,
                                                const DataBufferInfo& le_buffer_info);

  virtual ~AclDataChannel() = default;

  // Attach inspect node as a child node of |parent|.
  static constexpr const char* const kInspectNodeName = "acl_data_channel";
  virtual void AttachInspect(inspect::Node& parent, const std::string& name) = 0;

  // Assigns a handler callback for received ACL data packets. |rx_callback| will shall take
  // ownership of each packet received from the controller.
  virtual void SetDataRxHandler(ACLPacketHandler rx_callback) = 0;

  // Queues the given ACL data packet to be sent to the controller. Returns
  // false if the packet cannot be queued up, e.g. if the size of |data_packet|
  // exceeds the MTU for the link type set in RegisterLink().
  //
  // |data_packet| is passed by value, meaning that ACLDataChannel will take
  // ownership of it. |data_packet| must represent a valid ACL data packet.
  //
  // |channel_id| must match the l2cap channel that the packet is being sent to. It is needed to
  // determine what channel l2cap packet fragments are being sent to when revoking queued packets
  // for specific channels that have closed. If the packet does not contain a fragment of an l2cap
  // packet, |channel_id| should be set to |l2cap::kInvalidChannelId|.
  //
  // |priority| indicates the order this packet should be dispatched off of the queue relative to
  // packets of other priorities. Note that high priority packets may still wait behind low
  // priority packets that have already been sent to the controller.
  virtual bool SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                          PacketPriority priority) = 0;

  // Queues the given list of ACL data packets to be sent to the controller. The
  // behavior is identical to that of SendPacket() with the guarantee that all
  // packets that are in |packets| are queued atomically. The contents of |packets| must comprise
  // one or more complete PDUs for the same handle in order, due to queue management assumptions.
  // If any packet's handle is not registered in the allowlist, then none will be queued.
  //
  // Takes ownership of the contents of |packets|. Returns false if |packets|
  // contains an element that exceeds the MTU for its link type or |packets| is empty.
  //
  // |channel_id| must match the l2cap channel that all packets is being sent to. It is needed to
  // determine what channel l2cap packet fragments are being sent to when revoking queued packets
  // for channels that have closed. If the packets do not contain a fragment of an l2cap
  // packet, |channel_id| should be set to |l2cap::kInvalidChannelId|.
  //
  // |priority| indicates the order this packet should be dispatched off of the queue relative to
  // packets of other priorities. Note that high priority packets may still wait behind low priority
  // packets that have already been sent to the controller.
  virtual bool SendPackets(std::list<ACLDataPacketPtr> packets, UniqueChannelId channel_id,
                           PacketPriority priority) = 0;

  // Allowlist packets destined for the link identified by |handle| (of link type |ll_type|) for
  // submission to the controller.
  //
  // Failure to register a link before sending packets will result in the packets
  // being dropped immediately. A handle must not be registered again until after UnregisterLink
  // has been called on that handle.
  virtual void RegisterLink(hci_spec::ConnectionHandle handle, bt::LinkType ll_type) = 0;

  // Cleans up all outgoing data buffering state related to the logical link
  // with the given |handle|. This must be called upon disconnection of a link
  // to ensure that stale outbound packets are filtered out of the send queue.
  // All future packets sent to this link will be dropped.
  //
  // |RegisterLink| must be called before |UnregisterLink| for the same handle.
  //
  // |UnregisterLink| does not clear the controller packet count, so |ClearControllerPacketCount|
  // must be called after |UnregisterLink| and the HCI Disconnection Complete event has been
  // received.
  virtual void UnregisterLink(hci_spec::ConnectionHandle handle) = 0;

  // Removes all queued data packets for which |predicate| returns true.
  virtual void DropQueuedPackets(AclPacketPredicate predicate) = 0;

  // Resets controller packet count for |handle| so that controller buffer credits can be reused.
  // This must be called on the HCI_Disconnection_Complete event to notify ACLDataChannel that
  // packets in the controller's buffer for |handle| have been flushed. See Core Spec v5.1, Vol 2,
  // Part E, Section 4.3. This must be called after |UnregisterLink|.
  virtual void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) = 0;

  // Returns the BR/EDR buffer information that the channel was initialized
  // with.
  virtual const DataBufferInfo& GetBufferInfo() const = 0;

  // Returns the LE buffer information that the channel was initialized with.
  // This defaults to the BR/EDR buffers if the controller does not have a
  // dedicated LE buffer.
  virtual const DataBufferInfo& GetLeBufferInfo() const = 0;

  // Attempts to set the ACL |priority| of the connection indicated by |handle|. |callback| will be
  // called with the result of the request.
  virtual void RequestAclPriority(hci::AclPriority priority, hci_spec::ConnectionHandle handle,
                                  fit::callback<void(fitx::result<fitx::failed>)> callback) = 0;

  // Sets an automatic flush timeout with duration |flush_timeout| for the connection indicated by
  // |handle|. |callback| will be called with the result of the operation.
  // |handle| must correspond to a BR/EDR connection.
  // |flush_timeout| must be in the range [1ms - hci_spec::kMaxAutomaticFlushTimeoutDuration]. A
  // flush timeout of zx::duration::infinite() indicates an infinite flush timeout (no automatic
  // flush), the default. If an invalid value of |flush_timeout| is specified, an error will be
  // returned to |callback|.
  virtual void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                             hci_spec::ConnectionHandle handle,
                                             ResultCallback<> callback) = 0;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_
