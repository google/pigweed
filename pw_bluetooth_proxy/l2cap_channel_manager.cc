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

#include "pw_containers/algorithm.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

L2capChannelManager::L2capChannelManager(AclDataChannel& acl_data_channel)
    : acl_data_channel_(acl_data_channel) {}

void L2capChannelManager::Reset() { h4_storage_.Reset(); }

void L2capChannelManager::RegisterReadChannel(L2capReadChannel& channel) {
  read_channels_.push_front(channel);
}

bool L2capChannelManager::ReleaseReadChannel(L2capReadChannel& channel) {
  return read_channels_.remove(channel);
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

pw::Status L2capChannelManager::SendL2capPacket(H4PacketWithH4&& tx_packet) {
  // H4 packet is hereby moved. Either `AclDataChannel` will move the packet to
  // the controller or drop the packet on error. In either case, packet will be
  // destructed, so its release function will be invoked.
  return acl_data_channel_.SendAcl(std::move(tx_packet));
}

uint16_t L2capChannelManager::GetH4BuffSize() const {
  return H4Storage::GetH4BuffSize();
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

}  // namespace pw::bluetooth::proxy
