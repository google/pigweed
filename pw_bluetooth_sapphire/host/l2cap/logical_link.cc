// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logical_link.h"

#include <functional>

#include "bredr_dynamic_channel.h"
#include "bredr_signaling_channel.h"
#include "channel.h"
#include "le_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/trace.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::l2cap::internal {

using pw::bluetooth::AclPriority;

namespace {

const char* kInspectHandlePropertyName = "handle";
const char* kInspectLinkTypePropertyName = "link_type";
const char* kInspectChannelsNodeName = "channels";
const char* kInspectChannelNodePrefix = "channel_";
const char* kInspectFlushTimeoutPropertyName = "flush_timeout_ms";

constexpr bool IsValidLEFixedChannel(ChannelId id) {
  switch (id) {
    case kATTChannelId:
    case kLESignalingChannelId:
    case kLESMPChannelId:
      return true;
    default:
      break;
  }
  return false;
}

constexpr bool IsValidBREDRFixedChannel(ChannelId id) {
  switch (id) {
    case kSignalingChannelId:
    case kConnectionlessChannelId:
    case kSMPChannelId:
      return true;
    default:
      break;
  }
  return false;
}

}  // namespace

LogicalLink::LogicalLink(hci_spec::ConnectionHandle handle, bt::LinkType type,
                         hci_spec::ConnectionRole role, size_t max_acl_payload_size,
                         QueryServiceCallback query_service_cb,
                         hci::AclDataChannel* acl_data_channel, hci::CommandChannel* cmd_channel,
                         bool random_channel_ids)

    : handle_(handle),
      type_(type),
      role_(role),
      flush_timeout_(zx::duration::infinite(), /*convert=*/[](auto f) { return f.to_msecs(); }),
      closed_(false),
      fragmenter_(handle, max_acl_payload_size),
      recombiner_(handle),
      acl_data_channel_(acl_data_channel),
      cmd_channel_(cmd_channel),
      query_service_cb_(std::move(query_service_cb)),
      weak_self_(this) {
  BT_ASSERT(type_ == bt::LinkType::kLE || type_ == bt::LinkType::kACL);
  BT_ASSERT(acl_data_channel_);
  BT_ASSERT(cmd_channel_);
  BT_ASSERT(query_service_cb_);

  // Set up the signaling channel and dynamic channels.
  if (type_ == bt::LinkType::kLE) {
    signaling_channel_ =
        std::make_unique<LESignalingChannel>(OpenFixedChannel(kLESignalingChannelId), role_);
    // TODO(armansito): Initialize LE registry when it exists.

    ServeConnectionParameterUpdateRequest();
  } else {
    signaling_channel_ =
        std::make_unique<BrEdrSignalingChannel>(OpenFixedChannel(kSignalingChannelId), role_);
    dynamic_registry_ = std::make_unique<BrEdrDynamicChannelRegistry>(
        signaling_channel_.get(), fit::bind_member<&LogicalLink::OnChannelDisconnectRequest>(this),
        fit::bind_member<&LogicalLink::OnServiceRequest>(this), random_channel_ids);

    SendFixedChannelsSupportedInformationRequest();
  }
}

LogicalLink::~LogicalLink() {
  bt_log(DEBUG, "l2cap", "LogicalLink destroyed (handle: %#.4x)", handle_);
  BT_ASSERT(closed_);
}

