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

#include "pw_bluetooth_proxy/internal/acl_data_channel.h"

#include <cstdint>

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_containers/algorithm.h"  // IWYU pragma: keep
#include "pw_log/log.h"

namespace pw::bluetooth::proxy {

void AclDataChannel::Reset() {
  credit_allocation_mutex_.lock();
  initialized_ = false;
  proxy_max_le_acl_packets_ = 0;
  proxy_pending_le_acl_packets_ = 0;
  active_connections_.clear();
  credit_allocation_mutex_.unlock();
}

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  credit_allocation_mutex_.lock();
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
  credit_allocation_mutex_.unlock();
}

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV1CommandCompleteEventWriter read_buffer_event);

template void
AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent<
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter>(
    emboss::LEReadBufferSizeV2CommandCompleteEventWriter read_buffer_event);

void AclDataChannel::HandleNumberOfCompletedPacketsEvent(
    H4PacketWithHci&& h4_packet) {
  auto nocp_event =
      MakeEmbossWriter<emboss::NumberOfCompletedPacketsEventWriter>(
          h4_packet.GetHciSpan());
  if (!nocp_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for NUMBER_OF_COMPLETED_PACKETS event. So "
        "will not process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }
  credit_allocation_mutex_.lock();
  bool should_send_to_host = false;
  for (uint8_t i = 0; i < nocp_event->num_handles().Read(); ++i) {
    uint16_t handle = nocp_event->nocp_data()[i].connection_handle().Read();
    uint16_t num_completed_packets =
        nocp_event->nocp_data()[i].num_completed_packets().Read();

    if (num_completed_packets == 0) {
      continue;
    }

    AclConnection* connection_ptr = FindConnection(handle);
    if (!connection_ptr) {
      // Credits for connection we are not tracking, so should pass event on to
      // host.
      should_send_to_host = true;
      continue;
    }

    // Reclaim proxy's credits before event is forwarded to host
    uint16_t num_reclaimed =
        std::min(num_completed_packets, connection_ptr->num_pending_packets);
    proxy_pending_le_acl_packets_ -= num_reclaimed;
    connection_ptr->num_pending_packets -= num_reclaimed;
    if (connection_ptr->num_pending_packets == 0) {
      active_connections_.erase(connection_ptr);
    }
    uint16_t credits_remaining = num_completed_packets - num_reclaimed;
    nocp_event->nocp_data()[i].num_completed_packets().Write(credits_remaining);
    if (credits_remaining > 0) {
      // Connection has credits remaining, so should past event on to host.
      should_send_to_host = true;
    }
  }
  credit_allocation_mutex_.unlock();
  if (should_send_to_host) {
    hci_transport_.SendToHost(std::move(h4_packet));
  }
}

void AclDataChannel::HandleDisconnectionCompleteEvent(
    H4PacketWithHci&& h4_packet) {
  auto dc_event = MakeEmbossView<emboss::DisconnectionCompleteEventWriter>(
      h4_packet.GetHciSpan());
  if (!dc_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for DISCONNECTION_COMPLETE event. So will not "
        "process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }
  credit_allocation_mutex_.lock();

  uint16_t conn_handle = dc_event->connection_handle().Read();
  AclConnection* connection_ptr = FindConnection(conn_handle);
  if (!connection_ptr) {
    credit_allocation_mutex_.unlock();
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  emboss::StatusCode status = dc_event->status().Read();
  if (status == emboss::StatusCode::SUCCESS) {
    if (connection_ptr->num_pending_packets > 0) {
      PW_LOG_WARN(
          "Proxy viewed disconnect (reason: %#.2hhx) for connection %#.4hx "
          "with packets in flight. Releasing associated credits",
          cpp23::to_underlying(dc_event->reason().Read()),
          conn_handle);
      proxy_pending_le_acl_packets_ -= connection_ptr->num_pending_packets;
    }
    active_connections_.erase(connection_ptr);
  } else {
    if (connection_ptr->num_pending_packets > 0) {
      PW_LOG_WARN(
          "Proxy viewed failed disconnect (status: %#.2hhx) for connection "
          "%#.4hx with packets in flight. Not releasing associated credits.",
          cpp23::to_underlying(status),
          conn_handle);
    }
  }
  credit_allocation_mutex_.unlock();
  hci_transport_.SendToHost(std::move(h4_packet));
}

uint16_t AclDataChannel::GetLeAclCreditsToReserve() const {
  return le_acl_credits_to_reserve_;
}

uint16_t AclDataChannel::GetNumFreeLeAclPackets() const {
  credit_allocation_mutex_.lock();
  uint16_t free_packets =
      proxy_max_le_acl_packets_ - proxy_pending_le_acl_packets_;
  credit_allocation_mutex_.unlock();
  return free_packets;
}

bool AclDataChannel::SendAcl(H4PacketWithH4&& h4_packet) {
  credit_allocation_mutex_.lock();
  if (proxy_pending_le_acl_packets_ == proxy_max_le_acl_packets_) {
    PW_LOG_WARN("No ACL send credits available. So will not send.");
    credit_allocation_mutex_.unlock();
    return false;
  }
  ++proxy_pending_le_acl_packets_;

  auto acl_view =
      MakeEmbossView<emboss::AclDataFrameHeaderView>(h4_packet.GetHciSpan());
  if (!acl_view.ok()) {
    PW_LOG_ERROR("Received invalid ACL packet. So will not send.");
    credit_allocation_mutex_.unlock();
    return false;
  }
  uint16_t handle = acl_view->handle().Read();

  AclConnection* connection_ptr = FindConnection(handle);
  if (!connection_ptr) {
    if (active_connections_.full()) {
      PW_LOG_ERROR("No space left in connections list.");
      credit_allocation_mutex_.unlock();
      return false;
    }
    active_connections_.push_back({handle, /*num_pending_packets=*/1});
  } else {
    ++connection_ptr->num_pending_packets;
  }

  hci_transport_.SendToController(std::move(h4_packet));
  credit_allocation_mutex_.unlock();
  return true;
}

AclDataChannel::AclConnection* AclDataChannel::FindConnection(uint16_t handle) {
  AclConnection* connection_it = containers::FindIf(
      active_connections_, [&handle](const AclConnection& connection) {
        return connection.handle == handle;
      });
  return connection_it == active_connections_.end() ? nullptr : connection_it;
}

}  // namespace pw::bluetooth::proxy
