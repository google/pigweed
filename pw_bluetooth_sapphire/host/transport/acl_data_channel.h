// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_

#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>

#include <list>
#include <map>
#include <queue>
#include <unordered_map>

#include "pw_bluetooth/controller.h"
#include "pw_bluetooth/vendor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/data_buffer_info.h"
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
  // This interface will be implemented by l2cap::LogicalLink
  class ConnectionInterface {
   public:
    virtual ~ConnectionInterface() = default;

    virtual hci_spec::ConnectionHandle handle() const = 0;

    virtual bt::LinkType type() const = 0;

    // Returns the next PDU fragment, or nullptr if none is available.
    virtual std::unique_ptr<ACLDataPacket> GetNextOutboundPacket() = 0;

    // Returns true if link has a queued packet
    virtual bool HasAvailablePacket() const = 0;
  };

  // Registers a connection. Failure to register a connection before sending packets will result in
  // the packets being dropped immediately. A connection must not be registered again until after
  // |UnregisterConnection| has been called on that connection.
  virtual void RegisterConnection(WeakPtr<ConnectionInterface> connection) = 0;

  // Unregister a connection when it is disconnected. Cleans up all outgoing data buffering state
  // related to the logical link with the given |handle|. This must be called upon disconnection of
  // a link to ensure that stale outbound packets are filtered out of the send queue. All future
  // packets sent to this link will be dropped.
  //
  // |RegisterConnection| must be called before |UnregisterConnection| for the same handle.
  //
  // |UnregisterConnection| does not clear the controller packet count, so
  // |ClearControllerPacketCount| must be called after |UnregisterConnection| and the
  // HCI_Disconnection_Complete event has been received.
  virtual void UnregisterConnection(hci_spec::ConnectionHandle handle) = 0;

  // Called by LogicalLink when a packet is available
  virtual void OnOutboundPacketAvailable() = 0;

  enum class PacketPriority { kHigh, kLow };

  using AclPacketPredicate =
      fit::function<bool(const ACLDataPacketPtr& packet, UniqueChannelId channel_id)>;

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
  static std::unique_ptr<AclDataChannel> Create(Transport* transport,
                                                pw::bluetooth::Controller* hci,
                                                const DataBufferInfo& bredr_buffer_info,
                                                const DataBufferInfo& le_buffer_info);

  virtual ~AclDataChannel() = default;

  // Attach inspect node as a child node of |parent|.
  static constexpr const char* const kInspectNodeName = "acl_data_channel";
  virtual void AttachInspect(inspect::Node& parent, const std::string& name) = 0;

  // Assigns a handler callback for received ACL data packets. |rx_callback| will shall take
  // ownership of each packet received from the controller.
  virtual void SetDataRxHandler(ACLPacketHandler rx_callback) = 0;

  // Resets controller packet count for |handle| so that controller buffer credits can be reused.
  // This must be called on the HCI_Disconnection_Complete event to notify ACLDataChannel that
  // packets in the controller's buffer for |handle| have been flushed. See Core Spec v5.1, Vol 2,
  // Part E, Section 4.3. This must be called after |UnregisterConnection|.
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
  virtual void RequestAclPriority(pw::bluetooth::AclPriority priority,
                                  hci_spec::ConnectionHandle handle,
                                  fit::callback<void(fit::result<fit::failed>)> callback) = 0;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_ACL_DATA_CHANNEL_H_
