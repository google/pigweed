// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/trace/event.h>
#include <zircon/compiler.h>

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_command_handler.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/low_energy_command_handler.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/recombiner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::l2cap::internal {

class ChannelImpl;
class LESignalingChannel;
class SignalingChannel;

// Represents a controller logical link. Each instance aids in mapping L2CAP channels to their
// corresponding controller logical link and vice versa. This owns each link's signaling fixed
// channel and the dynamic channel logic that operates on that channel. A LogicalLink must be
// explicitly `Close`d before destruction, and will assert on this behavior in its destructor.
//
// Instances are created and owned by a ChannelManager.
class LogicalLink final {
 public:
  // Returns a function that accepts opened channels for a registered local service identified by
  // |psm| on a given connection identified by |handle|, or nullptr if there is no service
  // registered for that PSM.
  using QueryServiceCallback = fit::function<std::optional<ChannelManager::ServiceInfo>(
      hci_spec::ConnectionHandle handle, PSM psm)>;

  // Constructs a new LogicalLink and initializes the signaling fixed channel.
  // |max_payload_size| shall be the maximum "host to controller" data packet payload size for the
  // link |type|, per Core Spec v5.0 Vol 2, Part E, Sec 4.1.
  // Both |query_service_cb| and the inbound channel delivery callback that it returns will be
  // executed on this object's creation thread.
  // If |random_channel_ids| is true, assign dynamic channels randomly instead of
  // starting at the beginning of the dynamic channel range.
  LogicalLink(hci_spec::ConnectionHandle handle, bt::LinkType type, hci_spec::ConnectionRole role,
              size_t max_payload_size, QueryServiceCallback query_service_cb,
              hci::AclDataChannel* acl_data_channel, hci::CommandChannel* cmd_channel,
              bool random_channel_ids);

  // When a logical link is destroyed it notifies all of its channels to close themselves. Data
  // packets will no longer be routed to the associated channels.
  ~LogicalLink();

  // Notifies and closes all open channels on this link. This must be called to
  // cleanly shut down a LogicalLink.
  //
  // The link MUST not be closed when this is called.
  void Close();

  // Opens the channel with |channel_id| over this logical link. See channel.h
  // for documentation on |rx_callback| and |closed_callback|. Returns nullptr
  // if a Channel for |channel_id| already exists.
  //
  // The link MUST not be closed when this is called.
  fxl::WeakPtr<Channel> OpenFixedChannel(ChannelId channel_id);

  // Opens a dynamic channel to the requested |psm| with the preferred parameters |params| and
  // returns a channel asynchronously via |callback|.
  //
  // The link MUST not be closed when this is called.
  void OpenChannel(PSM psm, ChannelParameters params, ChannelCallback callback);

  // Takes ownership of |packet| for PDU processing and routes it to its target
  // channel. This must be called on this object's creation thread.
  //
  // The link MUST not be closed when this is called.
  void HandleRxPacket(hci::ACLDataPacketPtr packet);

  // Sends a PDU out over the ACL data channel, where |payload| is the contents following the Basic
  // L2CAP header and preceding the Frame Check Sequence (FCS; if enabled with |fcs_option|). Frame
  // formats are defined in Core Spec v5.0, Vol 3, Part A, Section 3.
  //
  // |remote_id| identifies the peer's L2CAP channel endpoint for this frame. This must be called on
  // the creation thread.
  //
  // |flushable| indicates whether the PDU should be marked as flushable (the controller can
  // flush the PDU after the automatic flush timeout).
  //
  // It is safe to call this function on a closed link; it will have no effect.
  void SendFrame(ChannelId remote_id, const ByteBuffer& payload,
                 FrameCheckSequenceOption fcs_option, bool flushable);

  // Requests a security upgrade using the registered security upgrade callback.
  // Invokes the |callback| argument with the result of the operation.
  //
  // Has no effect if the link is closed.
  void UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback);

  // Assigns the security level of this link and resolves pending security
  // upgrade requests. Has no effect if the link is closed.
  void AssignSecurityProperties(const sm::SecurityProperties& security);

  // Send a Connection Parameter Update Request on the LE signaling channel. When the Connection
  // Parameter Update Response is received, |request_cb| will be called with the result, |accepted|.
  // NOTE: the local Host must be an LE peripheral.
  void SendConnectionParameterUpdateRequest(hci_spec::LEPreferredConnectionParameters params,
                                            ConnectionParameterUpdateRequestCallback request_cb);

  // Request a change of this link's ACL priority to |priority|.
  // |channel| must indicate the channel making the request.
  // |callback| will be called with the result of the request.
  // The request will fail if |priority| conflicts with another channel's priority or the
  // controller does not support changing the ACL priority.
  //
  // Requests are queued and handled sequentially in order to prevent race conditions.
  void RequestAclPriority(Channel* channel, hci::AclPriority priority,
                          fit::callback<void(fitx::result<fitx::failed>)> callback);

  // Sets an automatic flush timeout with duration |flush_timeout|. |callback| will be called with
  // the result of the operation. This is only supported if the link type is kACL (BR/EDR).
  // |flush_timeout| must be in the range [1ms - hci_spec::kMaxAutomaticFlushTimeoutDuration]. A
  // flush timeout of zx::duration::infinite() indicates an infinite flush timeout (no automatic
  // flush), the default.
  void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout, hci::ResultCallback<> callback);

  // Attach LogicalLink's inspect node as a child of |parent| with the given |name|.
  void AttachInspect(inspect::Node& parent, std::string name);

  // Returns mapping of ChannelId -> PacketPriority for use with AclDataChannel::SendPacket.
  static hci::AclDataChannel::PacketPriority ChannelPriority(ChannelId id);

  // Assigns the link error callback to be invoked when a channel signals a link
  // error.
  void set_error_callback(fit::closure callback);

  // Assigns the security upgrade delegate for this link.
  void set_security_upgrade_callback(SecurityUpgradeCallback callback);

  // Assigns the callback to be invoked when a valid Connection Parameter Update Request is received
  // on the signaling channel.
  void set_connection_parameter_update_callback(LEConnectionParameterUpdateCallback callback);

  bt::LinkType type() const { return type_; }
  hci_spec::ConnectionRole role() const { return role_; }
  hci_spec::ConnectionHandle handle() const { return handle_; }

  const sm::SecurityProperties security() { return security_; }

  // Returns the LE signaling channel implementation or nullptr if this is not a
  // LE-U link.
  LESignalingChannel* le_signaling_channel() const;

  fxl::WeakPtr<LogicalLink> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  friend class ChannelImpl;

  // Returns true if |id| is valid and supported by the peer.
  bool AllowsFixedChannel(ChannelId id);

  // Called by ChannelImpl::Deactivate(). Removes the channel from the given link. Calls
  // |removed_cb| when the channel no longer exists.
  void RemoveChannel(Channel* chan, fit::closure removed_cb);

  // Called by ChannelImpl::SignalLinkError() to disconnect all channels then signal an error to the
  // lower layers (usually GAP, to request a link disconnection). Has no effect if the link is
  // closed.
  void SignalError();

  // If the service identified by |psm| can be opened, return a function to
  // complete the channel open for a newly-opened DynamicChannel. Otherwise,
  // return nullptr.
  //
  // This MUST not be called on a closed link.
  std::optional<DynamicChannelRegistry::ServiceInfo> OnServiceRequest(PSM psm);

  // Called by |dynamic_registry_| when the peer requests the closure of a
  // dynamic channel using a signaling PDU.
  //
  // This MUST not be called on a closed link.
  void OnChannelDisconnectRequest(const DynamicChannel* dyn_chan);

  // Given a newly-opened dynamic channel as reported by this link's DynamicChannelRegistry, create
  // a ChannelImpl for it to carry user data, then pass a pointer to it through |open_cb|. If
  // |dyn_chan| is null, then pass nullptr into |open_cb|.
  //
  // This MUST not be called on a closed link.
  void CompleteDynamicOpen(const DynamicChannel* dyn_chan, ChannelCallback open_cb);

  // Send an Information Request signaling packet of type Fixed Channels Supported.
  void SendFixedChannelsSupportedInformationRequest();

  // Handler for Information Response signaling packet. This is used to handle the Fixed Channels
  // Supported information response, which indicates which fixed channels the peer supports (Core
  // Spec v5.1, Vol 3, Part A, Sec 4.13). Except for the signaling channels, fixed channels may not
  // be created until this response has been received.
  // TODO(fxbug.dev/43668): save fixed channels mask and use to verify opened fixed channel ids are
  // supported
  void OnRxFixedChannelsSupportedInfoRsp(const BrEdrCommandHandler::InformationResponse& rsp);

  // Start serving Connection Parameter Update Requests on the LE signaling channel.
  void ServeConnectionParameterUpdateRequest();

  // Handler called when a Connection Parameter Update Request is received on the LE signaling
  // channel.
  void OnRxConnectionParameterUpdateRequest(
      uint16_t interval_min, uint16_t interval_max, uint16_t peripheral_latency,
      uint16_t timeout_multiplier,
      LowEnergyCommandHandler::ConnectionParameterUpdateResponder* responder);

  // Processes the next ACL priority request in the  |pending_acl_requests_| queue.
  // In order to optimize radio performance, ACL priority is downgraded whenever possible (i.e. when
  // no more channels are requesting high priority).
  void HandleNextAclPriorityRequest();

  sm::SecurityProperties security_;

  // Information about the underlying controller logical link.
  hci_spec::ConnectionHandle handle_;
  bt::LinkType type_;
  hci_spec::ConnectionRole role_;

  // The duration after which BR/EDR packets are flushed from the controller.
  // By default, the flush timeout is infinite (no automatic flush).
  UintInspectable<zx::duration> flush_timeout_;

  fit::closure link_error_cb_;

  SecurityUpgradeCallback security_callback_;

  LEConnectionParameterUpdateCallback connection_parameter_update_callback_;

  // No data packets are processed once this gets set to true.
  bool closed_;

  // Fragmenter and Recombiner are always accessed on the L2CAP thread.
  const Fragmenter fragmenter_;
  Recombiner recombiner_;

  // Channels that were created on this link. Channels notify the link for
  // removal when deactivated.
  using ChannelMap = std::unordered_map<ChannelId, std::unique_ptr<ChannelImpl>>;
  ChannelMap channels_;

  // Manages the L2CAP signaling channel on this logical link. Depending on |type_| this will
  // either implement the LE or BR/EDR signaling commands.
  std::unique_ptr<SignalingChannel> signaling_channel_;

  // Stores packets that have been received on a currently closed channel. We
  // buffer these for fixed channels so that the data is available when the
  // channel is opened.
  using PendingPduMap = std::unordered_map<ChannelId, std::list<PDU>>;
  PendingPduMap pending_pdus_;

  struct PendingAclRequest {
    fxl::WeakPtr<ChannelImpl> channel;
    hci::AclPriority priority;
    fit::callback<void(fitx::result<fitx::failed>)> callback;
  };
  std::queue<PendingAclRequest> pending_acl_requests_;

  // The current ACL priority of this link.
  hci::AclPriority acl_priority_ = hci::AclPriority::kNormal;

  // Dynamic channels opened with the remote. The registry is destroyed and all
  // procedures terminated when this link gets closed.
  std::unique_ptr<DynamicChannelRegistry> dynamic_registry_;

  hci::AclDataChannel* acl_data_channel_;
  hci::CommandChannel* cmd_channel_;

  // Search function for inbound service requests. Returns handler that accepts opened channels.
  QueryServiceCallback query_service_cb_;

  struct InspectProperties {
    inspect::Node node;
    inspect::Node channels_node;
    inspect::StringProperty handle;
    inspect::StringProperty link_type;
  };
  InspectProperties inspect_properties_;

  fxl::WeakPtrFactory<LogicalLink> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LogicalLink);
};

}  // namespace bt::l2cap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_LOGICAL_LINK_H_
