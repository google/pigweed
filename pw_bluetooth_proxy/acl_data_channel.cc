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

#include "pw_bluetooth_proxy/acl_data_channel.h"

#include <cstdint>
#include <mutex>

#include "pw_bluetooth/hci_data.emb.h"
#include "pw_containers/algorithm.h"  // IWYU pragma: keep
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  std::lock_guard lock(credit_allocation_mutex_);
  if (initialized_) {
    PW_LOG_WARN(
        "AclDataChannel is already initialized, but encountered another "
        "ReadBufferSizeCommandCompleteEvent.");
  }

  initialized_ = true;

  uint16_t controller_max_le_acl_packets =
      read_buffer_event.total_num_le_acl_data_packets().Read();
  proxy_max_le_acl_packets_ =
      std::min(controller_max_le_acl_packets, le_acl_credits_to_reserve_);
  uint16_t host_max_le_acl_packets =
      controller_max_le_acl_packets - proxy_max_le_acl_packets_;
  read_buffer_event.total_num_le_acl_data_packets().Write(
      host_max_le_acl_packets);
  PW_LOG_INFO(
      "Bluetooth Proxy reserved %d ACL data credits. Passed %d on to host.",
      proxy_max_le_acl_packets_,
      host_max_le_acl_packets);

  if (proxy_max_le_acl_packets_ < le_acl_credits_to_reserve_) {
    PW_LOG_ERROR(
        "Only was able to reserve %d acl data credits rather than the "
        "configured %d from the controller provided's data credits of %d. ",
        proxy_max_le_acl_packets_,
        le_acl_credits_to_reserve_,
        controller_max_le_acl_packets);
  }
}

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter read_buffer_event);

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter read_buffer_event);

void AclDataChannel::ProcessNumberOfCompletedPacketsEvent(
    emboss::NumberOfCompletedPacketsEventWriter nocp_event) {
  std::lock_guard lock(credit_allocation_mutex_);
  for (uint8_t i = 0; i < nocp_event.num_handles().Read(); ++i) {
    uint16_t handle = nocp_event.nocp_data()[i].connection_handle().Read();
    auto connection_it = containers::FindIf(
        active_connections_, [&handle](const AclConnection& connection) {
          return connection.handle == handle;
        });
    if (connection_it == active_connections_.end()) {
      continue;
    }

    // Reclaim proxy's credits before event is forwarded to host
    uint16_t num_completed_packets =
        nocp_event.nocp_data()[i].num_completed_packets().Read();
    uint16_t num_reclaimed =
        std::min(num_completed_packets, connection_it->num_pending_packets);
    proxy_pending_le_acl_packets_ -= num_reclaimed;
    connection_it->num_pending_packets -= num_reclaimed;
    nocp_event.nocp_data()[i].num_completed_packets().Write(
        num_completed_packets - num_reclaimed);
  }
}

uint16_t AclDataChannel::GetLeAclCreditsToReserve() const {
  return le_acl_credits_to_reserve_;
}

uint16_t AclDataChannel::GetNumFreeLeAclPackets() const {
  std::lock_guard lock(credit_allocation_mutex_);
  return proxy_max_le_acl_packets_ - proxy_pending_le_acl_packets_;
}

bool AclDataChannel::SendAcl(H4PacketWithH4&& h4_packet) {
  std::lock_guard lock(credit_allocation_mutex_);
  if (proxy_pending_le_acl_packets_ == proxy_max_le_acl_packets_) {
    return false;
  }
  ++proxy_pending_le_acl_packets_;

  emboss::AclDataFrameHeaderView acl_view =
      MakeEmboss<emboss::AclDataFrameHeaderView>(h4_packet.GetHciSpan());
  if (!acl_view.Ok()) {
    PW_LOG_ERROR("Received invalid ACL packet. So will not send.");
    return false;
  }
  uint16_t handle = acl_view.handle().Read();

  auto connection_it = containers::FindIf(
      active_connections_, [&handle](const AclConnection& connection) {
        return connection.handle == handle;
      });
  if (connection_it == active_connections_.end()) {
    active_connections_.push_back({handle, /*num_pending_packets=*/1});
  } else {
    ++connection_it->num_pending_packets;
  }

  hci_transport_.SendToController(std::move(h4_packet));
  return true;
}

}  // namespace pw::bluetooth::proxy
