// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logical_link.h"

#include <lib/fpromise/bridge.h>
#include <zircon/assert.h>

#include <functional>

#include "bredr_dynamic_channel.h"
#include "bredr_signaling_channel.h"
#include "channel.h"
#include "fbl/ref_ptr.h"
#include "le_signaling_channel.h"
#include "lib/fpromise/promise.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"

namespace bt::l2cap::internal {
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

// static
fbl::RefPtr<LogicalLink> LogicalLink::New(hci_spec::ConnectionHandle handle, bt::LinkType type,
                                          hci::Connection::Role role, fpromise::executor* executor,
                                          size_t max_acl_payload_size,
                                          QueryServiceCallback query_service_cb,
                                          hci::AclDataChannel* acl_data_channel,
                                          bool random_channel_ids) {
  auto ll = fbl::AdoptRef(new LogicalLink(handle, type, role, executor, max_acl_payload_size,
                                          std::move(query_service_cb), acl_data_channel));
  ll->Initialize(random_channel_ids);
  return ll;
}

LogicalLink::LogicalLink(hci_spec::ConnectionHandle handle, bt::LinkType type,
                         hci::Connection::Role role, fpromise::executor* executor,
                         size_t max_acl_payload_size, QueryServiceCallback query_service_cb,
                         hci::AclDataChannel* acl_data_channel)
    : handle_(handle),
      type_(type),
      role_(role),
      flush_timeout_(zx::duration::infinite(), /*convert=*/[](auto f) { return f.to_msecs(); }),
      closed_(false),
      fragmenter_(handle, max_acl_payload_size),
      recombiner_(handle),
      acl_data_channel_(acl_data_channel),
      query_service_cb_(std::move(query_service_cb)),
      executor_(executor),
      weak_ptr_factory_(this) {
  ZX_ASSERT(type_ == bt::LinkType::kLE || type_ == bt::LinkType::kACL);
  ZX_ASSERT(executor_);
  ZX_ASSERT(acl_data_channel_);
  ZX_ASSERT(query_service_cb_);
}

void LogicalLink::Initialize(bool randomize_channel_ids) {
  ZX_DEBUG_ASSERT(!signaling_channel_);
  ZX_DEBUG_ASSERT(!dynamic_registry_);

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
        signaling_channel_.get(), fit::bind_member(this, &LogicalLink::OnChannelDisconnectRequest),
        fit::bind_member(this, &LogicalLink::OnServiceRequest), randomize_channel_ids);

    SendFixedChannelsSupportedInformationRequest();
  }
}

LogicalLink::~LogicalLink() {
  bt_log(DEBUG, "l2cap", "LogicalLink destroyed (handle: %#.4x)", handle_);
  ZX_ASSERT(closed_);
}

fbl::RefPtr<Channel> LogicalLink::OpenFixedChannel(ChannelId id) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!closed_);

  TRACE_DURATION("bluetooth", "LogicalLink::OpenFixedChannel", "handle", handle_, "channel id", id);

  // We currently only support the pre-defined fixed-channels.
  if (!AllowsFixedChannel(id)) {
    bt_log(ERROR, "l2cap", "cannot open fixed channel with id %#.4x", id);
    return nullptr;
  }

  auto iter = channels_.find(id);
  if (iter != channels_.end()) {
    bt_log(ERROR, "l2cap", "channel is already open! (id: %#.4x, handle: %#.4x)", id, handle_);
    return nullptr;
  }

  auto chan = ChannelImpl::CreateFixedChannel(id, GetWeakPtr());

  auto pp_iter = pending_pdus_.find(id);
  if (pp_iter != pending_pdus_.end()) {
    for (auto& pdu : pp_iter->second) {
      TRACE_FLOW_END("bluetooth", "LogicalLink::HandleRxPacket queued", pdu.trace_id());
      chan->HandleRxPdu(std::move(pdu));
    }
    pending_pdus_.erase(pp_iter);
  }

  channels_[id] = chan;

  if (inspect_properties_.channels_node) {
    chan->AttachInspect(inspect_properties_.channels_node,
                        inspect_properties_.channels_node.UniqueName(kInspectChannelNodePrefix));
  }

  return chan;
}

