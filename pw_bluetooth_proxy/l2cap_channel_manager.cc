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

#include "pw_containers/algorithm.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

L2capChannelManager::L2capChannelManager(AclDataChannel& acl_data_channel)
    : acl_data_channel_(acl_data_channel),
      lrd_write_channel_(write_channels_.end()) {}

void L2capChannelManager::Reset() { h4_storage_.Reset(); }

void L2capChannelManager::RegisterReadChannel(L2capReadChannel& channel) {
  read_channels_.push_front(channel);
}

bool L2capChannelManager::ReleaseReadChannel(L2capReadChannel& channel) {
  return read_channels_.remove(channel);
}

void L2capChannelManager::RegisterWriteChannel(L2capWriteChannel& channel) {
  std::lock_guard lock(write_channels_mutex_);
  write_channels_.push_front(channel);
  if (lrd_write_channel_ == write_channels_.end()) {
    lrd_write_channel_ = write_channels_.begin();
  }
}

bool L2capChannelManager::ReleaseWriteChannel(L2capWriteChannel& channel) {
  std::lock_guard lock(write_channels_mutex_);
  if (&channel == &(*lrd_write_channel_)) {
    Advance(lrd_write_channel_);
  }

  bool was_removed = write_channels_.remove(channel);

  // If `channel` was the only element in `write_channels_`, advancing
  // `lrd_write_channel_` just wrapped it back on itself, so we reset it here.
  if (write_channels_.empty()) {
    lrd_write_channel_ = write_channels_.end();
  }

  return was_removed;
}

pw::Result<H4PacketWithH4> L2capChannelManager::GetTxH4Packet(uint16_t size) {
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

  H4PacketWithH4 h4_packet(
      span(h4_buff->data(), size),
      /*release_fn=*/[h4_storage = &h4_storage_](const uint8_t* buffer) {
        h4_storage->ReleaseH4Buff(buffer);
      });
  h4_packet.SetH4Type(emboss::H4PacketType::ACL_DATA);

  return h4_packet;
}

uint16_t L2capChannelManager::GetH4BuffSize() const {
  return H4Storage::GetH4BuffSize();
}

void L2capChannelManager::DrainWriteChannelQueues() {
  std::lock_guard lock(write_channels_mutex_);

  if (write_channels_.empty()) {
    return;
  }

  DrainWriteChannelQueues(AclTransportType::kBrEdr);
  DrainWriteChannelQueues(AclTransportType::kLe);
}

void L2capChannelManager::DrainWriteChannelQueues(AclTransportType transport) {
  IntrusiveForwardList<L2capWriteChannel>::iterator round_robin_start =
      lrd_write_channel_;
  // Iterate around `write_channels_` in round robin fashion. For each channel,
  // send as many queued packets as are available. Proceed until we run out of
  // ACL send credits or finish visiting every channel.
  // TODO: https://pwbug.dev/379337260 - Only drain one L2CAP PDU per channel
  // before moving on. (This may require sending multiple ACL fragments.)
  while (acl_data_channel_.GetNumFreeAclPackets(transport) > 0) {
    if (lrd_write_channel_->transport() != transport) {
      Advance(lrd_write_channel_);
      if (lrd_write_channel_ == round_robin_start) {
        return;
      }
      continue;
    }

    std::optional<H4PacketWithH4> packet = lrd_write_channel_->DequeuePacket();
    if (!packet) {
      Advance(lrd_write_channel_);
      if (lrd_write_channel_ == round_robin_start) {
        return;
      }
      continue;
    }

    PW_CHECK_OK(acl_data_channel_.SendAcl(std::move(*packet)));
  }
}

L2capWriteChannel* L2capChannelManager::FindWriteChannel(
    uint16_t connection_handle, uint16_t remote_cid) {
  std::lock_guard lock(write_channels_mutex_);
  auto connection_it = containers::FindIf(
      write_channels_,
      [connection_handle, remote_cid](const L2capWriteChannel& channel) {
        return channel.connection_handle() == connection_handle &&
               channel.remote_cid() == remote_cid;
      });
  return connection_it == write_channels_.end() ? nullptr : &(*connection_it);
}

L2capReadChannel* L2capChannelManager::FindReadChannel(
    uint16_t connection_handle, uint16_t local_cid) {
  auto connection_it = containers::FindIf(
      read_channels_,
      [connection_handle, local_cid](const L2capReadChannel& channel) {
        return channel.connection_handle() == connection_handle &&
               channel.local_cid() == local_cid;
      });
  return connection_it == read_channels_.end() ? nullptr : &(*connection_it);
}

void L2capChannelManager::Advance(
    IntrusiveForwardList<L2capWriteChannel>::iterator& it) {
  if (++it == write_channels_.end()) {
    it = write_channels_.begin();
  }
}

}  // namespace pw::bluetooth::proxy