Channel::WeakPtr LogicalLink::OpenFixedChannel(ChannelId id) {
  BT_DEBUG_ASSERT(!closed_);

  TRACE_DURATION("bluetooth", "LogicalLink::OpenFixedChannel", "handle", handle_, "channel id", id);

  // We currently only support the pre-defined fixed-channels.
  if (!AllowsFixedChannel(id)) {
    bt_log(ERROR, "l2cap", "cannot open fixed channel with id %#.4x", id);
    return Channel::WeakPtr();
  }

  auto iter = channels_.find(id);
  if (iter != channels_.end()) {
    bt_log(ERROR, "l2cap", "channel is already open! (id: %#.4x, handle: %#.4x)", id, handle_);
    return Channel::WeakPtr();
  }

  std::unique_ptr<ChannelImpl> chan =
      ChannelImpl::CreateFixedChannel(id, GetWeakPtr(), cmd_channel_->AsWeakPtr());

  auto pp_iter = pending_pdus_.find(id);
  if (pp_iter != pending_pdus_.end()) {
    for (auto& pdu : pp_iter->second) {
      TRACE_FLOW_END("bluetooth", "LogicalLink::HandleRxPacket queued", pdu.trace_id());
      chan->HandleRxPdu(std::move(pdu));
    }
    pending_pdus_.erase(pp_iter);
  }

  if (inspect_properties_.channels_node) {
    chan->AttachInspect(inspect_properties_.channels_node,
                        inspect_properties_.channels_node.UniqueName(kInspectChannelNodePrefix));
  }

  channels_[id] = std::move(chan);
  return channels_[id]->GetWeakPtr();
}

void LogicalLink::OpenChannel(PSM psm, ChannelParameters params, ChannelCallback callback) {
  BT_DEBUG_ASSERT(!closed_);

  // TODO(fxbug.dev/968): Implement channels for LE credit-based connections
  if (type_ == bt::LinkType::kLE) {
    bt_log(WARN, "l2cap", "not opening LE channel for PSM %.4x", psm);
    CompleteDynamicOpen(/*dyn_chan=*/nullptr, std::move(callback));
    return;
  }

  auto create_channel = [this, cb = std::move(callback)](const DynamicChannel* dyn_chan) mutable {
    CompleteDynamicOpen(dyn_chan, std::move(cb));
  };
  dynamic_registry_->OpenOutbound(psm, params, std::move(create_channel));
}

void LogicalLink::HandleRxPacket(hci::ACLDataPacketPtr packet) {
  BT_DEBUG_ASSERT(packet);
  BT_DEBUG_ASSERT(!closed_);

  TRACE_DURATION("bluetooth", "LogicalLink::HandleRxPacket", "handle", handle_);

  // We do not support the Connectionless data channel, and the active broadcast flag can
  // only be used on the connectionless channel.  Drop packets that are broadcast.
  if (packet->broadcast_flag() == hci_spec::ACLBroadcastFlag::kActivePeripheralBroadcast) {
    bt_log(DEBUG, "l2cap", "Unsupported Broadcast Frame dropped");
    return;
  }

  auto result = recombiner_.ConsumeFragment(std::move(packet));
  if (result.frames_dropped) {
    bt_log(TRACE, "l2cap", "Frame(s) dropped due to recombination error");
  }

  if (!result.pdu) {
    // Either a partial fragment was received, which was buffered for recombination, or the packet
    // was dropped.
    return;
  }

  BT_DEBUG_ASSERT(result.pdu->is_valid());

  uint16_t channel_id = result.pdu->channel_id();
  auto iter = channels_.find(channel_id);
  PendingPduMap::iterator pp_iter;

  if (iter == channels_.end()) {
    // Only buffer data for fixed channels. This prevents stale data that is
    // intended for a closed dynamic channel from being delivered to a new
    // channel that recycled the former's ID. The downside is that it's possible
    // to lose any data that is received after a dynamic channel's connection
    // request and before its completed configuration. This would require tricky
    // additional state to track "pending open" channels here and it's not clear
    // if that is necessary since hosts should not send data before a channel is
    // first configured.
    if (!AllowsFixedChannel(channel_id)) {
      bt_log(WARN, "l2cap", "Dropping PDU for nonexistent dynamic channel %#.4x on link %#.4x",
             channel_id, handle_);
      return;
    }

    // The packet was received on a channel for which no ChannelImpl currently
    // exists. Buffer packets for the channel to receive when it gets created.
    pp_iter = pending_pdus_.emplace(channel_id, std::list<PDU>()).first;
  } else {
    // A channel exists. |pp_iter| will be valid only if the drain task has not
    // run yet (see LogicalLink::OpenFixedChannel()).
    pp_iter = pending_pdus_.find(channel_id);
  }

  if (pp_iter != pending_pdus_.end()) {
    result.pdu->set_trace_id(TRACE_NONCE());
    TRACE_FLOW_BEGIN("bluetooth", "LogicalLink::HandleRxPacket queued", result.pdu->trace_id());

    pp_iter->second.emplace_back(std::move(*result.pdu));

    bt_log(TRACE, "l2cap", "PDU buffered (channel: %#.4x, ll: %#.4x)", channel_id, handle_);
    return;
  }

  iter->second->HandleRxPdu(std::move(*result.pdu));
}

