// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_

#include <lib/sys/inspect/cpp/component.h>

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::l2cap {

struct ChannelParameters;

// Represents the task domain that implements the host subsystem's data plane.
// This domain runs on the thread it is created on, and is not thread-safe.
// Protocols implemented here are: L2CAP.
class L2cap {
 public:
  L2cap() = default;
  virtual ~L2cap() = default;

  // Attach L2cap's inspect node as a child of |parent| with the given |name|
  static constexpr const char* kInspectNodeName = "l2cap";
  virtual void AttachInspect(inspect::Node& parent, std::string name) = 0;

  // Registers an ACL connection with the L2CAP layer. L2CAP channels can be
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
  // Has no effect if this L2cap is uninitialized or shut down.
  virtual void AddACLConnection(hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
                                l2cap::LinkErrorCallback link_error_callback,
                                l2cap::SecurityUpgradeCallback security_callback) = 0;

  // Registers an LE connection with the L2CAP layer. L2CAP channels can be
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
  // Returns the ATT and SMP fixed channels of this link.
  //
  // Has no effect if this L2cap is uninitialized or shut down.
  struct LEFixedChannels {
    fbl::RefPtr<l2cap::Channel> att;
    fbl::RefPtr<l2cap::Channel> smp;
  };
  virtual LEFixedChannels AddLEConnection(
      hci_spec::ConnectionHandle handle, hci_spec::ConnectionRole role,
      l2cap::LinkErrorCallback link_error_callback,
      l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
      l2cap::SecurityUpgradeCallback security_callback) = 0;

  // Removes a previously registered connection. All corresponding Channels will
  // be closed and all incoming data packets on this link will be dropped.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  //
  // Has no effect if this L2cap is uninitialized or shut down.
  virtual void RemoveConnection(hci_spec::ConnectionHandle handle) = 0;

  // Assigns the security level of a logical link. Has no effect if |handle| has
  // not been previously registered or the link is closed.
  virtual void AssignLinkSecurityProperties(hci_spec::ConnectionHandle handle,
                                            sm::SecurityProperties security) = 0;

  // Send an LE Connection Parameter Update Request requesting |params| on the LE signaling channel
  // of the LE connection represented by |handle|. This should only be used if the LE follower and
  // LE leader do not support the Connection Parameters Request Link Layer Control Procedure
  // (Core Spec v5.2  Vol 3, Part A, Sec 4.20). This should only be called when the local host is an
  // LE follower.
  //
  // |request_cb| will be called when a response (indicating acceptance or rejection) is received.
  virtual void RequestConnectionParameterUpdate(
      hci_spec::ConnectionHandle handle, hci_spec::LEPreferredConnectionParameters params,
      l2cap::ConnectionParameterUpdateRequestCallback request_cb) = 0;

  // Open an outbound dynamic channel against a peer's Protocol/Service
  // Multiplexing (PSM) code |psm| on a link identified by |handle| using the preferred channel
  // parameters |params|. If the peer requires different higher priority parameters, the local
  // device will accept those instead.
  //
  // |cb| will be called with the channel created to the remote, or nullptr if the channel creation
  // resulted in an error.
  //
  // Has no effect if this L2cap is uninitialized or shut down.
  virtual void OpenL2capChannel(hci_spec::ConnectionHandle handle, l2cap::PSM psm,
                                l2cap::ChannelParameters params, l2cap::ChannelCallback cb) = 0;

  // Registers a handler for peer-initiated dynamic channel requests that have
  // the Protocol/Service Multiplexing (PSM) code |psm|. The local device will attempt to configure
  // these channels using the preferred parameters |params|, but will accept different channel
  // parameters required by the peer if they are higher priority.
  //
  // |cb| will be called with the channel created by each inbound connection request received.
  // Handlers must be unregistered before they are replaced.
  //
  // Returns false if |psm| is invalid or already has a handler registered.
  //
  // Inbound connection requests with a PSM that has no registered handler will
  // be rejected.
  //
  // Has no effect if this L2cap is uninitialized or shut down.
  //
  // TODO(xow): Dynamic PSMs may need their routing space (ACL or LE) identified
  virtual bool RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                               l2cap::ChannelCallback callback) = 0;

  // Removes the handler for inbound channel requests for the previously-
  // registered service identified by |psm|. This only prevents new inbound
  // channels from being opened but does not close already-open channels.
  //
  // Has no effect if this L2cap is uninitialized or shut down.
  virtual void UnregisterService(l2cap::PSM psm) = 0;
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_L2CAP_H_