void LogicalLink::OpenChannel(PSM psm, ChannelParameters params, ChannelCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!closed_);

  // TODO(fxbug.dev/968): Implement channels for LE credit-based connections
  if (type_ == bt::LinkType::kLE) {
    bt_log(WARN, "l2cap", "not opening LE channel for PSM %.4x", psm);
    CompleteDynamicOpen(nullptr, std::move(callback));
    return;
  }

  auto create_channel = [this, cb = std::move(callback)](const DynamicChannel* dyn_chan) mutable {
    CompleteDynamicOpen(dyn_chan, std::move(cb));
  };
  dynamic_registry_->OpenOutbound(psm, params, std::move(create_channel));
}

void LogicalLink::HandleRxPacket(hci::ACLDataPacketPtr packet) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(packet);
  ZX_DEBUG_ASSERT(!closed_);

  TRACE_DURATION("bluetooth", "LogicalLink::HandleRxPacket", "handle", handle_);

  // We do not support the Connectionless data channel, and the active broadcast flag can
  // only be used on the connectionless channel.  Drop packets that are broadcast.
  if (packet->broadcast_flag() == hci_spec::ACLBroadcastFlag::kActiveSlaveBroadcast) {
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

  ZX_DEBUG_ASSERT(result.pdu->is_valid());

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

void LogicalLink::UpgradeSecurity(sm::SecurityLevel level, sm::StatusCallback callback,
                                  async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(security_callback_);
  ZX_DEBUG_ASSERT(dispatcher);

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignoring security request on closed link");
    return;
  }

  auto status_cb = [dispatcher, f = std::move(callback)](sm::Status status) mutable {
    async::PostTask(dispatcher, [f = std::move(f), status] { f(status); });
  };

  // Report success If the link already has the expected security level.
  if (level <= security().level()) {
    status_cb(sm::Status());
    return;
  }

  bt_log(DEBUG, "l2cap", "Security upgrade requested (level = %s)", sm::LevelToString(level));
  security_callback_(handle_, level, std::move(status_cb));
}

void LogicalLink::AssignSecurityProperties(const sm::SecurityProperties& security) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

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
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Drop out-bound packet on closed link");
    return;
  }

  // Copy payload into L2CAP frame fragments, sized for the HCI data transport.
  PDU pdu = fragmenter_.BuildFrame(id, payload, fcs_option, flushable);
  auto fragments = pdu.ReleaseFragments();

  ZX_ASSERT(!fragments.is_empty());
  acl_data_channel_->SendPackets(std::move(fragments), id, ChannelPriority(id));
}

void LogicalLink::set_error_callback(fit::closure callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  link_error_cb_ = std::move(callback);
}

void LogicalLink::set_security_upgrade_callback(SecurityUpgradeCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  security_callback_ = std::move(callback);
}

void LogicalLink::set_connection_parameter_update_callback(
    LEConnectionParameterUpdateCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  connection_parameter_update_callback_ = std::move(callback);
}

LESignalingChannel* LogicalLink::le_signaling_channel() const {
  return (type_ == bt::LinkType::kLE) ? static_cast<LESignalingChannel*>(signaling_channel_.get())
                                      : nullptr;
}

bool LogicalLink::AllowsFixedChannel(ChannelId id) {
  return (type_ == bt::LinkType::kLE) ? IsValidLEFixedChannel(id) : IsValidBREDRFixedChannel(id);
}

fpromise::promise<> LogicalLink::RemoveChannel(Channel* chan) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(chan);

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignore RemoveChannel() on closed link");
    return fpromise::make_ok_promise();
  }

  const ChannelId id = chan->id();
  auto iter = channels_.find(id);
  if (iter == channels_.end()) {
    return fpromise::make_ok_promise();
  }

  // Ignore if the found channel doesn't match the requested one (even though
  // their IDs are the same).
  if (iter->second.get() != chan) {
    return fpromise::make_ok_promise();
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
    // This bridge can and probably should be pushed farther down through DynamicChannelRegistry,
    // BrEdrCommandHandler, SignalingChannel, and ACLDataChannel all the way up to the underlying
    // async::Wait.
    //
    // Note that completing a bridge seems to always produce a new task for fulfilling dependent
    // consumer promises, which for fpromise::executor means posting the consumer onto its
    // dispatcher.
    fpromise::bridge<> bridge;
    dynamic_registry_->CloseChannel(id, bridge.completer.bind());

    // promise_or converts completer abandonment (e.g. destroyed in an early error branch) to a
    // promise that yields a result.
    return bridge.consumer.promise_or(fpromise::ok());
  }
  return fpromise::make_ok_promise();
}