void LogicalLink::UpgradeSecurity(sm::SecurityLevel level, sm::ResultFunction<> callback) {
  BT_DEBUG_ASSERT(security_callback_);

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignoring security request on closed link");
    return;
  }

  // Report success If the link already has the expected security level.
  if (level <= security().level()) {
    callback(fit::ok());
    return;
  }

  bt_log(DEBUG, "l2cap", "Security upgrade requested (level = %s)", sm::LevelToString(level));
  security_callback_(handle_, level, std::move(callback));
}

void LogicalLink::AssignSecurityProperties(const sm::SecurityProperties& security) {
  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignoring security request on closed link");
    return;
  }

  bt_log(DEBUG, "l2cap", "Link security updated (handle: %#.4x): %s", handle_,
         security.ToString().c_str());

  security_ = security;
}

void LogicalLink::SendFrame(ChannelId id, const ByteBuffer& payload,
                            FrameCheckSequenceOption fcs_option, bool flushable) {
  if (closed_) {
    bt_log(DEBUG, "l2cap", "Drop out-bound packet on closed link");
    return;
  }

  // Copy payload into L2CAP frame fragments, sized for the HCI data transport.
  PDU pdu = fragmenter_.BuildFrame(id, payload, fcs_option, flushable);
  auto fragments = pdu.ReleaseFragments();

  BT_ASSERT(!fragments.empty());
  acl_data_channel_->SendPackets(std::move(fragments), id, ChannelPriority(id));
}

void LogicalLink::set_error_callback(fit::closure callback) {
  link_error_cb_ = std::move(callback);
}

void LogicalLink::set_security_upgrade_callback(SecurityUpgradeCallback callback) {
  security_callback_ = std::move(callback);
}

void LogicalLink::set_connection_parameter_update_callback(
    LEConnectionParameterUpdateCallback callback) {
  connection_parameter_update_callback_ = std::move(callback);
}

LESignalingChannel* LogicalLink::le_signaling_channel() const {
  return (type_ == bt::LinkType::kLE) ? static_cast<LESignalingChannel*>(signaling_channel_.get())
                                      : nullptr;
}

bool LogicalLink::AllowsFixedChannel(ChannelId id) {
  return (type_ == bt::LinkType::kLE) ? IsValidLEFixedChannel(id) : IsValidBREDRFixedChannel(id);
}

void LogicalLink::RemoveChannel(Channel* chan, fit::closure removed_cb) {
  BT_DEBUG_ASSERT(chan);

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignore RemoveChannel() on closed link");
    removed_cb();
    return;
  }

  const ChannelId id = chan->id();
  auto iter = channels_.find(id);
  if (iter == channels_.end()) {
    removed_cb();
    return;
  }

  // Ignore if the found channel doesn't match the requested one (even though
  // their IDs are the same).
  if (iter->second.get() != chan) {
    removed_cb();
    return;
  }

  pending_pdus_.erase(id);
  channels_.erase(iter);

  // Drop stale packets queued for this channel.
  hci::AclDataChannel::AclPacketPredicate predicate = [this, id](const auto& packet,
                                                                 l2cap::ChannelId channel_id) {
    return packet->connection_handle() == handle_ && id == channel_id;
  };
  acl_data_channel_->DropQueuedPackets(std::move(predicate));

  // Disconnect the channel if it's a dynamic channel. This path is for local-
  // initiated closures and does not invoke callbacks back to the channel user.
  // TODO(armansito): Change this if statement into an assert when a registry
  // gets created for LE channels.
  if (dynamic_registry_) {
    dynamic_registry_->CloseChannel(id, std::move(removed_cb));
    return;
  }
  removed_cb();
}

