// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logical_link.h"

#include <zircon/assert.h>

#include <functional>

#include "bredr_dynamic_channel.h"
#include "bredr_signaling_channel.h"
#include "channel.h"
#include "le_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/run_or_post.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

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
fbl::RefPtr<LogicalLink> LogicalLink::New(
    hci::ConnectionHandle handle, hci::Connection::LinkType type, hci::Connection::Role role,
    async_dispatcher_t* dispatcher, size_t max_acl_payload_size,
    SendPacketsCallback send_packets_cb, DropQueuedAclCallback drop_queued_acl_cb,
    QueryServiceCallback query_service_cb) {
  auto ll = fbl::AdoptRef(new LogicalLink(handle, type, role, dispatcher, max_acl_payload_size,
                                          std::move(send_packets_cb), std::move(drop_queued_acl_cb),
                                          std::move(query_service_cb)));
  ll->Initialize();
  return ll;
}

LogicalLink::LogicalLink(hci::ConnectionHandle handle, hci::Connection::LinkType type,
                         hci::Connection::Role role, async_dispatcher_t* dispatcher,
                         size_t max_acl_payload_size, SendPacketsCallback send_packets_cb,
                         DropQueuedAclCallback drop_queued_acl_cb,
                         QueryServiceCallback query_service_cb)
    : dispatcher_(dispatcher),
      handle_(handle),
      type_(type),
      role_(role),
      closed_(false),
      fragmenter_(handle, max_acl_payload_size),
      send_packets_cb_(std::move(send_packets_cb)),
      drop_queued_acl_cb_(std::move(drop_queued_acl_cb)),
      query_service_cb_(std::move(query_service_cb)),
      weak_ptr_factory_(this) {
  ZX_ASSERT(dispatcher_);
  ZX_ASSERT(type_ == hci::Connection::LinkType::kLE || type_ == hci::Connection::LinkType::kACL);
  ZX_ASSERT(send_packets_cb_);
  ZX_ASSERT(drop_queued_acl_cb_);
  ZX_ASSERT(query_service_cb_);
}

void LogicalLink::Initialize() {
  ZX_DEBUG_ASSERT(!signaling_channel_);
  ZX_DEBUG_ASSERT(!dynamic_registry_);

  // Set up the signaling channel and dynamic channels.
  if (type_ == hci::Connection::LinkType::kLE) {
    signaling_channel_ =
        std::make_unique<LESignalingChannel>(OpenFixedChannel(kLESignalingChannelId), role_);
    // TODO(armansito): Initialize LE registry when it exists.
  } else {
    signaling_channel_ =
        std::make_unique<BrEdrSignalingChannel>(OpenFixedChannel(kSignalingChannelId), role_);
    dynamic_registry_ = std::make_unique<BrEdrDynamicChannelRegistry>(
        signaling_channel_.get(), fit::bind_member(this, &LogicalLink::OnChannelDisconnectRequest),
        fit::bind_member(this, &LogicalLink::OnServiceRequest));

    SendFixedChannelsSupportedInformationRequest();
  }
}

LogicalLink::~LogicalLink() { ZX_DEBUG_ASSERT(closed_); }

fbl::RefPtr<Channel> LogicalLink::OpenFixedChannel(ChannelId id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!closed_);

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

  auto chan = ChannelImpl::CreateFixedChannel(id, fbl::RefPtr(this));

  auto pp_iter = pending_pdus_.find(id);
  if (pp_iter != pending_pdus_.end()) {
    for (auto &pdu : pp_iter->second) {
      chan->HandleRxPdu(std::move(pdu));
    }
    pending_pdus_.erase(pp_iter);
  }

  channels_[id] = chan;

  return chan;
}

void LogicalLink::OpenChannel(PSM psm, ChannelParameters params, ChannelCallback callback,
                              async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!closed_);

  // TODO(NET-1437): Implement channels for LE credit-based connections
  if (type_ == hci::Connection::LinkType::kLE) {
    bt_log(WARN, "l2cap", "not opening LE channel for PSM %.4x", psm);
    CompleteDynamicOpen(nullptr, std::move(callback), dispatcher);
    return;
  }

  auto create_channel = [this, cb = std::move(callback),
                         dispatcher](const DynamicChannel* dyn_chan) mutable {
    CompleteDynamicOpen(dyn_chan, std::move(cb), dispatcher);
  };
  dynamic_registry_->OpenOutbound(psm, params, std::move(create_channel));
}

