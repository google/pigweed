// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_

#include <memory>
#include <mutex>
#include <unordered_map>

#include <lib/async/dispatcher.h>
#include <zircon/compiler.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/le_signaling_channel.h"

#include "src/lib/fxl/macros.h"
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

  ChannelManager(fxl::RefPtr<hci::Transport> hci, async_dispatcher_t* l2cap_dispatcher);
  ~ChannelManager();

  // Registers the ACL connection with the L2CAP layer. L2CAP channels can be
  // opened on the logical link represented by |handle| after a call to this
  // method.
  //
  // |link_error_callback| will be used to notify when a channel signals a link
  // error. It will be posted onto |dispatcher|.
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
                   LinkErrorCallback link_error_callback,
                   SecurityUpgradeCallback security_callback,
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
  using LEConnectionParameterUpdateCallback =
      internal::LESignalingChannel::ConnectionParameterUpdateCallback;
  void RegisterLE(hci::ConnectionHandle handle, hci::Connection::Role role,
                  LEConnectionParameterUpdateCallback conn_param_callback,
                  LinkErrorCallback link_error_callback,
                  SecurityUpgradeCallback security_callback,
                  async_dispatcher_t* dispatcher);

  // Removes a previously registered connection. All corresponding Channels will
  // be closed and all incoming data packets on this link will be dropped.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  void Unregister(hci::ConnectionHandle handle);

  // Assigns the security level of a logical link. Has no effect if |handle| has
  // not been previously registered or the link is closed.
  void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                    sm::SecurityProperties security);

  // Opens the L2CAP fixed channel with |channel_id| over the logical link
  // identified by |connection_handle| and starts routing packets. Returns
  // nullptr if the channel is already open.
  fbl::RefPtr<Channel> OpenFixedChannel(hci::ConnectionHandle connection_handle,
                                        ChannelId channel_id);

  // Open an out-bound connection-oriented L2CAP channel.
  void OpenChannel(hci::ConnectionHandle handle, PSM psm, ChannelCallback cb,
                   async_dispatcher_t* dispatcher);

  // Register/Unregister a callback for incoming service connections.
  bool RegisterService(PSM psm, ChannelCallback cb,
                       async_dispatcher_t* dispatcher);
  void UnregisterService(PSM psm);

 private:
  // Called when an ACL data packet is received from the controller. This method
  // is responsible for routing the packet to the corresponding LogicalLink.
  void OnACLDataReceived(hci::ACLDataPacketPtr data_packet);

  // Called by the various Register functions. Returns a pointer to the newly
  // added link.
  internal::LogicalLink* RegisterInternal(hci::ConnectionHandle handle,
                                          hci::Connection::LinkType ll_type,
                                          hci::Connection::Role role);

  // If a service (identified by |psm|) requested has been registered, return a
  // callback that passes an inbound channel to the registrant. The callback may
  // be called repeatedly to pass multiple channels for |psm|, but should not be
  // stored because the service may be unregistered at a later time. Calls for
  // unregistered services return an empty callback.
  ChannelCallback QueryService(hci::ConnectionHandle handle, PSM psm);

  fxl::RefPtr<hci::Transport> hci_;
  async_dispatcher_t* l2cap_dispatcher_;

  using LinkMap = std::unordered_map<hci::ConnectionHandle,
                                     fbl::RefPtr<internal::LogicalLink>>;
  LinkMap ll_map_;

  // Stores packets received on a connection handle before a link for it has
  // been created.
  using PendingPacketMap =
      std::unordered_map<hci::ConnectionHandle,
                         common::LinkedList<hci::ACLDataPacket>>;
  PendingPacketMap pending_packets_;

  // Store information required to create and forward channels for locally-
  // hosted services.
  //
  // TODO(NET-1240): Add desired configuration options
  using ServiceMap = std::unordered_map<PSM, ChannelCallback>;
  ServiceMap services_;

  fxl::ThreadChecker thread_checker_;
  fxl::WeakPtrFactory<ChannelManager> weak_ptr_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(ChannelManager);
};

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