void LogicalLink::SignalError() {
  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignore SignalError() on closed link");
    return;
  }

  bt_log(INFO, "l2cap", "Upper layer error on link %#.4x; closing all channels", handle());

  uint16_t num_channels_closing = channels_.size();

  if (signaling_channel_) {
    BT_ASSERT(channels_.count(kSignalingChannelId) || channels_.count(kLESignalingChannelId));
    // There is no need to close the signaling channel.
    num_channels_closing--;
  }

  if (num_channels_closing == 0) {
    link_error_cb_();
    return;
  }

  // num_channels_closing is shared across all callbacks.
  fit::closure channel_removed_cb = [this, num_channels_closing = num_channels_closing]() mutable {
    num_channels_closing--;
    if (num_channels_closing != 0) {
      return;
    }
    bt_log(TRACE, "l2cap", "Channels on link %#.4x closed; passing error to lower layer", handle());
    // Invoking error callback may destroy this LogicalLink.
    link_error_cb_();
  };

  for (auto channel_iter = channels_.begin(); channel_iter != channels_.end();) {
    auto& [id, channel] = *channel_iter++;

    // Do not close the signaling channel, as it is used to close the dynamic channels.
    if (id == kSignalingChannelId || id == kLESignalingChannelId) {
      continue;
    }

    // Signal the channel, as it did not request the closure.
    channel->OnClosed();

    // This erases from |channel_| and invalidates any iterator pointing to |channel|.
    RemoveChannel(channel.get(), channel_removed_cb.share());
  }
}

void LogicalLink::Close() {
  BT_DEBUG_ASSERT(!closed_);

  closed_ = true;

  for (auto& iter : channels_) {
    iter.second->OnClosed();
  }
  channels_.clear();
  dynamic_registry_.reset();
}

std::optional<DynamicChannelRegistry::ServiceInfo> LogicalLink::OnServiceRequest(PSM psm) {
  BT_DEBUG_ASSERT(!closed_);

  // Query upper layer for a service handler attached to this PSM.
  auto result = query_service_cb_(handle_, psm);
  if (!result) {
    return std::nullopt;
  }

  auto channel_cb =
      [this, chan_cb = std::move(result->channel_cb)](const DynamicChannel* dyn_chan) mutable {
        CompleteDynamicOpen(dyn_chan, std::move(chan_cb));
      };
  return DynamicChannelRegistry::ServiceInfo(result->channel_params, std::move(channel_cb));
}

void LogicalLink::OnChannelDisconnectRequest(const DynamicChannel* dyn_chan) {
  BT_DEBUG_ASSERT(dyn_chan);
  BT_DEBUG_ASSERT(!closed_);

  auto iter = channels_.find(dyn_chan->local_cid());
  if (iter == channels_.end()) {
    bt_log(WARN, "l2cap", "No ChannelImpl found for closing dynamic channel %#.4x",
           dyn_chan->local_cid());
    return;
  }

  ChannelImpl* channel = iter->second.get();
  BT_DEBUG_ASSERT(channel->remote_id() == dyn_chan->remote_cid());

  hci::AclDataChannel::AclPacketPredicate predicate =
      [this, id = channel->id()](const auto& packet, l2cap::ChannelId channel_id) {
        return packet->connection_handle() == handle_ && id == channel_id;
      };
  acl_data_channel_->DropQueuedPackets(std::move(predicate));

  // Signal closure because this is a remote disconnection.
  channel->OnClosed();
  channels_.erase(iter);
}