void LogicalLink::HandleRxPacket(hci::ACLDataPacketPtr packet) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!recombiner_.ready());
  ZX_DEBUG_ASSERT(packet);
  ZX_DEBUG_ASSERT(!closed_);

  if (!recombiner_.AddFragment(std::move(packet))) {
    bt_log(TRACE, "l2cap", "ACL data packet rejected (handle: %#.4x)", handle_);
    // TODO(armansito): This indicates that this connection is not reliable.
    // This needs to notify the channels of this state.
    return;
  }

  // |recombiner_| should have taken ownership of |packet|.
  ZX_DEBUG_ASSERT(!packet);
  ZX_DEBUG_ASSERT(!recombiner_.empty());

  // Wait for continuation fragments if a partial fragment was received.
  if (!recombiner_.ready())
    return;

  PDU pdu;
  recombiner_.Release(&pdu);

  ZX_DEBUG_ASSERT(pdu.is_valid());

  uint16_t channel_id = pdu.channel_id();
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
    pp_iter->second.emplace_back(std::move(pdu));

    bt_log(SPEW, "l2cap", "PDU buffered (channel: %#.4x, ll: %#.4x)", channel_id, handle_);
    return;
  }

  iter->second->HandleRxPdu(std::move(pdu));
}

void LogicalLink::UpgradeSecurity(sm::SecurityLevel level, sm::StatusCallback callback,
                                  async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(security_callback_);
  ZX_DEBUG_ASSERT(dispatcher);

  if (closed_) {
    bt_log(TRACE, "l2cap", "Ignoring security request on closed link");
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

  bt_log(TRACE, "l2cap", "Security upgrade requested (level = %s)", sm::LevelToString(level));
  async::PostTask(security_dispatcher_, [handle = handle_, level, f = security_callback_.share(),
                                         status_cb = std::move(status_cb)]() mutable {
    f(handle, level, std::move(status_cb));
  });
}

void LogicalLink::AssignSecurityProperties(const sm::SecurityProperties& security) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (closed_) {
    bt_log(TRACE, "l2cap", "Ignoring security request on closed link");
    return;
  }

  bt_log(TRACE, "l2cap", "Link security updated (handle: %#.4x): %s", handle_,
         security.ToString().c_str());

  std::lock_guard<std::mutex> lock(mtx_);
  security_ = security;
}

void LogicalLink::SendFrame(ChannelId id, const ByteBuffer& payload,
                            FrameCheckSequenceOption fcs_option) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (closed_) {
    bt_log(TRACE, "l2cap", "Drop out-bound packet on closed link");
    return;
  }

  // Copy payload into L2CAP frame fragments, sized for the HCI data transport.
  PDU pdu = fragmenter_.BuildFrame(id, payload, fcs_option);
  auto fragments = pdu.ReleaseFragments();

  ZX_ASSERT(!fragments.is_empty());
  send_packets_cb_(std::move(fragments), id);
}

void LogicalLink::set_error_callback(fit::closure callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(static_cast<bool>(callback) == static_cast<bool>(dispatcher));

  link_error_cb_ = std::move(callback);
  link_error_dispatcher_ = dispatcher;
}

void LogicalLink::set_security_upgrade_callback(SecurityUpgradeCallback callback,
                                                async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(static_cast<bool>(callback) == static_cast<bool>(dispatcher));

  security_callback_ = std::move(callback);
  security_dispatcher_ = dispatcher;
}

LESignalingChannel* LogicalLink::le_signaling_channel() const {
  return (type_ == hci::Connection::LinkType::kLE)
             ? static_cast<LESignalingChannel*>(signaling_channel_.get())
             : nullptr;
}

bool LogicalLink::AllowsFixedChannel(ChannelId id) {
  return (type_ == hci::Connection::LinkType::kLE) ? IsValidLEFixedChannel(id)
                                                   : IsValidBREDRFixedChannel(id);
}

