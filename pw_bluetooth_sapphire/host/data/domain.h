// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_DOMAIN_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_DOMAIN_H_

#include <lib/sys/inspect/cpp/component.h>
#include <lib/zx/socket.h>

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"

namespace bt {

namespace l2cap {
struct ChannelParameters;
struct ChannelSocket;
}  // namespace l2cap

namespace data {

// Represents the task domain that implements the host subsystem's data plane.
// This domain owns its own thread on which data-path tasks are dispatched.
// Protocols implemented here are:
//
//   a. L2CAP and SCO.
//   b. RFCOMM.
//   c. Data sockets that bridge out-of-process users to above protocols.
//
// Interactions between the data domain and other library threads is performed
// primarily via message passing.
class Domain : public fbl::RefCounted<Domain> {
 public:
  static constexpr const char* kInspectNodeName = "data_domain";

  // Constructs an uninitialized data domain that can be used in production.
  // This spawns a thread on which data-domain tasks will be scheduled.
  static fbl::RefPtr<Domain> Create(fxl::RefPtr<hci::Transport> hci, inspect::Node node,
                                    std::string thread_name);

  // Constructs an instance using the given |dispatcher| instead of spawning a
  // thread. This is intended for unit tests.
  static fbl::RefPtr<Domain> CreateWithDispatcher(fxl::RefPtr<hci::Transport> hci,
                                                  inspect::Node node,
                                                  async_dispatcher_t* dispatcher);

  // These send an Initialize/ShutDown message to the data task runner. It is
  // safe for the caller to drop its Domain reference after ShutDown is called.
  //
  // Operations on an uninitialized or shut-down Domain have no effect.
  virtual void Initialize() = 0;
  virtual void ShutDown() = 0;

  // Registers an ACL connection with the L2CAP layer. L2CAP channels can be
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
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void AddACLConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                                l2cap::LinkErrorCallback link_error_callback,
                                l2cap::SecurityUpgradeCallback security_callback,
                                async_dispatcher_t* dispatcher) = 0;

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
  // Upon successful registration of the link, |channel_callback| will be called
  // with the ATT and SMP fixed channels.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void AddLEConnection(hci::ConnectionHandle handle, hci::Connection::Role role,
                               l2cap::LinkErrorCallback link_error_callback,
                               l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
                               l2cap::LEFixedChannelsCallback channel_callback,
                               l2cap::SecurityUpgradeCallback security_callback,
                               async_dispatcher_t* dispatcher) = 0;

  // Removes a previously registered connection. All corresponding Channels will
  // be closed and all incoming data packets on this link will be dropped.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void RemoveConnection(hci::ConnectionHandle handle) = 0;

  // Assigns the security level of a logical link. Has no effect if |handle| has
  // not been previously registered or the link is closed.
  virtual void AssignLinkSecurityProperties(hci::ConnectionHandle handle,
                                            sm::SecurityProperties security) = 0;

  // Send an LE Connection Parameter Update Request requesting |params| on the LE signaling channel
  // of the LE connection represented by |handle|. This should only be used if the LE follower and
  // LE leader do not support the Connection Parameters Request Link Layer Control Procedure
  // (Core Spec v5.2  Vol 3, Part A, Sec 4.20). This should only be called when the local host is an
  // LE follower.
  //
  // |request_cb| will be called on |dispatcher| when a response (indicating acceptance or
  // rejection) is received.
  virtual void RequestConnectionParameterUpdate(
      hci::ConnectionHandle handle, hci::LEPreferredConnectionParameters params,
      l2cap::ConnectionParameterUpdateRequestCallback request_cb,
      async_dispatcher_t* dispatcher) = 0;

  // Open an outbound dynamic channel against a peer's Protocol/Service
  // Multiplexing (PSM) code |psm| on a link identified by |handle| using the preferred channel
  // parameters |params|. If the peer requires different higher priority parameters, the local
  // device will accept those instead.
  //
  // |cb| will be called on |dispatcher| with the channel created to the remote,
  // or nullptr if the channel creation resulted in an error.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                l2cap::ChannelParameters params, l2cap::ChannelCallback cb,
                                async_dispatcher_t* dispatcher) = 0;

  // Open an outbound dynamic channel against a peer's Protocol/Service
  // Multiplexing (PSM) code |psm| on a link identified by |handle| using the preferred channel
  // parameters |params|.
  //
  // |socket_callback| will be called on |dispatcher| with a zx::socket corresponding to the
  // channel created to the remote or ZX_INVALID_HANDLE if the channel creation resulted in an
  // error.
  //
  // Regardless of success, |link_handle| will be the same as the initial
  // |handle| argument.
  //
  // On successful channel creation, |chan_info| contains the configured channel parameters.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  using SocketCallback =
      fit::function<void(l2cap::ChannelSocket, hci::ConnectionHandle link_handle)>;
  virtual void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                l2cap::ChannelParameters params, SocketCallback socket_callback,
                                async_dispatcher_t* dispatcher) = 0;

  // Registers a handler for peer-initiated dynamic channel requests that have
  // the Protocol/Service Multiplexing (PSM) code |psm|. The local device will attempt to configure
  // these channels using the preferred parameters |params|, but will accept different channel
  // parameters required by the peer if they are higher priority.
  //
  // |cb| will be called on |dispatcher| with the channel created by each
  // inbound connection request received. Handlers must be unregistered before
  // they are replaced.
  //
  // Returns false if |psm| is invalid or already has a handler registered.
  //
  // Inbound connection requests with a PSM that has no registered handler will
  // be rejected.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  //
  // TODO(xow): Dynamic PSMs may need their routing space (ACL or LE) identified
  virtual void RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                               l2cap::ChannelCallback callback, async_dispatcher_t* dispatcher) = 0;

  // Similar to RegisterService, but instead of providing a l2cap::Channel,
  // provides a zx::socket which can be used to communicate on the channel.
  // The underlying l2cap::Channel is activated; the socket provided will
  // receive any data sent to the channel and any data sent to the socket
  // will be sent as if sent by l2cap::Channel::Send.
  // |link_handle| disambiguates which remote device initiated the channel.
  //
  // TODO(armansito): Return the socket in a data structure that contains
  // additional meta-data about the connection, such as its link type and
  // channel configuration parameters (see NET-1084 and TODOs for
  // RegisterService above.
  virtual void RegisterService(l2cap::PSM psm, l2cap::ChannelParameters params,
                               SocketCallback socket_callback, async_dispatcher_t* dispatcher) = 0;

  // Removes the handler for inbound channel requests for the previously-
  // registered service identified by |psm|. This only prevents new inbound
  // channels from being opened but does not close already-open channels.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void UnregisterService(l2cap::PSM psm) = 0;

 protected:
  friend class fbl::RefPtr<Domain>;
  Domain() = default;
  virtual ~Domain() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Domain);
};

}  // namespace data
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_DATA_DOMAIN_H_