void LogicalLink::CompleteDynamicOpen(const DynamicChannel* dyn_chan, ChannelCallback open_cb) {
  BT_DEBUG_ASSERT(!closed_);

  if (!dyn_chan) {
    open_cb(Channel::WeakPtr());
    return;
  }

  const ChannelId local_cid = dyn_chan->local_cid();
  const ChannelId remote_cid = dyn_chan->remote_cid();
  bt_log(DEBUG, "l2cap", "Link %#.4x: Channel opened with ID %#.4x (remote ID: %#.4x, psm: %s)",
         handle_, local_cid, remote_cid, PsmToString(dyn_chan->psm()).c_str());

  auto chan_info = dyn_chan->info();
  // Extract preferred flush timeout to avoid creating channel with a flush timeout that hasn't been
  // successfully configured yet.
  auto preferred_flush_timeout = chan_info.flush_timeout;
  chan_info.flush_timeout.reset();

  std::unique_ptr<ChannelImpl> chan = ChannelImpl::CreateDynamicChannel(
      local_cid, remote_cid, GetWeakPtr(), chan_info, cmd_channel_->AsWeakPtr());
  auto chan_weak = chan->GetWeakPtr();
  channels_[local_cid] = std::move(chan);

  if (inspect_properties_.channels_node) {
    chan_weak->AttachInspect(
        inspect_properties_.channels_node,
        inspect_properties_.channels_node.UniqueName(kInspectChannelNodePrefix));
  }

  // If a flush timeout was requested for this channel, try to set it before returning the channel
  // to the client to ensure outbound PDUs have correct flushable flag.
  if (preferred_flush_timeout.has_value()) {
    chan_weak->SetBrEdrAutomaticFlushTimeout(
        preferred_flush_timeout.value(),
        [cb = std::move(open_cb), chan_weak](auto /*result*/) { cb(chan_weak); });
    return;
  }

  open_cb(std::move(chan_weak));
}

void LogicalLink::SendFixedChannelsSupportedInformationRequest() {
  BT_ASSERT(signaling_channel_);

  BrEdrCommandHandler cmd_handler(signaling_channel_.get());
  if (!cmd_handler.SendInformationRequest(InformationType::kFixedChannelsSupported,
                                          [self = GetWeakPtr()](auto& rsp) {
                                            if (self.is_alive()) {
                                              self->OnRxFixedChannelsSupportedInfoRsp(rsp);
                                            }
                                          })) {
    bt_log(ERROR, "l2cap", "Failed to send Fixed Channels Supported Information Request");
    return;
  }

  bt_log(TRACE, "l2cap", "Sent Fixed Channels Supported Information Request");
}

void LogicalLink::OnRxFixedChannelsSupportedInfoRsp(
    const BrEdrCommandHandler::InformationResponse& rsp) {
  if (rsp.status() == BrEdrCommandHandler::Status::kReject) {
    bt_log(TRACE, "l2cap", "Fixed Channels Supported Information Request rejected (reason %#.4hx)",
           rsp.reject_reason());
    return;
  }

  if (rsp.result() == InformationResult::kNotSupported) {
    bt_log(TRACE, "l2cap",
           "Received Fixed Channels Supported Information Response (result: Not Supported)");
    return;
  }

  if (rsp.result() != InformationResult::kSuccess) {
    bt_log(TRACE, "l2cap", "Received Fixed Channels Supported Information Response (result: %.4hx)",
           static_cast<uint16_t>(rsp.result()));
    return;
  }

  if (rsp.type() != InformationType::kFixedChannelsSupported) {
    bt_log(TRACE, "l2cap",
           "Incorrect Fixed Channels Supported Information Response type (type: %#.4hx)",
           rsp.type());
    return;
  }

  bt_log(TRACE, "l2cap", "Received Fixed Channels Supported Information Response (mask: %#016lx)",
         rsp.fixed_channels());
}

