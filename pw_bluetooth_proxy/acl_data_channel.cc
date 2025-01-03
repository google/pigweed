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

AclDataChannel::AclConnection::AclConnection(
    AclTransportType transport,
    uint16_t connection_handle,
    uint16_t num_pending_packets,
    L2capChannelManager& l2cap_channel_manager)
    : transport_(transport),
      state_(State::kOpen),
      connection_handle_(connection_handle),
      num_pending_packets_(num_pending_packets),
      leu_signaling_channel_(l2cap_channel_manager, connection_handle),
      aclu_signaling_channel_(l2cap_channel_manager, connection_handle) {
  PW_LOG_INFO(
      "btproxy: AclConnection ctor. transport_: %u, connection_handle_: %#x",
      cpp23::to_underlying(transport_),
      connection_handle_);
}

void AclDataChannel::AclConnection::Close() {
  PW_LOG_INFO(
      "btproxy: AclConnection::Close. transport_: %u, connection_handle_: %#x, "
      "previous state_: %u",
      cpp23::to_underlying(transport_),
      connection_handle_,
      cpp23::to_underlying(state_));

  state_ = State::kClosed;
}

AclDataChannel::SendCredit::SendCredit(SendCredit&& other) {
  *this = std::move(other);
}

AclDataChannel::SendCredit& AclDataChannel::SendCredit::operator=(
    SendCredit&& other) {
  if (this != &other) {
    transport_ = other.transport_;
    relinquish_fn_ = std::move(other.relinquish_fn_);
    other.relinquish_fn_ = nullptr;
  }
  return *this;
}

AclDataChannel::SendCredit::~SendCredit() {
  if (relinquish_fn_) {
    relinquish_fn_(transport_);
  }
}

AclDataChannel::SendCredit::SendCredit(
    AclTransportType transport,
    Function<void(AclTransportType transport)>&& relinquish_fn)
    : transport_(transport), relinquish_fn_(std::move(relinquish_fn)) {}

void AclDataChannel::SendCredit::MarkUsed() {
  PW_CHECK(relinquish_fn_);
  relinquish_fn_ = nullptr;
}

void AclDataChannel::Reset() {
  std::lock_guard lock(mutex_);
  le_credits_.Reset();
  br_edr_credits_.Reset();
  acl_connections_.clear();
}

void AclDataChannel::Credits::Reset() {
  proxy_max_ = 0;
  proxy_pending_ = 0;
}

