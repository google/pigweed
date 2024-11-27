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

#include <mutex>

#include "lib/stdcompat/utility.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_containers/algorithm.h"  // IWYU pragma: keep
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

void AclDataChannel::Reset() {
  std::lock_guard lock(credit_allocation_mutex_);
  le_credits_.Reset();
  br_edr_credits_.Reset();
  active_acl_connections_.clear();
}

void AclDataChannel::Credits::Reset() {
  proxy_max_ = 0;
  proxy_pending_ = 0;
}

uint16_t AclDataChannel::Credits::Reserve(uint16_t controller_max) {
  PW_CHECK(proxy_max_ == 0,
           "AclDataChannel is already initialized, but encountered another "
           "ReadBufferSizeCommandCompleteEvent.");

  proxy_max_ = std::min(controller_max, to_reserve_);
  const uint16_t host_max = controller_max - proxy_max_;

  PW_LOG_INFO(
      "Bluetooth Proxy reserved %d ACL data credits. Passed %d on to host.",
      proxy_max_,
      host_max);

  if (proxy_max_ < to_reserve_) {
    PW_LOG_ERROR(
        "Only was able to reserve %d acl data credits rather than the "
        "configured %d from the controller provided's data credits of %d. ",
        proxy_max_,
        to_reserve_,
        controller_max);
  }

  return host_max;
}

Status AclDataChannel::Credits::MarkPending(uint16_t num_credits) {
  if (num_credits > Available()) {
    return Status::ResourceExhausted();
  }

  proxy_pending_ += num_credits;

  return OkStatus();
}

void AclDataChannel::Credits::MarkCompleted(uint16_t num_credits) {
  if (num_credits > proxy_pending_) {
    PW_LOG_ERROR("Tried to mark completed more packets than were pending.");
    proxy_pending_ = 0;
  } else {
    proxy_pending_ -= num_credits;
  }
}

AclDataChannel::Credits& AclDataChannel::LookupCredits(AclTransport transport) {
  switch (transport) {
    case AclTransport::kBrEdr:
      return br_edr_credits_;
    case AclTransport::kLe:
      return le_credits_;
    default:
      PW_CHECK(false, "Invalid transport type");
  }
}

const AclDataChannel::Credits& AclDataChannel::LookupCredits(
    AclTransport transport) const {
  switch (transport) {
    case AclTransport::kBrEdr:
      return br_edr_credits_;
    case AclTransport::kLe:
      return le_credits_;
    default:
      PW_CHECK(false, "Invalid transport type");
  }
}