void LogicalLink::SendConnectionParameterUpdateRequest(
    hci_spec::LEPreferredConnectionParameters params,
    ConnectionParameterUpdateRequestCallback request_cb) {
  BT_ASSERT(signaling_channel_);
  BT_ASSERT(type_ == bt::LinkType::kLE);
  BT_ASSERT(role_ == hci_spec::ConnectionRole::PERIPHERAL);

  LowEnergyCommandHandler cmd_handler(signaling_channel_.get());
  cmd_handler.SendConnectionParameterUpdateRequest(
      params.min_interval(), params.max_interval(), params.max_latency(),
      params.supervision_timeout(),
      [cb = std::move(request_cb)](
          const LowEnergyCommandHandler::ConnectionParameterUpdateResponse& rsp) mutable {
        bool accepted = false;

        if (rsp.status() != LowEnergyCommandHandler::Status::kSuccess) {
          bt_log(TRACE, "l2cap", "LE Connection Parameter Update Request rejected (reason: %#.4hx)",
                 rsp.reject_reason());
        } else {
          accepted = rsp.result() == ConnectionParameterUpdateResult::kAccepted;
        }
        cb(accepted);
      });
}

void LogicalLink::RequestAclPriority(Channel::WeakPtr channel, AclPriority priority,
                                     fit::callback<void(fit::result<fit::failed>)> callback) {
  BT_ASSERT(channel.is_alive());
  auto iter = channels_.find(channel->id());
  BT_ASSERT(iter != channels_.end());
  pending_acl_requests_.push(PendingAclRequest{std::move(channel), priority, std::move(callback)});
  if (pending_acl_requests_.size() == 1) {
    HandleNextAclPriorityRequest();
  }
}

void LogicalLink::SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                                hci::ResultCallback<> callback) {
  if (type_ != bt::LinkType::kACL) {
    bt_log(ERROR, "l2cap", "attempt to set flush timeout on non-ACL logical link");
    callback(ToResult(hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS));
    return;
  }

  auto callback_wrapper = [self = GetWeakPtr(), flush_timeout,
                           cb = std::move(callback)](auto result) mutable {
    if (self.is_alive() && result.is_ok()) {
      self->flush_timeout_.Set(flush_timeout);
    }
    cb(result);
  };

  if (flush_timeout < zx::msec(1) || (flush_timeout > hci_spec::kMaxAutomaticFlushTimeoutDuration &&
                                      flush_timeout != zx::duration::infinite())) {
    callback_wrapper(ToResult(hci_spec::StatusCode::INVALID_HCI_COMMAND_PARAMETERS));
    return;
  }

  uint16_t converted_flush_timeout;
  if (flush_timeout == zx::duration::infinite()) {
    // The command treats a flush timeout of 0 as infinite.
    converted_flush_timeout = 0;
  } else {
    // Slight imprecision from casting or converting to ms is fine for the flush timeout (a few
    // ms difference from the requested value doesn't matter). Overflow is not possible because of
    // the max value check above.
    converted_flush_timeout =
        static_cast<uint16_t>(static_cast<float>(flush_timeout.to_msecs()) *
                              hci_spec::kFlushTimeoutMsToCommandParameterConversionFactor);
    BT_ASSERT(converted_flush_timeout != 0);
    BT_ASSERT(converted_flush_timeout <= hci_spec::kMaxAutomaticFlushTimeoutCommandParameterValue);
  }

  auto packet = hci::CommandPacket::New(hci_spec::kWriteAutomaticFlushTimeout,
                                        sizeof(hci_spec::WriteAutomaticFlushTimeoutCommandParams));
  auto packet_view = packet->mutable_payload<hci_spec::WriteAutomaticFlushTimeoutCommandParams>();
  packet_view->connection_handle = htole16(handle_);
  packet_view->flush_timeout = htole16(converted_flush_timeout);

  cmd_channel_->SendCommand(std::move(packet), [cb = std::move(callback_wrapper), handle = handle_,
                                                flush_timeout](
                                                   auto, const hci::EventPacket& event) mutable {
    if (event.ToResult().is_error()) {
      bt_log(WARN, "hci", "WriteAutomaticFlushTimeout command failed (result: %s, handle: %#.4x)",
             bt_str(event.ToResult()), handle);
    } else {
      bt_log(DEBUG, "hci", "automatic flush timeout updated (handle: %#.4x, timeout: %ld ms)",
             handle, flush_timeout.to_msecs());
    }
    cb(event.ToResult());
  });
}

