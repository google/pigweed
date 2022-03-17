// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dynamic_channel_registry.h"

#include <zircon/assert.h>

#include "lib/zx/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {

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
void DynamicChannelRegistry::OpenOutbound(PSM psm, ChannelParameters params,
                                          DynamicChannelCallback open_cb) {
  const ChannelId id = FindAvailableChannelId();
  if (id == kInvalidChannelId) {
    bt_log(ERROR, "l2cap", "No dynamic channel IDs available");
    open_cb(nullptr);
    return;
  }

  auto iter = channels_.emplace(id, MakeOutbound(psm, id, params)).first;
  ActivateChannel(iter->second.get(), std::move(open_cb), /*pass_failed=*/true);
}

void DynamicChannelRegistry::CloseChannel(ChannelId local_cid, fit::closure close_cb) {
  DynamicChannel* channel = FindChannelByLocalId(local_cid);
  if (!channel) {
    close_cb();
    return;
  }

  ZX_DEBUG_ASSERT(channel->IsConnected());
  auto disconn_done_cb = [self = weak_ptr_factory_.GetWeakPtr(), close_cb = std::move(close_cb),
                          channel] {
    if (!self) {
      close_cb();
      return;
    }
    self->RemoveChannel(channel);
    close_cb();
  };
  channel->Disconnect(std::move(disconn_done_cb));
}

DynamicChannelRegistry::DynamicChannelRegistry(uint16_t max_num_channels,
                                               DynamicChannelCallback close_cb,
                                               ServiceRequestCallback service_request_cb,
                                               bool random_channel_ids)
    : max_num_channels_(max_num_channels),
      close_cb_(std::move(close_cb)),
      service_request_cb_(std::move(service_request_cb)),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(max_num_channels > 0);
  ZX_DEBUG_ASSERT(max_num_channels < 65473);
  ZX_DEBUG_ASSERT(close_cb_);
  ZX_DEBUG_ASSERT(service_request_cb_);
  if (random_channel_ids) {
    rng_ = std::default_random_engine(Random<uint64_t>());
  }
}

DynamicChannel* DynamicChannelRegistry::RequestService(PSM psm, ChannelId local_cid,
                                                       ChannelId remote_cid) {
  ZX_DEBUG_ASSERT(local_cid != kInvalidChannelId);

  auto service_info = service_request_cb_(psm);
  if (!service_info) {
    bt_log(WARN, "l2cap", "No service found for PSM %#.4x from %#.4x", psm, remote_cid);
    return nullptr;
  }

  auto iter =
      channels_
          .emplace(local_cid, MakeInbound(psm, local_cid, remote_cid, service_info->channel_params))
          .first;
  ActivateChannel(iter->second.get(), std::move(service_info->channel_cb), /*pass_failed=*/false);
  return iter->second.get();
}

ChannelId DynamicChannelRegistry::FindAvailableChannelId() {
  uint16_t offset = 0;
  if (rng_.has_value()) {
    std::uniform_int_distribution<ChannelId> distribution(0, max_num_channels_);
    offset = distribution(*rng_);
  }
  for (uint16_t i = 0; i < max_num_channels_; i++) {
    ChannelId id = kFirstDynamicChannelId + ((offset + i) % max_num_channels_);
    if (channels_.count(id) == 0) {
      return id;
    }
  }

  return kInvalidChannelId;
}

size_t DynamicChannelRegistry::AliveChannelCount() const { return channels_.size(); }

DynamicChannel* DynamicChannelRegistry::FindChannelByLocalId(ChannelId local_cid) const {
  auto iter = channels_.find(local_cid);
  if (iter == channels_.end()) {
    return nullptr;
  }
  return iter->second.get();
}

DynamicChannel* DynamicChannelRegistry::FindChannelByRemoteId(ChannelId remote_cid) const {
  for (auto& [id, channel_ptr] : channels_) {
    if (channel_ptr->remote_cid() == remote_cid) {
      return channel_ptr.get();
    }
  }
  return nullptr;
}

void DynamicChannelRegistry::ForEach(fit::function<void(DynamicChannel*)> f) const {
  for (auto iter = channels_.begin(); iter != channels_.end();) {
    // f() may remove the channel from the registry, so get next iterator to avoid invalidation.
    // Only the erased iterator is invalidated.
    auto next = std::next(iter);
    f(iter->second.get());
    iter = next;
  }
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

    bt_log(DEBUG, "l2cap", "Failed to open dynamic channel %#.4x (remote %#.4x) for PSM %#.4x",
           channel->local_cid(), channel->remote_cid(), channel->psm());

    // TODO(fxbug.dev/1059): Maybe negotiate channel parameters here? For now, just
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

}  // namespace bt::l2cap::internal
