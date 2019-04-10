// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_

#include <fbl/macros.h>
#include <fbl/ref_counted.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <zircon/compiler.h>

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/recombiner.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace l2cap {
namespace internal {

class ChannelImpl;
class LESignalingChannel;
class SignalingChannel;

// Represents a controller logical link. Each instance aids in mapping L2CAP
// channels to their corresponding controller logical link and vice versa.
// Instances are created and owned by a ChannelManager.
class LogicalLink final : public fbl::RefCounted<LogicalLink> {
 public:
  // Used to query if ChannelManager has a service registered (identified by
  // |psm|). If it does, return a function that can be used to provide the
  // registrant with channels opened for the service. Otherwise, return nullptr.
  using QueryServiceCallback =
      fit::function<ChannelCallback(hci::ConnectionHandle handle, PSM psm)>;

  // Constructs a new LogicalLink and initializes the signaling fixed channel.
  static fbl::RefPtr<LogicalLink> New(hci::ConnectionHandle handle,
                                      hci::Connection::LinkType type,
                                      hci::Connection::Role role,
                                      async_dispatcher_t* dispatcher,
                                      fxl::RefPtr<hci::Transport> hci,
                                      QueryServiceCallback query_service_cb);

  // Notifies and closes all open channels on this link. This must be called to
  // cleanly shut down a LogicalLink. WARNING: Failure to do so will cause the
  // memory to leak since associated channels hold a reference back to the link.
  //
  // The link MUST not be closed when this is called.
  void Close();

  // Opens the channel with |channel_id| over this logical link. See channel.h
  // for documentation on |rx_callback| and |closed_callback|. Returns nullptr
  // if a Channel for |channel_id| already exists.
  //
  // The link MUST not be closed when this is called.
  fbl::RefPtr<Channel> OpenFixedChannel(ChannelId channel_id);

  // Opens a dynamic channel to the requested |psm| and returns a channel
  // asynchronously via |callback| (posted on the given |dispatcher|).
  //
  // The link MUST not be closed when this is called.
  void OpenChannel(PSM psm, ChannelCallback callback,
                   async_dispatcher_t* dispatcher);

  // Takes ownership of |packet| for PDU processing and routes it to its target
  // channel. This must be called on the HCI I/O thread.
  //
  // The link MUST not be closed when this is called.
  void HandleRxPacket(hci::ACLDataPacketPtr packet);

  // Sends a B-frame PDU out over the ACL data channel, where |payload| is the
  // B-frame information payload. |remote_id| identifies the destination peer's
  // L2CAP channel endpoint for this frame. This must be called on the creation
  // thread.
  //
  // It is safe to call this function on a closed link; it will have no effect.
  void SendBasicFrame(ChannelId remote_id, const common::ByteBuffer& payload);

  // Requests a security upgrade using the registered security upgrade callback.
  // Invokes the |callback| argument with the result of the operation.
  // |callback| will be run by the requested |dispatcher|.
  //
  // Has no effect if the link is closed.
  void UpgradeSecurity(sm::SecurityLevel level, sm::StatusCallback callback,
                       async_dispatcher_t* dispatcher);

  // Assigns the security level of this link and resolves pending security
  // upgrade requests. Has no effect if the link is closed.
  void AssignSecurityProperties(const sm::SecurityProperties& security);

  // Assigns the link error callback to be invoked when a channel signals a link
  // error.
  void set_error_callback(fit::closure callback,
                          async_dispatcher_t* dispatcher);

  // Assigns the security upgrade delegate for this link.
  void set_security_upgrade_callback(SecurityUpgradeCallback callback,
                                     async_dispatcher_t* dispatcher);

  // Returns the dispatcher that this LogicalLink operates on.
  async_dispatcher_t* dispatcher() const { return dispatcher_; }

  hci::Connection::LinkType type() const { return type_; }
  hci::Connection::Role role() const { return role_; }
  hci::ConnectionHandle handle() const { return handle_; }

  const sm::SecurityProperties security() {
    std::lock_guard<std::mutex> lock(mtx_);
    return security_;
  }

  // Returns the LE signaling channel implementation or nullptr if this is not a
  // LE-U link.
  LESignalingChannel* le_signaling_channel() const;

 private:
  friend class ChannelImpl;
  friend fbl::RefPtr<LogicalLink>;

  LogicalLink(hci::ConnectionHandle handle, hci::Connection::LinkType type,
              hci::Connection::Role role, async_dispatcher_t* dispatcher,
              fxl::RefPtr<hci::Transport> hci,
              QueryServiceCallback query_service_cb);

  // Initializes the fragmenter, the fixed signaling channel, and the dynamic
  // channel registry based on the link type. Called by the factory method
  // "New()".
  void Initialize();

  // When a logical link is destroyed it notifies all of its channels to close
  // themselves. Data packets will no longer be routed to the associated
  // channels.
  ~LogicalLink();

  bool AllowsFixedChannel(ChannelId id);

  // Called by ChannelImpl::Deactivate(). Removes the channel from the given
  // link.
  //
  // Does nothing if the link is closed.
  void RemoveChannel(Channel* chan);

  // Called by ChannelImpl::SignalLinkError(). Has no effect if the link is
  // closed.
  void SignalError();

  // If the service identified by |psm| can be opened, return a function to
  // complete the channel open for a newly-opened DynamicChannel. Otherwise,
  // return nullptr.
  //
  // This MUST not be called on a closed link.
  DynamicChannelRegistry::DynamicChannelCallback OnServiceRequest(PSM psm);

  // Called by |dynamic_registry_| when the peer requests the closure of a
  // dynamic channel using a signaling PDU.
  //
  // This MUST not be called on a closed link.
  void OnChannelDisconnectRequest(const DynamicChannel* dyn_chan);

  // Given a newly-opened dynamic channel as reported by this link's
  // DynamicChannelRegistry, create a ChannelImpl for it to carry user data,
  // then pass a pointer to it through |open_cb| on |dispatcher|. If |dyn_chan|
  // is null, then pass nullptr into |open_cb|.
  //
  // This MUST not be called on a closed link.
  void CompleteDynamicOpen(const DynamicChannel* dyn_chan,
                           ChannelCallback open_cb,
                           async_dispatcher_t* dispatcher);

  // Members that can be accessed from any thread.
  std::mutex mtx_;
  sm::SecurityProperties security_ __TA_GUARDED(mtx_);

  // All members below must be accessed on the L2CAP dispatcher thread.
  fxl::RefPtr<hci::Transport> hci_;
  async_dispatcher_t* dispatcher_;

  // Information about the underlying controller logical link.
  hci::ConnectionHandle handle_;
  hci::Connection::LinkType type_;
  hci::Connection::Role role_;

  fit::closure link_error_cb_;
  async_dispatcher_t* link_error_dispatcher_;

  SecurityUpgradeCallback security_callback_;
  async_dispatcher_t* security_dispatcher_;

  // No data packets are processed once this gets set to true.
  bool closed_;

  // Owns and manages the L2CAP signaling channel on this logical link.
  // Depending on |type_| this will either implement the LE or BR/EDR signaling
  // commands.
  std::unique_ptr<SignalingChannel> signaling_channel_;

  // Fragmenter and Recombiner are always accessed on the L2CAP thread.
  Fragmenter fragmenter_;
  Recombiner recombiner_;

  // Channels that were created on this link. Channels notify the link for
  // removal when deactivated.
  using ChannelMap = std::unordered_map<ChannelId, fbl::RefPtr<ChannelImpl>>;
  ChannelMap channels_;

  // Stores packets that have been received on a currently closed channel. We
  // buffer these for fixed channels so that the data is available when the
  // channel is opened.
  using PendingPduMap = std::unordered_map<ChannelId, std::list<PDU>>;
  PendingPduMap pending_pdus_;

  // Dynamic channels opened with the remote. The registry is destroyed and all
  // procedures terminated when this link gets closed.
  std::unique_ptr<DynamicChannelRegistry> dynamic_registry_;

  QueryServiceCallback query_service_cb_;

  fxl::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LogicalLink);
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_