void LogicalLink::AttachInspect(inspect::Node& parent, std::string name) {
  if (!parent) {
    return;
  }

  auto node = parent.CreateChild(name);
  inspect_properties_.handle = node.CreateString(kInspectHandlePropertyName,
                                                 bt_lib_cpp_string::StringPrintf("%#.4x", handle_));
  inspect_properties_.link_type =
      node.CreateString(kInspectLinkTypePropertyName, LinkTypeToString(type_));
  inspect_properties_.channels_node = node.CreateChild(kInspectChannelsNodeName);
  flush_timeout_.AttachInspect(node, kInspectFlushTimeoutPropertyName);
  inspect_properties_.node = std::move(node);

  for (auto& [_, chan] : channels_) {
    chan->AttachInspect(inspect_properties_.channels_node,
                        inspect_properties_.channels_node.UniqueName(kInspectChannelNodePrefix));
  }
}

hci::AclDataChannel::PacketPriority LogicalLink::ChannelPriority(ChannelId id) {
  switch (id) {
    case kSignalingChannelId:
    case kLESignalingChannelId:
    case kSMPChannelId:
    case kLESMPChannelId:
      return hci::AclDataChannel::PacketPriority::kHigh;
    default:
      return hci::AclDataChannel::PacketPriority::kLow;
  }
}

void LogicalLink::HandleNextAclPriorityRequest() {
  if (pending_acl_requests_.empty() || closed_) {
    return;
  }

  auto& request = pending_acl_requests_.front();
  BT_ASSERT(request.callback);

  // Prevent closed channels with queued requests from upgrading channel priority.
  // Allow closed channels to downgrade priority so that they can clean up their priority on
  // destruction.
  if (!request.channel.is_alive() && request.priority != AclPriority::kNormal) {
    request.callback(fit::failed());
    pending_acl_requests_.pop();
    HandleNextAclPriorityRequest();
    return;
  }

  // Skip sending command if desired priority is already set. Do this here instead of Channel in
  // case Channel queues up multiple requests.
  if (request.priority == acl_priority_) {
    request.callback(fit::ok());
    pending_acl_requests_.pop();
    HandleNextAclPriorityRequest();
    return;
  }

  // If priority is not kNormal, then a channel might be using a conflicting priority, and the new
  // priority should not be requested.
  if (acl_priority_ != AclPriority::kNormal) {
    for (auto& [chan_id, chan] : channels_) {
      if ((request.channel.is_alive() && chan.get() == &request.channel.get()) ||
          chan->requested_acl_priority() == AclPriority::kNormal) {
        continue;
      }

      // If the request returns priority to normal but a different channel still requires high
      // priority, skip sending command and just report success.
      if (request.priority == AclPriority::kNormal) {
        request.callback(fit::ok());
        break;
      }

      // If the request tries to upgrade priority but it conflicts with another channel's priority
      // (e.g. sink vs. source), report an error.
      if (request.priority != chan->requested_acl_priority()) {
        request.callback(fit::failed());
        break;
      }
    }

    if (!request.callback) {
      pending_acl_requests_.pop();
      HandleNextAclPriorityRequest();
      return;
    }
  }

  auto cb_wrapper = [self = GetWeakPtr(), cb = std::move(request.callback),
                     priority = request.priority](auto result) mutable {
    if (!self.is_alive()) {
      return;
    }
    if (result.is_ok()) {
      self->acl_priority_ = priority;
    }
    cb(result);
    self->pending_acl_requests_.pop();
    self->HandleNextAclPriorityRequest();
  };

  acl_data_channel_->RequestAclPriority(request.priority, handle_, std::move(cb_wrapper));
}

