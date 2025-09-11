// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"

#include <mutex>
#include <optional>

#include "pw_bluetooth_proxy/internal/acl_data_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_containers/algorithm.h"
#include "pw_containers/flat_map.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

L2capChannelManager::L2capChannelManager(AclDataChannel& acl_data_channel)
    : acl_data_channel_(acl_data_channel),
      lrd_channel_(channels_.end()),
      round_robin_terminus_(channels_.end()) {}

void L2capChannelManager::RegisterChannel(L2capChannel& channel) {
  std::lock_guard lock(channels_mutex_);
  RegisterChannelLocked(channel);
}

void L2capChannelManager::RegisterChannelLocked(L2capChannel& channel) {
  // Insert new channels before `lrd_channel_`.
  IntrusiveForwardList<L2capChannel>::iterator before_it =
      channels_.before_begin();
  for (auto it = channels_.begin(); it != lrd_channel_; ++it) {
    ++before_it;
  }
  channels_.insert_after(before_it, channel);
  if (lrd_channel_ == channels_.end()) {
    lrd_channel_ = channels_.begin();
  }
}

void L2capChannelManager::DeregisterChannelLocked(L2capChannel& channel) {
  if (&channel == &(*lrd_channel_)) {
    Advance(lrd_channel_);
  }
  if (&channel == &(*round_robin_terminus_)) {
    Advance(round_robin_terminus_);
  }

  // Channel will only be removed once, but DeregisterChannel() may be called
  // multiple times on the same channel so it's ok for this to return false.
  channels_.remove(channel);

  // If `channel` was the only element in `channels_`, advancing channels just
  // wrapped them back on itself, so we reset it here.
  if (channels_.empty()) {
    lrd_channel_ = channels_.end();
    round_robin_terminus_ = channels_.end();
  }
}

void L2capChannelManager::DeregisterChannel(L2capChannel& channel) {
  std::lock_guard lock(channels_mutex_);
  DeregisterChannelLocked(channel);
}

void L2capChannelManager::MoveChannelRegistration(L2capChannel& from,
                                                  L2capChannel& to) {
  std::lock_guard lock(channels_mutex_);
  DeregisterChannelLocked(from);
  RegisterChannelLocked(to);
}

void L2capChannelManager::DeregisterAndCloseChannels(L2capChannelEvent event) {
  std::lock_guard lock(channels_mutex_);
  while (!channels_.empty()) {
    L2capChannel& front = channels_.front();
    channels_.pop_front();
    front.InternalClose(event);
  }
  lrd_channel_ = channels_.end();
  round_robin_terminus_ = channels_.end();
}

pw::Result<H4PacketWithH4> L2capChannelManager::GetAclH4Packet(uint16_t size) {
  if (size > GetH4BuffSize()) {
    PW_LOG_ERROR(
        "Requested packet is too large for H4 buffer. So will not send.");
    return pw::Status::InvalidArgument();
  }

  std::optional<span<uint8_t>> h4_buff = h4_storage_.ReserveH4Buff();
  if (!h4_buff) {
    PW_LOG_WARN("No H4 buffers available.");
    return pw::Status::Unavailable();
  }

  H4PacketWithH4 h4_packet(span(h4_buff->data(), size),
                           /*release_fn=*/[this](const uint8_t* buffer) {
                             this->h4_storage_.ReleaseH4Buff(buffer);
                             // TODO: https://pwbug.dev/421249712 - Only report
                             // if we were previously out of buffers.
                             ForceDrainChannelQueues();
                           });
  h4_packet.SetH4Type(emboss::H4PacketType::ACL_DATA);

  return h4_packet;
}

uint16_t L2capChannelManager::GetH4BuffSize() const {
  return H4Storage::GetH4BuffSize();
}

void L2capChannelManager::ForceDrainChannelQueues() {
  ReportNewTxPacketsOrCredits();
  DrainChannelQueuesIfNewTx();
}

void L2capChannelManager::ReportNewTxPacketsOrCredits() {
  std::lock_guard lock(drain_status_mutex_);
  drain_needed_ = true;
}