void LogicalLink::SignalError() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  if (closed_) {
    bt_log(DEBUG, "l2cap", "Ignore SignalError() on closed link");
    return;
  }

  bt_log(INFO, "l2cap", "Upper layer error on link %#.4x; closing all channels", handle());

  std::vector<fpromise::promise<>> channel_closures;
  for (auto channel_iter = channels_.begin(); channel_iter != channels_.end();) {
    auto& [id, channel] = *channel_iter++;

    // Do not close the signaling channel, as it is used to close the dynamic channels.
    if (id == kSignalingChannelId || id == kLESignalingChannelId) {
      continue;
    }

    // Signal the channel, as it did not request the closure.
    channel->OnClosed();

    // This erases from |channel_| and invalidates any iterator pointing to |channel|.
    channel_closures.emplace_back(RemoveChannel(channel.get()));
  }

  fpromise::promise<> error_signaler =
      fpromise::join_promise_vector(std::move(channel_closures))
          .and_then([this](const std::vector<fpromise::result<>>& results) {
            bt_log(TRACE, "l2cap", "Channels on link %#.4x closed; passing error to lower layer",
                   handle());

            // Invoking this may destroy this LogicalLink.
            link_error_cb_();

            // Link is expected to be closed by its owner.
            return fpromise::ok();
          });
  executor_->schedule_task(std::move(error_signaler));
}

void LogicalLink::Close() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!closed_);

  closed_ = true;

  for (auto& iter : channels_) {
    iter.second->OnClosed();
  }
  channels_.clear();
}

std::optional<DynamicChannelRegistry::ServiceInfo> LogicalLink::OnServiceRequest(PSM psm) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!closed_);

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
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(dyn_chan);
  ZX_DEBUG_ASSERT(!closed_);

  auto iter = channels_.find(dyn_chan->local_cid());
  if (iter == channels_.end()) {
    bt_log(WARN, "l2cap", "No ChannelImpl found for closing dynamic channel %#.4x",
           dyn_chan->local_cid());
    return;
  }

  auto channel = iter->second;
  ZX_DEBUG_ASSERT(channel->remote_id() == dyn_chan->remote_cid());

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
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!closed_);

  if (!dyn_chan) {
    open_cb(nullptr);
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

  auto chan = ChannelImpl::CreateDynamicChannel(local_cid, remote_cid, GetWeakPtr(), chan_info);
  channels_[local_cid] = chan;

  if (inspect_properties_.channels_node) {
    chan->AttachInspect(inspect_properties_.channels_node,
                        inspect_properties_.channels_node.UniqueName(kInspectChannelNodePrefix));
  }

  // If a flush timeout was requested for this channel, try to set it before returning the channel
  // to the client to ensure outbound PDUs have correct flushable flag.
  if (preferred_flush_timeout.has_value()) {
    chan->SetBrEdrAutomaticFlushTimeout(
        preferred_flush_timeout.value(),
        [cb = std::move(open_cb), chan](auto /*result*/) { cb(chan); });
    return;
  }

  open_cb(std::move(chan));
}

