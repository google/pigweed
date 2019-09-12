// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dynamic_channel_registry.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace l2cap {
namespace internal {

DynamicChannelRegistry::~DynamicChannelRegistry() {
  // Clean up connected channels.
  for (auto& [id, channel] : channels_) {
    if (channel->IsConnected()) {
      const auto no_op = [] {};
      channel->Disconnect(no_op);
    }
  }
}

// Run return callbacks on the L2CAP thread. LogicalLink takes care of out-of-
// thread dispatch for delivering the pointer to the channel.
void DynamicChannelRegistry::OpenOutbound(PSM psm, DynamicChannelCallback open_cb) {
  const ChannelId id = FindAvailableChannelId();
  if (id == kInvalidChannelId) {
    bt_log(ERROR, "l2cap", "No dynamic channel IDs available");
    open_cb(nullptr);
    return;
  }

  auto iter = channels_.emplace(id, MakeOutbound(psm, id)).first;
  ActivateChannel(iter->second.get(), std::move(open_cb), true);
}

void DynamicChannelRegistry::CloseChannel(ChannelId local_cid) {
  DynamicChannel* channel = FindChannel(local_cid);
  if (!channel) {
    return;
  }

  ZX_DEBUG_ASSERT(channel->IsConnected());
  auto disconn_done_cb = [self = weak_ptr_factory_.GetWeakPtr(), channel] {
    if (!self) {
      return;
    }
    self->RemoveChannel(channel);
  };
  channel->Disconnect(std::move(disconn_done_cb));
}

DynamicChannelRegistry::DynamicChannelRegistry(ChannelId largest_channel_id,
                                               DynamicChannelCallback close_cb,
                                               ServiceRequestCallback service_request_cb)
    : largest_channel_id_(largest_channel_id),
      close_cb_(std::move(close_cb)),
      service_request_cb_(std::move(service_request_cb)),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(largest_channel_id_ >= kFirstDynamicChannelId);
  ZX_DEBUG_ASSERT(close_cb_);
  ZX_DEBUG_ASSERT(service_request_cb_);
}

DynamicChannel* DynamicChannelRegistry::RequestService(PSM psm, ChannelId local_cid,
                                                       ChannelId remote_cid) {
  ZX_DEBUG_ASSERT(local_cid != kInvalidChannelId);

  DynamicChannelCallback return_chan_cb = service_request_cb_(psm);
  if (!return_chan_cb) {
    bt_log(WARN, "l2cap", "No service found for PSM %#.4x from %#.4x", psm, remote_cid);
    return nullptr;
  }

  auto iter = channels_.emplace(local_cid, MakeInbound(psm, local_cid, remote_cid)).first;
  ActivateChannel(iter->second.get(), std::move(return_chan_cb), false);
  return iter->second.get();
}

ChannelId DynamicChannelRegistry::FindAvailableChannelId() const {
  for (ChannelId id = kFirstDynamicChannelId; id != largest_channel_id_ + 1; id++) {
    if (channels_.count(id) == 0) {
      return id;
    }
  }

  return kInvalidChannelId;
}

DynamicChannel* DynamicChannelRegistry::FindChannel(ChannelId local_cid) const {
  auto iter = channels_.find(local_cid);
  if (iter == channels_.end()) {
    return nullptr;
  }
  return iter->second.get();
}

void DynamicChannelRegistry::ActivateChannel(DynamicChannel* channel,
                                             DynamicChannelCallback open_cb, bool pass_failed) {
  // It's safe to capture |this| here because the callback will be owned by the
  // DynamicChannel, which this registry owns.
  auto return_chan = [this, channel, open_cb = std::move(open_cb), pass_failed]() mutable {
    if (channel->IsOpen()) {
      open_cb(channel);
      return;
    }

    bt_log(TRACE, "l2cap", "Failed to open dynamic channel %#.4x (remote %#.4x) for PSM %#.4x",
           channel->local_cid(), channel->remote_cid(), channel->psm());

    // TODO(NET-1084): Maybe negotiate channel parameters here? For now, just
    // disconnect the channel.
    // Move the callback to the stack to prepare for channel destruction.
    auto pass_failure = [open_cb = std::move(open_cb), pass_failed] {
      if (pass_failed) {
        open_cb(nullptr);
      }
    };

    // This lambda is owned by the channel, so captures are no longer valid
    // after this call.
    auto disconn_done_cb = [self = weak_ptr_factory_.GetWeakPtr(), channel] {
      if (!self) {
        return;
      }
      self->RemoveChannel(channel);
    };
    channel->Disconnect(std::move(disconn_done_cb));

    pass_failure();
  };

  channel->Open(std::move(return_chan));
}

void DynamicChannelRegistry::OnChannelDisconnected(DynamicChannel* channel) {
  if (channel->opened()) {
    close_cb_(channel);
  }
  RemoveChannel(channel);
}

void DynamicChannelRegistry::RemoveChannel(DynamicChannel* channel) {
  ZX_DEBUG_ASSERT(channel);
  ZX_DEBUG_ASSERT(!channel->IsConnected());

  auto iter = channels_.find(channel->local_cid());
  if (iter == channels_.end()) {
    return;
  }

  if (channel != iter->second.get()) {
    return;
  }

  channels_.erase(iter);
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
