// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/thread_checker.h>
#include <lib/sys/inspect/cpp/component.h>
#include <lib/trace/event.h>
#include <zircon/compiler.h>

#include <memory>
#include <mutex>
#include <unordered_map>

#include <fbl/macros.h>

#include "lib/async/cpp/executor.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/le_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"

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

  // Creates L2CAP state for logical links and channels.
  //
  // |max_acl_payload_size| and |max_le_payload_size| are the "maximum size[s] of HCI ACL
  // (excluding header) Data Packets... sent from the Host to the Controller" (Core v5.0 Vol 2,
  // Part E, Section 4.1) used for fragmenting outbound data. Data that is fragmented will be
  // passed contiguously as invocations of |acl_data_channel->SendPackets()|.
  ChannelManager(size_t max_acl_payload_size, size_t max_le_payload_size,
                 hci::AclDataChannel* acl_data_channel, bool random_channel_ids);
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
  // It is an error to register the same |handle| value more than once as either
  // kind of channel without first unregistering it (asserted in debug builds).
  void RegisterACL(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                   LinkErrorCallback link_error_callback,
                   SecurityUpgradeCallback security_callback);

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
  // It is an error to register the same |handle| value more than once as either
  // kind of channel without first unregistering it (asserted in debug builds).
  void RegisterLE(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                  LEConnectionParameterUpdateCallback conn_param_callback,
                  LinkErrorCallback link_error_callback, SecurityUpgradeCallback security_callback);

  // Removes a connection. All incoming data packets on this link will be dropped. If the
  // connection was previously registered, all corresponding Channels will be closed.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  void Unregister(hci_spec::ConnectionHandle handle);

  // Assigns the security level of a logical link. Has no effect if |handle| has
  // not been previously registered or the link is closed.
  void AssignLinkSecurityProperties(hci_spec::ConnectionHandle handle,
                                    sm::SecurityProperties security);

  // Opens the L2CAP fixed channel with |channel_id| over the logical link
  // identified by |connection_handle| and starts routing packets. Returns
  // nullptr if the channel is already open.
  fbl::RefPtr<Channel> OpenFixedChannel(hci_spec::ConnectionHandle connection_handle,
                                        ChannelId channel_id);

  // Opens an out-bound connection-oriented L2CAP channel on the link specified by |handle| to the
  // requested |psm| with the preferred parameters |params|.
  // Returns a channel asynchronously via |callback|.
  void OpenChannel(hci_spec::ConnectionHandle handle, PSM psm, ChannelParameters params,
                   ChannelCallback cb);

  // Register/Unregister a callback for incoming service connections.
  // Incoming channels will be configured using using the preferred parameters |params|.
  bool RegisterService(PSM psm, ChannelParameters params, ChannelCallback cb);
  void UnregisterService(PSM psm);

  // Information stored and returned for registered services that is needed to configure and forward
  // new channels for this service.
  using ServiceInfo = ServiceInfo<ChannelCallback>;

  // Send a Connection Parameter Update Request on the LE signaling channel. When the Connection
  // Parameter Update Response is received, |request_cb| will be called with the result, |accepted|.
  //
  // NOTE: The local Host must be an LE peripheral, and this request should only be sent if the
  // peripheral or host does not support the Connection Parameters Request Link Layer Control
  // Procedure (Core Spec v5.2, Vol 3, Part A, Sec 4.20).
  void RequestConnectionParameterUpdate(hci_spec::ConnectionHandle handle,
                                        hci_spec::LEPreferredConnectionParameters params,
                                        ConnectionParameterUpdateRequestCallback request_cb);

  // Attach ChannelManager's inspect nodes as children of |parent|.
  void AttachInspect(inspect::Node& parent);

  // Returns a pointer to the internal LogicalLink with the corresponding link |handle|, or nullptr
  // if none exists.
  // NOTE: This is intended ONLY for unit tests. Clients should use the other public methods to
  // interact with the link.
  fxl::WeakPtr<internal::LogicalLink> LogicalLinkForTesting(hci_spec::ConnectionHandle handle);

 private:
  // Called when an ACL data packet is received from the controller. This method
  // is responsible for routing the packet to the corresponding LogicalLink.
  void OnACLDataReceived(hci::ACLDataPacketPtr data_packet);

  // Called by the various Register functions. Returns a pointer to the newly
  // added link.
  internal::LogicalLink* RegisterInternal(hci_spec::ConnectionHandle handle, bt::LinkType ll_type,
                                          hci_spec::ConnectionRole role, size_t max_payload_size);

  // If a service (identified by |psm|) requested has been registered, return a ServiceInfo object
  // containing preferred channel parameters and a callback that passes an inbound channel to the
  // registrant. The callback may be called repeatedly to pass multiple channels for |psm|, but
  // should not be stored because the service may be unregistered at a later time. Calls for
  // unregistered services return null.
  std::optional<ServiceInfo> QueryService(hci_spec::ConnectionHandle handle, PSM psm);

  // Maximum sizes for data packet payloads from host to controller.
  const size_t max_acl_payload_size_;
  const size_t max_le_payload_size_;

  hci::AclDataChannel* acl_data_channel_;

  using LinkMap =
      std::unordered_map<hci_spec::ConnectionHandle, fbl::RefPtr<internal::LogicalLink>>;
  LinkMap ll_map_;
  inspect::Node ll_node_;

  // Stores packets received on a connection handle before a link for it has
  // been created.
  using PendingPacketMap =
      std::unordered_map<hci_spec::ConnectionHandle, LinkedList<hci::ACLDataPacket>>;
  PendingPacketMap pending_packets_;

  // Store information required to create and forward channels for locally-
  // hosted services.
  struct ServiceData {
    void AttachInspect(inspect::Node& parent);
    ServiceInfo info;
    PSM psm;
    inspect::Node node;
    inspect::StringProperty psm_property;
  };
  using ServiceMap = std::unordered_map<PSM, ServiceData>;
  ServiceMap services_;
  inspect::Node services_node_;

  // Stored info on whether random channel ids are requested.
  bool random_channel_ids_;

  // TODO(fxbug.rev/63851): Find a better home for this. For now, we know that this only holds
  // promises scheduled by LogicalLinks to destroy themselves, so this living here provides a
  // minimal guarantee that the executor outlives the LogicalLinks.
  async::Executor executor_;

  fit::thread_checker thread_checker_;
  fxl::WeakPtrFactory<ChannelManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ChannelManager);
};

}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_CHANNEL_MANAGER_H_