void LogicalLink::RemoveChannel(Channel* chan) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(chan);

  if (closed_) {
    bt_log(TRACE, "l2cap", "Ignore RemoveChannel() on closed link");
    return;
  }

  const ChannelId id = chan->id();
  auto iter = channels_.find(id);
  if (iter == channels_.end())
    return;

  // Ignore if the found channel doesn't match the requested one (even though
  // their IDs are the same).
  if (iter->second.get() != chan)
    return;

  pending_pdus_.erase(id);
  channels_.erase(iter);

  // Drop stale packets queued for this channel.
  hci::ACLPacketPredicate predicate = [this, id](const auto& packet, l2cap::ChannelId channel_id) {
    return packet->connection_handle() == handle_ && id == channel_id;
  };
  drop_queued_acl_cb_(std::move(predicate));

  // Disconnect the channel if it's a dynamic channel. This path is for local-
  // initiated closures and does not invoke callbacks back to the channel user.
  // TODO(armansito): Change this if statement into an assert when a registry
  // gets created for LE channels.
  if (dynamic_registry_) {
    dynamic_registry_->CloseChannel(id);
  }
}

void LogicalLink::SignalError() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  if (closed_) {
    bt_log(TRACE, "l2cap", "Ignore SignalError() on closed link");
    return;
  }

  if (link_error_cb_) {
    async::PostTask(link_error_dispatcher_, std::move(link_error_cb_));
    link_error_dispatcher_ = nullptr;
  }
}

void LogicalLink::Close() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!closed_);

  closed_ = true;

  auto channels = std::move(channels_);
  for (auto& iter : channels) {
    iter.second->OnClosed();
  }
}

std::optional<DynamicChannelRegistry::ServiceInfo> LogicalLink::OnServiceRequest(PSM psm) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!closed_);

  // Query upper layer for a service handler attached to this PSM.
  auto result = query_service_cb_(handle_, psm);
  if (!result) {
    return std::nullopt;
  }

  auto channel_cb =
      [this, chan_cb = std::move(result->channel_cb)](const DynamicChannel* dyn_chan) mutable {
        CompleteDynamicOpen(dyn_chan, std::move(chan_cb), nullptr);
      };
  return DynamicChannelRegistry::ServiceInfo(result->channel_params, std::move(channel_cb));
}

void LogicalLink::OnChannelDisconnectRequest(const DynamicChannel* dyn_chan) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(dyn_chan);
  ZX_DEBUG_ASSERT(!closed_);

  auto iter = channels_.find(dyn_chan->local_cid());
  if (iter == channels_.end()) {
    bt_log(WARN, "l2cap", "No ChannelImpl found for closing dynamic channel %#.4x",
           dyn_chan->local_cid());
    return;
  }

  fbl::RefPtr<ChannelImpl> channel = std::move(iter->second);
  ZX_DEBUG_ASSERT(channel->remote_id() == dyn_chan->remote_cid());
  channels_.erase(iter);

  hci::ACLPacketPredicate predicate = [this, id = channel->id()](const auto& packet,
                                                                 l2cap::ChannelId channel_id) {
    return packet->connection_handle() == handle_ && id == channel_id;
  };
  drop_queued_acl_cb_(std::move(predicate));

  // Signal closure because this is a remote disconnection.
  channel->OnClosed();
}

void LogicalLink::CompleteDynamicOpen(const DynamicChannel* dyn_chan, ChannelCallback open_cb,
                                      async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(!closed_);

  if (!dyn_chan) {
    RunOrPost(std::bind(std::move(open_cb), nullptr), dispatcher);
    return;
  }

  const ChannelId local_cid = dyn_chan->local_cid();
  const ChannelId remote_cid = dyn_chan->remote_cid();
  bt_log(TRACE, "l2cap", "Link %#.4x: Channel opened with ID %#.4x (remote ID %#.4x)", handle_,
         local_cid, remote_cid);

  auto chan =
      ChannelImpl::CreateDynamicChannel(local_cid, remote_cid, fbl::RefPtr(this), dyn_chan->info());
  channels_[local_cid] = chan;
  RunOrPost(std::bind(std::move(open_cb), std::move(chan)), dispatcher);
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

  bt_log(SPEW, "l2cap", "Sent Fixed Channels Supported Information Request");
}

void LogicalLink::OnRxFixedChannelsSupportedInfoRsp(
    const BrEdrCommandHandler::InformationResponse& rsp) {
  ZX_ASSERT(rsp.type() == InformationType::kFixedChannelsSupported);

  if (rsp.result() == InformationResult::kNotSupported) {
    bt_log(SPEW, "l2cap",
           "Received Fixed Channels Supported Information Response (result: Not Supported)");
    return;
  }

  bt_log(SPEW, "l2cap", "Received Fixed Channels Supported Information Response (mask: %#016lx)",
         rsp.fixed_channels());
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