void LogicalLink::SendFixedChannelsSupportedInformationRequest() {
  ZX_ASSERT(signaling_channel_);

  BrEdrCommandHandler cmd_handler(signaling_channel_.get());
  if (!cmd_handler.SendInformationRequest(InformationType::kFixedChannelsSupported,
                                          [self = GetWeakPtr()](auto& rsp) {
                                            if (self) {
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
  ZX_ASSERT(signaling_channel_);
  ZX_ASSERT(type_ == bt::LinkType::kLE);
  ZX_ASSERT(role_ == hci::Connection::Role::kSlave);

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

void LogicalLink::RequestAclPriority(Channel* channel, hci::AclPriority priority,
                                     fit::callback<void(fpromise::result<>)> callback) {
  ZX_ASSERT(channel);
  auto iter = channels_.find(channel->id());
  ZX_ASSERT(iter != channels_.end());
  auto chan_ref = iter->second;

  // Store ref pointer to channel to ensure it stays alive until this request is handled.
  pending_acl_requests_.push(PendingAclRequest{chan_ref, priority, std::move(callback)});
  if (pending_acl_requests_.size() == 1) {
    HandleNextAclPriorityRequest();
  }
}

void LogicalLink::SetBrEdrAutomaticFlushTimeout(
    zx::duration flush_timeout,
    fit::callback<void(fpromise::result<void, hci_spec::StatusCode>)> callback) {
  if (type_ != bt::LinkType::kACL) {
    bt_log(ERROR, "l2cap", "attempt to set flush timeout on non-ACL logical link");
    callback(fpromise::error(hci_spec::StatusCode::kInvalidHCICommandParameters));
    return;
  }

  auto callback_wrapper = [self = GetWeakPtr(), flush_timeout,
                           cb = std::move(callback)](auto result) mutable {
    if (self && result.is_ok()) {
      self->flush_timeout_.Set(flush_timeout);
    }
    cb(result);
  };

  acl_data_channel_->SetBrEdrAutomaticFlushTimeout(flush_timeout, handle_,
                                                   std::move(callback_wrapper));
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
  ZX_ASSERT(request.channel);
  ZX_ASSERT(request.callback);

  // Prevent closed channels with queued requests from upgrading channel priority.
  if (channels_.find(request.channel->id()) == channels_.end() &&
      request.priority != hci::AclPriority::kNormal) {
    request.callback(fpromise::error());
    pending_acl_requests_.pop();
    HandleNextAclPriorityRequest();
    return;
  }

  // Skip sending command if desired priority is already set. Do this here instead of Channel in
  // case Channel queues up multiple requests.
  if (request.channel->requested_acl_priority() == request.priority) {
    request.callback(fpromise::ok());
    pending_acl_requests_.pop();
    HandleNextAclPriorityRequest();
    return;
  }

  for (auto& [chan_id, chan] : channels_) {
    if (chan.get() == request.channel.get()) {
      continue;
    }

    if (chan->requested_acl_priority() != hci::AclPriority::kNormal) {
      // If the request returns priority to normal but a different channel still requires high
      // priority OR if another channel already is using the requested priority, skip sending
      // command and just report success.
      if (request.priority == hci::AclPriority::kNormal ||
          request.priority == chan->requested_acl_priority()) {
        request.callback(fpromise::ok());
        break;
      }

      // If the request tries to upgrade priority but it conflicts with another channel's priority
      // (e.g. sink vs. source), report an error.
      if (request.priority != hci::AclPriority::kNormal &&
          request.priority != chan->requested_acl_priority()) {
        request.callback(fpromise::error());
        break;
      }
    }
  }

  if (!request.callback) {
    pending_acl_requests_.pop();
    HandleNextAclPriorityRequest();
    return;
  }

  auto cb_wrapper = [self = GetWeakPtr(), cb = std::move(request.callback)](auto result) mutable {
    if (!self) {
      return;
    }
    cb(result);
    self->pending_acl_requests_.pop();
    self->HandleNextAclPriorityRequest();
  };

  acl_data_channel_->RequestAclPriority(request.priority, handle_, std::move(cb_wrapper));
}

void LogicalLink::ServeConnectionParameterUpdateRequest() {
  ZX_ASSERT(signaling_channel_);
  ZX_ASSERT(type_ == bt::LinkType::kLE);

  LowEnergyCommandHandler cmd_handler(signaling_channel_.get());
  cmd_handler.ServeConnectionParameterUpdateRequest(
      fit::bind_member(this, &LogicalLink::OnRxConnectionParameterUpdateRequest));
}

void LogicalLink::OnRxConnectionParameterUpdateRequest(
    uint16_t interval_min, uint16_t interval_max, uint16_t slave_latency,
    uint16_t timeout_multiplier,
    LowEnergyCommandHandler::ConnectionParameterUpdateResponder* responder) {
  // Only a LE slave can send this command. "If an LE slave Host receives a
  // Connection Parameter Update Request packet it shall respond with a Command
  // Reject Packet [...]" (v5.0, Vol 3, Part A, Section 4.20).
  if (role_ == hci::Connection::Role::kSlave) {
    bt_log(DEBUG, "l2cap", "rejecting conn. param. update request from master");
    responder->RejectNotUnderstood();
    return;
  }

  // Reject the connection parameters if they are outside the ranges allowed by
  // the HCI specification (see HCI_LE_Connection_Update command v5.0, Vol 2,
  // Part E, Section 7.8.18).
  bool reject = false;

  hci_spec::LEPreferredConnectionParameters params(interval_min, interval_max, slave_latency,
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
    bt_log(DEBUG, "l2cap", "conn. slave latency too large: %#.4x", params.max_latency());
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