void L2capChannelManager::DrainChannelQueuesIfNewTx() {
  {
    std::lock_guard lock(drain_status_mutex_);
    if (drain_running_) {
      // Drain is already in progress
      return;
    }
    if (!drain_needed_) {
      return;
    }
    drain_running_ = true;
    drain_needed_ = false;
  }

  pw::containers::FlatMap<AclTransportType,
                          std::optional<AclDataChannel::SendCredit>,
                          2>
      credits({{{AclTransportType::kBrEdr, {}}, {AclTransportType::kLe, {}}}});

  for (;;) {
    std::optional<H4PacketWithH4> packet;
    std::optional<AclDataChannel::SendCredit> packet_credit{};

    // Attempt to reserve credits. This may be our first pass or we may have
    // used one on last pass.
    // We reserve credits upfront so that acl_data_channel_'s credits mutex lock
    // is not acquired inside the channels_mutex_ lock below.
    // SendCredit is RAII object, any held credits will be returned when
    // function exits.
    for (auto& [transport, credit] : credits) {
      if (!credit.has_value()) {
        credits.at(transport) = acl_data_channel_.ReserveSendCredit(transport);
      }
    }

    {
      std::lock_guard lock(channels_mutex_);

      // Container is empty, nothing to do.
      if (lrd_channel_ == channels_.end()) {
        // No channels, no drain needed.
        std::lock_guard drain_lock(drain_status_mutex_);
        drain_needed_ = false;
        drain_running_ = false;
        return;
      }

      // If we haven't set terminus yet, just use end of the container.
      if (round_robin_terminus_ == channels_.end()) {
        round_robin_terminus_ = lrd_channel_;
      }

      // If we have a credit for the channel's type, attempt to dequeue
      // packet from channel.
      std::optional<AclDataChannel::SendCredit>& current_credit =
          credits.at(lrd_channel_->transport());
      if (current_credit.has_value()) {
        packet = lrd_channel_->DequeuePacket();
        if (packet) {
          // We were able to dequeue a packet. So also take the current credit
          // to use when sending the packet below.
          packet_credit = std::exchange(current_credit, std::nullopt);
        }
      }

      // Always advance so next dequeue is from next channel.
      Advance(lrd_channel_);

      if (packet) {
        // Round robin will continue until we have done a full loop with no
        // packets dequeued.
        round_robin_terminus_ = lrd_channel_;
      }
    }  // std::lock_guard lock(channels_mutex_);

    if (packet) {
      // A packet with a credit was found inside the lock. Send while unlocked
      // with that credit.
      // This will trigger another Drain when `packet` is released. This could
      // happen during the SendAcl call, but that is fine because `lrd_channel_`
      // and `round_robin_terminus_` are always adjusted inside the lock. So
      // each Drain frame's loop will just resume where last one left off and
      // continue until that it has found no channels with something to dequeue.
      PW_CHECK_OK(acl_data_channel_.SendAcl(
          std::move(*packet),
          std::move(std::exchange(packet_credit, std::nullopt).value())));
      continue;
    }
    {
      std::lock_guard channels_lock(channels_mutex_);
      std::lock_guard drain_lock(drain_status_mutex_);

      if (drain_needed_) {
        // Additional tx packets or resources have arrived, so reset terminus so
        // we attempt to dequeue all the channels again.
        round_robin_terminus_ = lrd_channel_;
        drain_needed_ = false;
        continue;
      }

      if (lrd_channel_ != round_robin_terminus_) {
        // Continue until last drained channel is terminus, meaning we have
        // failed to dequeue from all channels (so nothing left to send).
        continue;
      }

      drain_running_ = false;
      return;
    }
  }  // lock_guard: channels_mutex_, drain_status_mutex_
}

std::optional<LockedL2capChannel> L2capChannelManager::FindChannelByLocalCid(
    uint16_t connection_handle, uint16_t local_cid) PW_NO_LOCK_SAFETY_ANALYSIS {
  // Lock annotations don't work with unique_lock
  std::unique_lock lock(channels_mutex_);
  L2capChannel* channel =
      FindChannelByLocalCidLocked(connection_handle, local_cid);
  if (!channel) {
    return std::nullopt;
  }
  return LockedL2capChannel(*channel, std::move(lock));
}