void AclDataChannel::ProcessReadBufferSizeCommandCompleteEvent(
    emboss::ReadBufferSizeCommandCompleteEventWriter read_buffer_event) {
  {
    std::lock_guard lock(credit_allocation_mutex_);
    const uint16_t controller_max =
        read_buffer_event.total_num_acl_data_packets().Read();
    const uint16_t host_max = br_edr_credits_.Reserve(controller_max);
    read_buffer_event.total_num_acl_data_packets().Write(host_max);
  }

  l2cap_channel_manager_.DrainWriteChannelQueues();
}

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  {
    std::lock_guard lock(credit_allocation_mutex_);
    const uint16_t controller_max =
        read_buffer_event.total_num_le_acl_data_packets().Read();
    // TODO: https://pwbug.dev/380316252 - Support shared buffers.
    const uint16_t host_max = le_credits_.Reserve(controller_max);
    read_buffer_event.total_num_le_acl_data_packets().Write(host_max);
  }

  // Send packets that may have queued before we acquired any LE ACL credits.
  l2cap_channel_manager_.DrainWriteChannelQueues();
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
  Result<emboss::NumberOfCompletedPacketsEventWriter> nocp_event =
      MakeEmbossWriter<emboss::NumberOfCompletedPacketsEventWriter>(
          h4_packet.GetHciSpan());
  if (!nocp_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for NUMBER_OF_COMPLETED_PACKETS event. So "
        "will not process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  bool should_send_to_host = false;
  bool did_reclaim_credits = false;
  {
    std::lock_guard lock(credit_allocation_mutex_);
    for (uint8_t i = 0; i < nocp_event->num_handles().Read(); ++i) {
      uint16_t handle = nocp_event->nocp_data()[i].connection_handle().Read();
      uint16_t num_completed_packets =
          nocp_event->nocp_data()[i].num_completed_packets().Read();

      if (num_completed_packets == 0) {
        continue;
      }

      AclConnection* connection_ptr = FindAclConnection(handle);
      if (!connection_ptr) {
        // Credits for connection we are not tracking, so should pass event on
        // to host.
        should_send_to_host = true;
        continue;
      }

      // Reclaim proxy's credits before event is forwarded to host
      uint16_t num_pending_packets = connection_ptr->num_pending_packets();
      uint16_t num_reclaimed =
          std::min(num_completed_packets, num_pending_packets);

      if (num_reclaimed > 0) {
        did_reclaim_credits = true;
      }

      LookupCredits(connection_ptr->transport()).MarkCompleted(num_reclaimed);

      connection_ptr->set_num_pending_packets(num_pending_packets -
                                              num_reclaimed);

      uint16_t credits_remaining = num_completed_packets - num_reclaimed;
      nocp_event->nocp_data()[i].num_completed_packets().Write(
          credits_remaining);
      if (credits_remaining > 0) {
        // Connection has credits remaining, so should past event on to host.
        should_send_to_host = true;
      }
    }
  }

  if (did_reclaim_credits) {
    l2cap_channel_manager_.DrainWriteChannelQueues();
  }
  if (should_send_to_host) {
    hci_transport_.SendToHost(std::move(h4_packet));
  }
}

void AclDataChannel::HandleDisconnectionCompleteEvent(
    H4PacketWithHci&& h4_packet) {
  Result<emboss::DisconnectionCompleteEventView> dc_event =
      MakeEmbossView<emboss::DisconnectionCompleteEventView>(
          h4_packet.GetHciSpan());
  if (!dc_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for DISCONNECTION_COMPLETE event. So will not "
        "process.");
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  {
    std::lock_guard lock(credit_allocation_mutex_);
    uint16_t conn_handle = dc_event->connection_handle().Read();

    AclConnection* connection_ptr = FindAclConnection(conn_handle);
    if (!connection_ptr) {
      hci_transport_.SendToHost(std::move(h4_packet));
      return;
    }

    emboss::StatusCode status = dc_event->status().Read();
    if (status == emboss::StatusCode::SUCCESS) {
      if (connection_ptr->num_pending_packets() > 0) {
        PW_LOG_WARN(
            "Proxy viewed disconnect (reason: %#.2hhx) for connection %#.4hx "
            "with packets in flight. Releasing associated credits",
            cpp23::to_underlying(dc_event->reason().Read()),
            conn_handle);

        LookupCredits(connection_ptr->transport())
            .MarkCompleted(connection_ptr->num_pending_packets());
      }
      active_acl_connections_.erase(connection_ptr);
    } else {
      if (connection_ptr->num_pending_packets() > 0) {
        PW_LOG_WARN(
            "Proxy viewed failed disconnect (status: %#.2hhx) for connection "
            "%#.4hx with packets in flight. Not releasing associated credits.",
            cpp23::to_underlying(status),
            conn_handle);
      }
    }
  }
  hci_transport_.SendToHost(std::move(h4_packet));
}

bool AclDataChannel::HasSendAclCapability(AclTransport transport) const {
  std::lock_guard lock(credit_allocation_mutex_);
  return LookupCredits(transport).HasSendCapability();
}

uint16_t AclDataChannel::GetNumFreeAclPackets(AclTransport transport) const {
  std::lock_guard lock(credit_allocation_mutex_);
  return LookupCredits(transport).Remaining();
}

