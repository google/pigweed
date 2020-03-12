// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <zircon/compiler.h>

#include <memory>
#include <mutex>
#include <unordered_map>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/le_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {

namespace hci {
class Transport;
}  // namespace hci

namespace l2cap {

namespace internal {
class LogicalLink;
}  // namespace internal

// ChannelManager implements the "Channel Manager" control block of L2CAP. In
// particular:
//
//   * It acts as a routing table for incoming ACL data by directing packets to
//     the appropriate internal logical link handler;
//
//   * Manages priority based scheduling.
//
//   * Provides an API surface for L2CAP channel creation and logical link
//     management bound to a single creation thread.
//
// There can be a single instance of ChannelManager for a HCI transport.
//
// THREAD-SAFETY:
//
// This object is not thread safe. Construction/destruction must happen on the
// thread where this is used.
class ChannelManager final {
 public:
  using LinkErrorCallback = fit::closure;

  // Used to schedule a series of packets on the link type |ll_type| to be transmitted to the
  // controller's ACL endpoint. All the packets in each invocation must be transmitted contiguously
  // and in order. This will be called on the thread which the ChannelManager object is created, up
  // to the object's duration.
  using SendAclCallback =
      fit::function<bool(LinkedList<hci::ACLDataPacket> packets, ChannelId channel_id,
                         hci::ACLDataChannel::PacketPriority priority)>;

  // Used to drop stale queued ACL data packets for which |predicate| returns true (eg. when a
  // channel is closed). Queued ACL data packets are those that were sent with |SendAclCallback| but
  // not have not yet been transmitted to the controller.
  using DropQueuedAclCallback = fit::function<void(hci::ACLPacketPredicate predicate)>;

  // Creates L2CAP state for logical links and channels.
  //
  // |max_acl_payload_size| and |max_le_payload_size| are the "maximum size[s] of HCI ACL (excluding
  // header) Data Packets... sent from the Host to the Controller" (Core v5.0 Vol 2, Part E, Section
  // 4.1) used for fragmenting outbound data. Data that is fragmented will be passed contiguously as
  // invocations of |send_packets_cb|.
  //
  // State changes are processed on |l2cap_dispatcher|.
  ChannelManager(size_t max_acl_payload_size, size_t max_le_payload_size,
                 SendAclCallback send_acl_cb, DropQueuedAclCallback filter_acl_cb,
                 async_dispatcher_t* l2cap_dispatcher);
  ~ChannelManager();

  // Returns a handler for data packets received from the Bluetooth controller bound to this object.
  // It must be called from the creation thread, but it is safe to call past ChannelManager's
  // lifetime.
  hci::ACLPacketHandler MakeInboundDataHandler();

  // Registers the ACL connection with the L2CAP layer. L2CAP channels can be
  // opened on the logical link represented by |handle| after a call to this
  // method.
  //
  // |link_error_callback| will be used to notify when a channel signals a link
  // error.
  //
  // |security_callback| will be used to request an upgrade to the link security
  // level. This can be triggered by dynamic L2CAP channel creation or by a
  // service-level client via Channel::UpgradeSecurity().
  //
  // All callbacks will be posted onto |dispatcher|.
  //
  // It is an error to register the same |handle| value more than once as either
  // kind of channel without first unregistering it (asserted in debug builds).
  void RegisterACL(hci::ConnectionHandle handle, hci::Connection::Role role,
                   LinkErrorCallback link_error_callback, SecurityUpgradeCallback security_callback,
                   async_dispatcher_t* dispatcher);

  // Registers a LE connection with the L2CAP layer. L2CAP channels can be
  // opened on the logical link represented by |handle| after a call to this
  // method.
  //
  // |conn_param_callback| will be used to notify the caller if new connection
  // parameters were accepted from the remote end of the link.
  //
  // |link_error_callback| will be used to notify when a channel signals a link
  // error.
  //
  // |security_callback| will be used to request an upgrade to the link security
  // level. This can be triggered by dynamic L2CAP channel creation or by a
  // service-level client via Channel::UpgradeSecurity().
  //
  // All callbacks will be posted onto |dispatcher|.
  //
  // It is an error to register the same |handle| value more than once as either
  // kind of channel without first unregistering it (asserted in debug builds).
  void RegisterLE(hci::ConnectionHandle handle, hci::Connection::Role role,
                  LEConnectionParameterUpdateCallback conn_param_callback,
                  LinkErrorCallback link_error_callback, SecurityUpgradeCallback security_callback,
                  async_dispatcher_t* dispatcher);