uint16_t AclDataChannel::Credits::Reserve(uint16_t controller_max) {
  PW_CHECK(!Initialized(),
           "AclDataChannel is already initialized. Proxy should have been "
           "reset before this.");

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

AclDataChannel::Credits& AclDataChannel::LookupCredits(
    AclTransportType transport) {
  switch (transport) {
    case AclTransportType::kBrEdr:
      return br_edr_credits_;
    case AclTransportType::kLe:
      return le_credits_;
    default:
      PW_CHECK(false, "Invalid transport type");
  }
}

const AclDataChannel::Credits& AclDataChannel::LookupCredits(
    AclTransportType transport) const {
  switch (transport) {
    case AclTransportType::kBrEdr:
      return br_edr_credits_;
    case AclTransportType::kLe:
      return le_credits_;
    default:
      PW_CHECK(false, "Invalid transport type");
  }
}

void AclDataChannel::ProcessReadBufferSizeCommandCompleteEvent(
    emboss::ReadBufferSizeCommandCompleteEventWriter read_buffer_event) {
  {
    std::lock_guard lock(mutex_);
    const uint16_t controller_max =
        read_buffer_event.total_num_acl_data_packets().Read();
    const uint16_t host_max = br_edr_credits_.Reserve(controller_max);
    read_buffer_event.total_num_acl_data_packets().Write(host_max);
  }

  l2cap_channel_manager_.DrainChannelQueues();
}

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  {
    std::lock_guard lock(mutex_);
    const uint16_t controller_max =
        read_buffer_event.total_num_le_acl_data_packets().Read();
    // TODO: https://pwbug.dev/380316252 - Support shared buffers.
    const uint16_t host_max = le_credits_.Reserve(controller_max);
    read_buffer_event.total_num_le_acl_data_packets().Write(host_max);
  }

  const uint16_t le_acl_data_packet_length =
      read_buffer_event.le_acl_data_packet_length().Read();
  // TODO: https://pwbug.dev/380316252 - Support shared buffers.
  if (le_acl_data_packet_length == 0) {
    PW_LOG_ERROR(
        "Controller shares data buffers between BR/EDR and LE transport, which "
        "is not yet supported. So channels on LE transport will not be "
        "functional.");
  }
  l2cap_channel_manager_.set_le_acl_data_packet_length(
      le_acl_data_packet_length);
  // Send packets that may have queued before we acquired any LE ACL credits.
  l2cap_channel_manager_.DrainChannelQueues();
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
    std::lock_guard lock(mutex_);
    for (uint8_t i = 0; i < nocp_event->num_handles().Read(); ++i) {
      uint16_t handle = nocp_event->nocp_data()[i].connection_handle().Read();
      uint16_t num_completed_packets =
          nocp_event->nocp_data()[i].num_completed_packets().Read();

      if (num_completed_packets == 0) {
        continue;
      }

      AclConnection* connection_ptr = FindOpenAclConnection(handle);
      if (!connection_ptr) {
        // Credits for connection we are not tracking or closed connection, so
        // should pass event on to host.
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
    l2cap_channel_manager_.DrainChannelQueues();
  }
  if (should_send_to_host) {
    hci_transport_.SendToHost(std::move(h4_packet));
  }
}

void AclDataChannel::HandleConnectionCompleteEvent(
    H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::ConnectionCompleteEventView> connection_complete_event =
      MakeEmbossView<emboss::ConnectionCompleteEventView>(hci_buffer);
  if (!connection_complete_event.ok()) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  if (connection_complete_event->status().Read() !=
      emboss::StatusCode::SUCCESS) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  const uint16_t conn_handle =
      connection_complete_event->connection_handle().Read();

  if (CreateAclConnection(conn_handle, AclTransportType::kBrEdr) ==
      Status::ResourceExhausted()) {
    PW_LOG_ERROR(
        "Could not track connection like requested. Max connections "
        "reached.");
  }

  hci_transport_.SendToHost(std::move(h4_packet));
}

void AclDataChannel::HandleLeConnectionCompleteEvent(
    uint16_t connection_handle, emboss::StatusCode status) {
  if (status != emboss::StatusCode::SUCCESS) {
    return;
  }

  if (CreateAclConnection(connection_handle, AclTransportType::kLe) ==
      Status::ResourceExhausted()) {
    PW_LOG_ERROR(
        "Could not track connection like requested. Max connections "
        "reached.");
  }
}

void AclDataChannel::HandleLeConnectionCompleteEvent(
    H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::LEConnectionCompleteSubeventView> event =
      MakeEmbossView<emboss::LEConnectionCompleteSubeventView>(hci_buffer);
  if (!event.ok()) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  HandleLeConnectionCompleteEvent(event->connection_handle().Read(),
                                  event->status().Read());

  hci_transport_.SendToHost(std::move(h4_packet));
}

void AclDataChannel::HandleLeEnhancedConnectionCompleteV1Event(
    H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::LEEnhancedConnectionCompleteSubeventV1View> event =
      MakeEmbossView<emboss::LEEnhancedConnectionCompleteSubeventV1View>(
          hci_buffer);
  if (!event.ok()) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  HandleLeConnectionCompleteEvent(event->connection_handle().Read(),
                                  event->status().Read());

  hci_transport_.SendToHost(std::move(h4_packet));
}

void AclDataChannel::HandleLeEnhancedConnectionCompleteV2Event(
    H4PacketWithHci&& h4_packet) {
  pw::span<uint8_t> hci_buffer = h4_packet.GetHciSpan();
  Result<emboss::LEEnhancedConnectionCompleteSubeventV2View> event =
      MakeEmbossView<emboss::LEEnhancedConnectionCompleteSubeventV2View>(
          hci_buffer);
  if (!event.ok()) {
    hci_transport_.SendToHost(std::move(h4_packet));
    return;
  }

  HandleLeConnectionCompleteEvent(event->connection_handle().Read(),
                                  event->status().Read());

  hci_transport_.SendToHost(std::move(h4_packet));
}

void AclDataChannel::ProcessDisconnectionCompleteEvent(
    pw::span<uint8_t> hci_span) {
  Result<emboss::DisconnectionCompleteEventView> dc_event =
      MakeEmbossView<emboss::DisconnectionCompleteEventView>(hci_span);
  if (!dc_event.ok()) {
    PW_LOG_ERROR(
        "Buffer is too small for DISCONNECTION_COMPLETE event. So will not "
        "process.");
    return;
  }

  {
    std::lock_guard lock(mutex_);
    uint16_t conn_handle = dc_event->connection_handle().Read();

    AclConnection* connection_ptr = FindOpenAclConnection(conn_handle);

    if (!connection_ptr) {
      PW_LOG_WARN(
          "btproxy: Viewed disconnect (reason: %#.2hhx) for connection %#x, "
          "but was unable to find an existing open AclConnection.",
          cpp23::to_underlying(dc_event->reason().Read()),
          conn_handle);
      return;
    }

    emboss::StatusCode status = dc_event->status().Read();
    if (status == emboss::StatusCode::SUCCESS) {
      if (connection_ptr->num_pending_packets() > 0) {
        PW_LOG_WARN(
            "Proxy viewed disconnect (reason: %#.2hhx) for connection %#x "
            "with packets in flight. Releasing associated credits.",
            cpp23::to_underlying(dc_event->reason().Read()),
            conn_handle);

        LookupCredits(connection_ptr->transport())
            .MarkCompleted(connection_ptr->num_pending_packets());
      }

      // Close but do not erase connection until all channels on the connection
      // are dtored, as the channels may still try to access their connection's
      // contained objects like signaling channels.
      connection_ptr->Close();
      l2cap_channel_manager_.HandleDisconnectionComplete(conn_handle);
      return;
    }
    if (connection_ptr->num_pending_packets() > 0) {
      PW_LOG_WARN(
          "Proxy viewed failed disconnect (status: %#.2hhx) for connection "
          "%#x with packets in flight. Not releasing associated credits.",
          cpp23::to_underlying(status),
          conn_handle);
    }
  }
}

bool AclDataChannel::HasSendAclCapability(AclTransportType transport) const {
  std::lock_guard lock(mutex_);
  return LookupCredits(transport).HasSendCapability();
}

uint16_t AclDataChannel::GetNumFreeAclPackets(
    AclTransportType transport) const {
  std::lock_guard lock(mutex_);
  return LookupCredits(transport).Remaining();
}

std::optional<AclDataChannel::SendCredit> AclDataChannel::ReserveSendCredit(
    AclTransportType transport) {
  std::lock_guard lock(mutex_);
  if (const auto status = LookupCredits(transport).MarkPending(1);
      !status.ok()) {
    return std::nullopt;
  }
  return SendCredit(transport, [this](AclTransportType t) {
    std::lock_guard fn_lock(mutex_);
    LookupCredits(t).MarkCompleted(1);
  });
}

pw::Status AclDataChannel::SendAcl(H4PacketWithH4&& h4_packet,
                                   SendCredit&& credit) {
  std::lock_guard lock(mutex_);
  Result<emboss::AclDataFrameHeaderView> acl_view =
      MakeEmbossView<emboss::AclDataFrameHeaderView>(h4_packet.GetHciSpan());
  if (!acl_view.ok()) {
    PW_LOG_ERROR("An invalid ACL packet was provided. So will not send.");
    return pw::Status::InvalidArgument();
  }
  uint16_t handle = acl_view->handle().Read();

  AclConnection* connection_ptr = FindOpenAclConnection(handle);
  if (!connection_ptr) {
    PW_LOG_ERROR("Tried to send ACL packet on unregistered connection.");
    return pw::Status::NotFound();
  }

  if (connection_ptr->transport() != credit.transport_) {
    PW_LOG_WARN("Provided credit for wrong transport. So will not send.");
    return pw::Status::InvalidArgument();
  }
  credit.MarkUsed();

  connection_ptr->set_num_pending_packets(
      connection_ptr->num_pending_packets() + 1);

  hci_transport_.SendToController(std::move(h4_packet));
  return pw::OkStatus();
}

Status AclDataChannel::CreateAclConnection(uint16_t connection_handle,
                                           AclTransportType transport) {
  std::lock_guard lock(mutex_);
  AclConnection* connection_it = FindOpenAclConnection(connection_handle);
  if (connection_it) {
    PW_LOG_WARN(
        "btproxy: Attempt to create new AclConnection when existing one is "
        "already open. connection_handle: %#x",
        connection_handle);
    return Status::AlreadyExists();
  }
  if (acl_connections_.full()) {
    PW_LOG_ERROR(
        "btproxy: Attempt to create new AclConnection when acl_connections_ is"
        "already full. connection_handle: %#x",
        connection_handle);
    return Status::ResourceExhausted();
  }
  acl_connections_.emplace_back(transport,
                                /*connection_handle=*/connection_handle,
                                /*num_pending_packets=*/0,
                                l2cap_channel_manager_);
  return OkStatus();
}

pw::Status AclDataChannel::FragmentedPduStarted(Direction direction,
                                                uint16_t connection_handle) {
  std::lock_guard lock(mutex_);
  AclConnection* connection_ptr = FindOpenAclConnection(connection_handle);
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
  std::lock_guard lock(mutex_);
  AclConnection* connection_ptr = FindOpenAclConnection(connection_handle);
  if (!connection_ptr) {
    return Status::NotFound();
  }
  return connection_ptr->is_receiving_fragmented_pdu(direction);
}

pw::Status AclDataChannel::FragmentedPduFinished(Direction direction,
                                                 uint16_t connection_handle) {
  std::lock_guard lock(mutex_);
  AclConnection* connection_ptr = FindOpenAclConnection(connection_handle);
  if (!connection_ptr) {
    return Status::NotFound();
  }
  if (!connection_ptr->is_receiving_fragmented_pdu(direction)) {
    return Status::FailedPrecondition();
  }
  connection_ptr->set_is_receiving_fragmented_pdu(direction, false);
  return OkStatus();
}

L2capSignalingChannel* AclDataChannel::FindSignalingChannel(
    uint16_t connection_handle, uint16_t local_cid) {
  std::lock_guard lock(mutex_);

  AclConnection* connection_ptr = FindOpenAclConnection(connection_handle);
  if (!connection_ptr) {
    return nullptr;
  }

  if (local_cid == connection_ptr->signaling_channel()->local_cid()) {
    return connection_ptr->signaling_channel();
  }
  return nullptr;
}

AclDataChannel::AclConnection* AclDataChannel::FindOpenAclConnection(
    uint16_t connection_handle) {
  AclConnection* connection_it = containers::FindIf(
      acl_connections_, [connection_handle](const AclConnection& connection) {
        return connection.connection_handle() == connection_handle &&
               connection.state() == AclConnection::State::kOpen;
      });
  return connection_it == acl_connections_.end() ? nullptr : connection_it;
}

}  // namespace pw::bluetooth::proxy