std::optional<LockedL2capChannel> L2capChannelManager::FindChannelByRemoteCid(
    uint16_t connection_handle,
    uint16_t remote_cid) PW_NO_LOCK_SAFETY_ANALYSIS {
  // Lock annotations don't work with unique_lock
  std::unique_lock lock(channels_mutex_);
  L2capChannel* channel =
      FindChannelByRemoteCidLocked(connection_handle, remote_cid);
  if (!channel) {
    return std::nullopt;
  }
  return LockedL2capChannel(*channel, std::move(lock));
}

L2capChannel* L2capChannelManager::FindChannelByLocalCidLocked(
    uint16_t connection_handle, uint16_t local_cid) PW_NO_LOCK_SAFETY_ANALYSIS {
  auto channel_it = containers::FindIf(
      channels_, [connection_handle, local_cid](const L2capChannel& channel) {
        return channel.connection_handle() == connection_handle &&
               channel.local_cid() == local_cid;
      });
  if (channel_it == channels_.end()) {
    return nullptr;
  }
  return &(*channel_it);
}

L2capChannel* L2capChannelManager::FindChannelByRemoteCidLocked(
    uint16_t connection_handle,
    uint16_t remote_cid) PW_NO_LOCK_SAFETY_ANALYSIS {
  auto channel_it = containers::FindIf(
      channels_, [connection_handle, remote_cid](const L2capChannel& channel) {
        return channel.connection_handle() == connection_handle &&
               channel.remote_cid() == remote_cid;
      });
  if (channel_it == channels_.end()) {
    return nullptr;
  }
  return &(*channel_it);
}

void L2capChannelManager::Advance(
    IntrusiveForwardList<L2capChannel>::iterator& it) {
  if (++it == channels_.end()) {
    it = channels_.begin();
  }
}

void L2capChannelManager::RegisterStatusDelegate(
    L2capStatusDelegate& delegate) {
  status_tracker_.RegisterDelegate(delegate);
}

void L2capChannelManager::UnregisterStatusDelegate(
    L2capStatusDelegate& delegate) {
  status_tracker_.UnregisterDelegate(delegate);
}

void L2capChannelManager::HandleConnectionComplete(
    const L2capChannelConnectionInfo& info) {
  status_tracker_.HandleConnectionComplete(info);
}

void L2capChannelManager::HandleConfigurationChanged(
    const L2capChannelConfigurationInfo& info) {
  status_tracker_.HandleConfigurationChanged(info);
}

void L2capChannelManager::HandleAclDisconnectionComplete(
    uint16_t connection_handle) {
  PW_LOG_INFO(
      "btproxy: L2capChannelManager::HandleAclDisconnectionComplete - "
      "connection_handle: %u",
      connection_handle);
  for (;;) {
    IntrusiveForwardList<L2capChannel>::iterator channel_it;

    std::lock_guard lock(channels_mutex_);
    channel_it = containers::FindIf(
        channels_, [connection_handle](L2capChannel& channel) {
          return channel.connection_handle() == connection_handle &&
                 channel.state() == L2capChannel::State::kRunning;
        });
    if (channel_it == channels_.end()) {
      break;
    }

    DeregisterChannelLocked(*channel_it);
    channel_it->InternalClose();
  }

  status_tracker_.HandleAclDisconnectionComplete(connection_handle);
}

void L2capChannelManager::HandleDisconnectionCompleteLocked(
    const L2capStatusTracker::DisconnectParams& params)
    PW_NO_LOCK_SAFETY_ANALYSIS {
  // Must be called under channels_lock_ but we can't use proper lock annotation
  // here since the call comes via signaling channel.
  // TODO: https://pwbug.dev/390511432 - Figure out way to add annotations to
  // enforce this invariant.

  L2capChannel* channel =
      FindChannelByLocalCidLocked(params.connection_handle, params.local_cid);
  if (channel) {
    DeregisterChannelLocked(*channel);
    channel->InternalClose();
  }
  status_tracker_.HandleDisconnectionComplete(params);
}

void L2capChannelManager::DeliverPendingEvents() {
  status_tracker_.DeliverPendingEvents();
}

}  // namespace pw::bluetooth::proxy