  // Removes a connection. All incoming data packets on this link will be dropped. If the
  // connection was previously registered, all corresponding Channels will be closed.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  void Unregister(hci::ConnectionHandle handle);

  // Assigns the security level of a logical link. Has no effect if |handle| has
  // not been previously registered or the link is closed.
  void AssignLinkSecurityProperties(hci::ConnectionHandle handle, sm::SecurityProperties security);

  // Opens the L2CAP fixed channel with |channel_id| over the logical link
  // identified by |connection_handle| and starts routing packets. Returns
  // nullptr if the channel is already open.
  fbl::RefPtr<Channel> OpenFixedChannel(hci::ConnectionHandle connection_handle,
                                        ChannelId channel_id);

  // Opens an out-bound connection-oriented L2CAP channel on the link specified by |handle| to the
  // requested |psm| with the preferred parameters |params|.
  // Returns a channel asynchronously via |callback| (posted on the given |dispatcher|).
  void OpenChannel(hci::ConnectionHandle handle, PSM psm, ChannelParameters params,
                   ChannelCallback cb, async_dispatcher_t* dispatcher);

  // Register/Unregister a callback for incoming service connections.
  // Incoming channels will be configured using using the preferred parameters |params|.
  bool RegisterService(PSM psm, ChannelParameters params, ChannelCallback cb,
                       async_dispatcher_t* dispatcher);
  void UnregisterService(PSM psm);

  // Information stored and returned for registered services that is needed to configure and forward
  // new channels for this service.
  using ServiceInfo = ServiceInfo<ChannelCallback>;

  // Returns mapping of ChannelId -> PacketPriority for use with ACLDataChannel::SendPacket.
  static hci::ACLDataChannel::PacketPriority ChannelPriority(ChannelId id);

  // Send a Connection Parameter Update Request on the LE signaling channel. When the Connection
  // Parameter Update Response is received, |request_cb| will be called on |dispatcher| and passed
  // the result, |accepted|.
  // NOTE: The local Host must be an LE slave, and this request should only be sent if the slave or
  // host does not support the Connection Parameters Request Link Layer Control Procedure (Core Spec
  // v5.2, Vol 3, Part A, Sec 4.20).
  void RequestConnectionParameterUpdate(hci::ConnectionHandle handle,
                                        hci::LEPreferredConnectionParameters params,
                                        ConnectionParameterUpdateRequestCallback request_cb,
                                        async_dispatcher_t* dispatcher);

 private:
  // Called when an ACL data packet is received from the controller. This method
  // is responsible for routing the packet to the corresponding LogicalLink.
  void OnACLDataReceived(hci::ACLDataPacketPtr data_packet);

  // Called by the various Register functions. Returns a pointer to the newly
  // added link.
  internal::LogicalLink* RegisterInternal(hci::ConnectionHandle handle,
                                          hci::Connection::LinkType ll_type,
                                          hci::Connection::Role role, size_t max_payload_size);

  // If a service (identified by |psm|) requested has been registered, return a ServiceInfo object
  // containing preferred channel parameters and a callback that passes an inbound channel to the
  // registrant. The callback may be called repeatedly to pass multiple channels for |psm|, but
  // should not be stored because the service may be unregistered at a later time. Calls for
  // unregistered services return null.
  std::optional<ServiceInfo> QueryService(hci::ConnectionHandle handle, PSM psm);

  // Maximum sizes for data packet payloads from host to controller.
  const size_t max_acl_payload_size_;
  const size_t max_le_payload_size_;

  // Queues data packets to be delivered to the controller for a given link type.
  SendAclCallback send_acl_cb_;

  // Drops data packets pending delivery to the controller.
  DropQueuedAclCallback drop_queued_acl_cb_;

  async_dispatcher_t* l2cap_dispatcher_;

  using LinkMap = std::unordered_map<hci::ConnectionHandle, fbl::RefPtr<internal::LogicalLink>>;
  LinkMap ll_map_;

  // Stores packets received on a connection handle before a link for it has
  // been created.
  using PendingPacketMap =
      std::unordered_map<hci::ConnectionHandle, LinkedList<hci::ACLDataPacket>>;
  PendingPacketMap pending_packets_;

  // Store information required to create and forward channels for locally-
  // hosted services.
  using ServiceMap = std::unordered_map<PSM, ServiceInfo>;
  ServiceMap services_;

  fxl::ThreadChecker thread_checker_;
  fxl::WeakPtrFactory<ChannelManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelManager);
};

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