pw::Status AclDataChannel::SendAcl(H4PacketWithH4&& h4_packet) {
  std::lock_guard lock(credit_allocation_mutex_);
  Result<emboss::AclDataFrameHeaderView> acl_view =
      MakeEmbossView<emboss::AclDataFrameHeaderView>(h4_packet.GetHciSpan());
  if (!acl_view.ok()) {
    PW_LOG_ERROR("An invalid ACL packet was provided. So will not send.");
    return pw::Status::InvalidArgument();
  }
  uint16_t handle = acl_view->handle().Read();

  AclConnection* connection_ptr = FindAclConnection(handle);
  if (!connection_ptr) {
    PW_LOG_ERROR("Tried to send ACL packet on unregistered connection.");
    return pw::Status::NotFound();
  }

  if (const auto status =
          LookupCredits(connection_ptr->transport()).MarkPending(1);
      !status.ok()) {
    PW_LOG_WARN("No ACL send credits available. So will not send.");
    return pw::Status::Unavailable();
  }

  connection_ptr->set_num_pending_packets(
      connection_ptr->num_pending_packets() + 1);

  hci_transport_.SendToController(std::move(h4_packet));
  return pw::OkStatus();
}

Status AclDataChannel::CreateAclConnection(uint16_t connection_handle,
                                           AclTransport transport) {
  std::lock_guard lock(credit_allocation_mutex_);
  AclConnection* connection_it = FindAclConnection(connection_handle);
  if (connection_it) {
    return Status::AlreadyExists();
  }
  if (active_acl_connections_.full()) {
    return Status::ResourceExhausted();
  }
  active_acl_connections_.emplace_back(transport,
                                       /*connection_handle=*/connection_handle,
                                       /*num_pending_packets=*/0,
                                       l2cap_channel_manager_);
  return OkStatus();
}

pw::Status AclDataChannel::FragmentedPduStarted(Direction direction,
                                                uint16_t connection_handle) {
  std::lock_guard lock(credit_allocation_mutex_);
  AclConnection* connection_ptr = FindAclConnection(connection_handle);
  if (!connection_ptr) {
    return Status::NotFound();
  }
  if (connection_ptr->is_receiving_fragmented_pdu(direction)) {
    return Status::FailedPrecondition();
  }
  connection_ptr->set_is_receiving_fragmented_pdu(direction, true);
  return OkStatus();
}

pw::Result<bool> AclDataChannel::IsReceivingFragmentedPdu(
    Direction direction, uint16_t connection_handle) {
  std::lock_guard lock(credit_allocation_mutex_);
  AclConnection* connection_ptr = FindAclConnection(connection_handle);
  if (!connection_ptr) {
    return Status::NotFound();
  }
  return connection_ptr->is_receiving_fragmented_pdu(direction);
}

pw::Status AclDataChannel::FragmentedPduFinished(Direction direction,
                                                 uint16_t connection_handle) {
  std::lock_guard lock(credit_allocation_mutex_);
  AclConnection* connection_ptr = FindAclConnection(connection_handle);
  if (!connection_ptr) {
    return Status::NotFound();
  }
  if (!connection_ptr->is_receiving_fragmented_pdu(direction)) {
    return Status::FailedPrecondition();
  }
  connection_ptr->set_is_receiving_fragmented_pdu(direction, false);
  return OkStatus();
}

L2capReadChannel* AclDataChannel::FindSignalingChannel(
    uint16_t connection_handle, uint16_t local_cid) {
  std::lock_guard lock(credit_allocation_mutex_);

  AclConnection* connection_ptr = FindAclConnection(connection_handle);
  if (!connection_ptr) {
    return nullptr;
  }

  if (local_cid == connection_ptr->signaling_channel()->local_cid()) {
    return connection_ptr->signaling_channel();
  }
  return nullptr;
}

AclDataChannel::AclConnection* AclDataChannel::FindAclConnection(
    uint16_t connection_handle) {
  AclConnection* connection_it = containers::FindIf(
      active_acl_connections_,
      [&connection_handle](const AclConnection& connection) {
        return connection.connection_handle() == connection_handle;
      });
  return connection_it == active_acl_connections_.end() ? nullptr
                                                        : connection_it;
}

}  // namespace pw::bluetooth::proxy