void LogicalLink::ServeConnectionParameterUpdateRequest() {
  BT_ASSERT(signaling_channel_);
  BT_ASSERT(type_ == bt::LinkType::kLE);

  LowEnergyCommandHandler cmd_handler(signaling_channel_.get());
  cmd_handler.ServeConnectionParameterUpdateRequest(
      fit::bind_member<&LogicalLink::OnRxConnectionParameterUpdateRequest>(this));
}

void LogicalLink::OnRxConnectionParameterUpdateRequest(
    uint16_t interval_min, uint16_t interval_max, uint16_t peripheral_latency,
    uint16_t timeout_multiplier,
    LowEnergyCommandHandler::ConnectionParameterUpdateResponder* responder) {
  // Only a LE peripheral can send this command. "If a Peripheral’s Host receives an
  // L2CAP_CONNECTION_PARAMETER_UPDATE_REQ packet it shall respond with an L2CAP_COMMAND_REJECT_RSP
  // packet with reason 0x0000 (Command not understood)." (v5.0, Vol 3, Part A, Section 4.20)
  if (role_ == hci_spec::ConnectionRole::PERIPHERAL) {
    bt_log(DEBUG, "l2cap", "rejecting conn. param. update request from central");
    responder->RejectNotUnderstood();
    return;
  }

  // Reject the connection parameters if they are outside the ranges allowed by
  // the HCI specification (see HCI_LE_Connection_Update command v5.0, Vol 2,
  // Part E, Section 7.8.18).
  bool reject = false;

  hci_spec::LEPreferredConnectionParameters params(interval_min, interval_max, peripheral_latency,
                                                   timeout_multiplier);

  if (params.min_interval() > params.max_interval()) {
    bt_log(DEBUG, "l2cap", "conn. min interval larger than max");
    reject = true;
  } else if (params.min_interval() < hci_spec::kLEConnectionIntervalMin) {
    bt_log(DEBUG, "l2cap", "conn. min interval outside allowed range: %#.4x",
           params.min_interval());
    reject = true;
  } else if (params.max_interval() > hci_spec::kLEConnectionIntervalMax) {
    bt_log(DEBUG, "l2cap", "conn. max interval outside allowed range: %#.4x",
           params.max_interval());
    reject = true;
  } else if (params.max_latency() > hci_spec::kLEConnectionLatencyMax) {
    bt_log(DEBUG, "l2cap", "conn. peripheral latency too large: %#.4x", params.max_latency());
    reject = true;
  } else if (params.supervision_timeout() < hci_spec::kLEConnectionSupervisionTimeoutMin ||
             params.supervision_timeout() > hci_spec::kLEConnectionSupervisionTimeoutMax) {
    bt_log(DEBUG, "l2cap", "conn supv. timeout outside allowed range: %#.4x",
           params.supervision_timeout());
    reject = true;
  }

  ConnectionParameterUpdateResult result = reject ? ConnectionParameterUpdateResult::kRejected
                                                  : ConnectionParameterUpdateResult::kAccepted;
  responder->Send(result);

  if (!reject) {
    if (!connection_parameter_update_callback_) {
      bt_log(DEBUG, "l2cap", "no callback set for LE Connection Parameter Update Request");
      return;
    }

    connection_parameter_update_callback_(params);
  }
}
}  // namespace bt::l2cap::internal
