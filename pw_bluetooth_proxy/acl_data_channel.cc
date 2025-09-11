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
#include <mutex>
#include <optional>

#include "lib/stdcompat/utility.h"
#include "pw_assert/check.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel_manager.h"
#include "pw_bluetooth_proxy/internal/recombiner.h"
#include "pw_containers/algorithm.h"  // IWYU pragma: keep
#include "pw_log/log.h"
#include "pw_span/cast.h"
#include "pw_status/status.h"

namespace pw::bluetooth::proxy {

AclDataChannel::AclConnection::AclConnection(
    AclTransportType transport,
    uint16_t connection_handle,
    uint16_t num_pending_packets,
    L2capChannelManager& l2cap_channel_manager)
    : transport_(transport),
      connection_handle_(connection_handle),
      num_pending_packets_(num_pending_packets),
      leu_signaling_channel_(l2cap_channel_manager, connection_handle),
      aclu_signaling_channel_(l2cap_channel_manager, connection_handle) {
  PW_LOG_INFO(
      "btproxy: AclConnection ctor. transport_: %u, connection_handle_: %#x",
      cpp23::to_underlying(transport_),
      connection_handle_);
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
  {
    std::lock_guard lock(credit_mutex_);
    // Reset credits first so no packets queued in signaling channels can be
    // sent.
    le_credits_.Reset();
    br_edr_credits_.Reset();
  }
  {
    std::lock_guard lock(connection_mutex_);
    acl_connections_.clear();
  }
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
    std::lock_guard lock(credit_mutex_);
    const uint16_t controller_max =
        read_buffer_event.total_num_acl_data_packets().Read();
    const uint16_t host_max = br_edr_credits_.Reserve(controller_max);
    read_buffer_event.total_num_acl_data_packets().Write(host_max);
  }

  l2cap_channel_manager_.ForceDrainChannelQueues();
}

template <class EventT>
void AclDataChannel::ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
    EventT read_buffer_event) {
  {
    std::lock_guard lock(credit_mutex_);
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
  l2cap_channel_manager_.ForceDrainChannelQueues();
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
    std::lock_guard lock(connection_mutex_);
    for (uint8_t i = 0; i < nocp_event->num_handles().Read(); ++i) {
      uint16_t handle = nocp_event->nocp_data()[i].connection_handle().Read();
      uint16_t num_completed_packets =
          nocp_event->nocp_data()[i].num_completed_packets().Read();

      if (num_completed_packets == 0) {
        continue;
      }

      AclConnection* connection_ptr = FindAclConnection(handle);
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

      {
        std::lock_guard credit_lock(credit_mutex_);
        LookupCredits(connection_ptr->transport()).MarkCompleted(num_reclaimed);
      }

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
    l2cap_channel_manager_.ForceDrainChannelQueues();
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
    std::lock_guard lock(connection_mutex_);
    uint16_t conn_handle = dc_event->connection_handle().Read();

    AclConnection* connection_ptr = FindAclConnection(conn_handle);

    if (!connection_ptr) {
      PW_LOG_INFO(
          "btproxy: Viewed disconnect (reason: %#.2hhx) for unacquired "
          "connection %#x.",
          cpp23::to_underlying(dc_event->reason().Read()),
          conn_handle);
      return;
    }

    emboss::StatusCode status = dc_event->status().Read();
    if (status == emboss::StatusCode::SUCCESS) {
      PW_LOG_INFO(
          "Proxy viewed disconnect (reason: %#.2hhx) for connection %#x.",
          cpp23::to_underlying(dc_event->reason().Read()),
          conn_handle);
      if (connection_ptr->num_pending_packets() > 0) {
        PW_LOG_WARN(
            "Connection %#x is disconnecting with packets in flight. Releasing "
            "associated credits.",
            conn_handle);
        std::lock_guard credit_lock(credit_mutex_);
        LookupCredits(connection_ptr->transport())
            .MarkCompleted(connection_ptr->num_pending_packets());
      }

      l2cap_channel_manager_.HandleAclDisconnectionComplete(conn_handle);
      acl_connections_.erase(connection_ptr);
    } else {  // Failed disconnect status
      if (connection_ptr->num_pending_packets() > 0) {
        PW_LOG_WARN(
            "Proxy viewed failed disconnect (status: %#.2hhx) for connection "
            "%#x with packets in flight. Not releasing associated credits.",
            cpp23::to_underlying(status),
            conn_handle);
      }
    }
  }
}

bool AclDataChannel::HasSendAclCapability(AclTransportType transport) const {
  std::lock_guard lock(credit_mutex_);
  return LookupCredits(transport).HasSendCapability();
}

uint16_t AclDataChannel::GetNumFreeAclPackets(
    AclTransportType transport) const {
  std::lock_guard lock(credit_mutex_);
  return LookupCredits(transport).Remaining();
}

std::optional<AclDataChannel::SendCredit> AclDataChannel::ReserveSendCredit(
    AclTransportType transport) {
  std::lock_guard lock(credit_mutex_);
  if (const auto status = LookupCredits(transport).MarkPending(1);
      !status.ok()) {
    return std::nullopt;
  }
  return SendCredit(transport, [this](AclTransportType t) {
    std::lock_guard fn_lock(credit_mutex_);
    LookupCredits(t).MarkCompleted(1);
  });
}

pw::Status AclDataChannel::SendAcl(H4PacketWithH4&& h4_packet,
                                   SendCredit&& credit) {
  std::lock_guard lock(connection_mutex_);
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
  std::lock_guard lock(connection_mutex_);
  AclConnection* connection_it = FindAclConnection(connection_handle);
  if (connection_it) {
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

L2capSignalingChannel* AclDataChannel::FindSignalingChannel(
    uint16_t connection_handle, uint16_t local_cid) {
  std::lock_guard lock(connection_mutex_);

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
      acl_connections_, [connection_handle](const AclConnection& connection) {
        return connection.connection_handle() == connection_handle;
      });
  return connection_it == acl_connections_.end() ? nullptr : connection_it;
}

namespace {

std::optional<LockedL2capChannel> GetLockedChannel(
    Direction direction,
    uint16_t handle,
    uint16_t l2cap_channel_id,
    L2capChannelManager& manager) {
  std::optional<LockedL2capChannel> channel;

  switch (direction) {
    case Direction::kFromController:
      return manager.FindChannelByLocalCid(handle, l2cap_channel_id);
    case Direction::kFromHost:
      return manager.FindChannelByRemoteCid(handle, l2cap_channel_id);
    default:
      PW_LOG_ERROR("Unrecognized Direction enumerator value %d.",
                   cpp23::to_underlying(direction));
      return std::nullopt;
  }
}

}  // namespace

namespace {
static constexpr std::uint8_t kH4PacketIndicatorSize = 1;
constexpr std::uint64_t kH4AclHeaderSize =
    emboss::AclDataFrameHeader::IntrinsicSizeInBytes() + kH4PacketIndicatorSize;
}  // namespace

bool AclDataChannel::HandleAclData(Direction direction,
                                   emboss::AclDataFrameWriter& acl) {
  // This function returns whether or not the frame was handled here.
  // * Return true if the frame was handled by the proxy and should _not_ be
  //   passed on to the other side (Host/Controller).
  // * Return false if the frame was _not_ handled by the proxy and should be
  //   passed on to the other side (Host/Controller).
  //
  // Special care needs to be taken when handling fragments. We don't want the
  // proxy to consume an initial fragment, and then decide to pass a subsequent
  // fragment because we didn't like it. That would cause the receiver to see
  // an unexpected CONTINUING_FRAGMENT.
  //
  // This ACL frame could contain
  // * A complete L2CAP PDU...
  //   * for an unrecognized channel    -> Pass
  //   * for a recognized channel       -> Handle and Consume
  //
  // * An initial fragment (w/ complete L2CAP header)...
  //   * while already recombining      -> Stop recombination and Pass(?)
  //   * for an unrecognized channel    -> Pass
  //   * for a recognized channel       -> Start recombination and Consume
  //
  // * A subsequent fragment (CONTINUING_FRAGMENT)...
  //   * while recombining              -> Recombine fragment and Consume
  //     (we know this must be for an L2CAP channel we care about)
  //   * while not recombining          -> Pass
  //
  // TODO: https://pwbug.dev/392666078 - Consider refactoring to look like
  // L2capCoc::ProcessPduFromControllerMultibuf() if we are okay with
  // allocating and copying for every PDU.
  static constexpr bool kHandled = true;
  static constexpr bool kUnhandled = false;

  const uint16_t handle = acl.header().handle().Read();

  bool is_first = false;
  bool is_fragment = false;

  // Set once we know CID from the first packet or from recombiner.
  uint16_t local_cid;

  // TODO: https://pwbug.dev/392665312 - make this <const uint8_t>
  const pw::span<uint8_t> acl_payload{
      acl.payload().BackingStorage().data(),
      acl.payload().BackingStorage().SizeInBytes()};

  {
    // Connection_mutex_ is expected to be acquired before/outside
    // channels_mutex_.
    // TODO: https://pwbug.dev/404344433 - Look at how we can ensure this with
    // thread safety analysis.
    std::lock_guard lock(connection_mutex_);
    AclConnection* connection = FindAclConnection(handle);
    if (!connection) {
      return kUnhandled;
    }
    Recombiner& recombiner = connection->GetRecombiner(direction);

    // Is this a fragment?
    const emboss::AclDataPacketBoundaryFlag boundary_flag =
        acl.header().packet_boundary_flag().Read();
    switch (boundary_flag) {
      // A subsequent fragment of a fragmented PDU.
      case emboss::AclDataPacketBoundaryFlag::CONTINUING_FRAGMENT: {
        // If recombination is not active, these are probably fragments for a
        // PDU that we previously chose not to recombine. Simply ignore them.
        //
        // TODO: https://pwbug.dev/393417198 - This could also be an erroneous
        // continuation of an already-recombined PDU, which would be better to
        // drop.
        if (!recombiner.IsActive()) {
          return kUnhandled;
        }

        local_cid = recombiner.local_cid();

        is_fragment = true;
        break;
      }
      // Non-fragment or the first fragment of a fragmented PDU.
      case emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE:
      case emboss::AclDataPacketBoundaryFlag::FIRST_FLUSHABLE: {
        is_first = true;

        // Ensure recombination is not already in progress
        if (recombiner.IsActive()) {
          PW_LOG_WARN(
              "Received non-continuation packet %s on connection %#x while "
              "recombination is active! Dropping previous partially-recombined "
              "PDU and handling this first packet normally.",
              DirectionToString(direction),
              handle);

          // Note this conditionally acquires channels_mutex_ which, if nested,
          // is expected to be acquired after/inside connection_mutex_.
          std::optional<LockedL2capChannel> channel =
              GetLockedChannel(direction,
                               handle,
                               recombiner.local_cid(),
                               l2cap_channel_manager_);
          recombiner.EndRecombination(channel);
        }  // recombiner.IsActive(). channel with channels_mutex_ released.

        // Currently, we require the full L2CAP header: We need the pdu_length
        // field so we know how much data to recombine, and we need the
        // channel_id field so we know whether or not this is a recognized L2CAP
        // channel and therefore whether or not we should recombine it.
        // TODO: https://pwbug.dev/437958454 - Handle fragments that are too
        // small to contain the L2CAP header.
        emboss::BasicL2capHeaderView l2cap_header =
            emboss::MakeBasicL2capHeaderView(acl_payload.data(),
                                             acl_payload.size());
        if (!l2cap_header.Ok()) {
          PW_LOG_ERROR(
              "ACL packet %s on connection %#x does not include full L2CAP "
              "header. Passing on.",
              DirectionToString(direction),
              handle);
          return kUnhandled;
        }

        local_cid = l2cap_header.channel_id().Read();

        // Is this a channel we care about?
        // Note this conditionally acquires channels_mutex_ which, if nested,
        // is expected to be acquired after/inside connection_mutex_.
        std::optional<LockedL2capChannel> channel = GetLockedChannel(
            direction, handle, local_cid, l2cap_channel_manager_);
        if (!channel.has_value()) {
          return kUnhandled;
        }

        const uint16_t acl_payload_size = acl.data_total_length().Read();

        const uint16_t l2cap_frame_length =
            emboss::BasicL2capHeader::IntrinsicSizeInBytes() +
            l2cap_header.pdu_length().Read();

        if (l2cap_frame_length < acl_payload_size) {
          PW_LOG_ERROR(
              "ACL packet %s for channel %#x on connection %#x has payload "
              "(%u bytes) larger than specified L2CAP PDU size (%u bytes). "
              "Dropping.",
              DirectionToString(direction),
              channel->channel().local_cid(),
              handle,
              acl_payload_size,
              l2cap_frame_length);
          return kHandled;
        }

        // Is this the first fragment of a fragmented PDU?
        // The first fragment is recognized when the L2CAP frame length exceeds
        // the ACL frame data_total_length.
        if (l2cap_frame_length > acl_payload_size) {
          is_fragment = true;

          // Start recombination
          pw::Status status = recombiner.StartRecombination(
              *channel, l2cap_frame_length, kH4AclHeaderSize);
          if (!status.ok()) {
            // TODO: https://pwbug.dev/404275508 - This is an acquired channel,
            // so need to do something different than just pass on to AP.
            PW_LOG_ERROR(
                "Cannot start recombination for L2capChannel connection. Will "
                "passthrough."
                "channel: %#x, local_cid: %#x, status:%s",
                channel->channel().connection_handle(),
                channel->channel().local_cid(),
                status.str());
            return kUnhandled;
          }
        }
        break;
      }  // FIRST_FLUSHABLE. channel with channels_mutex_ released.

      default: {
        PW_LOG_ERROR(
            "Packet %s on connection %#x: Unexpected ACL boundary flag: %u",
            DirectionToString(direction),
            handle,
            cpp23::to_underlying(boundary_flag));
        return kUnhandled;
      }
    }

    if (is_fragment) {
      // Recombine this fragment

      // If value, includes channels_mutex_ unique_lock.
      std::optional<LockedL2capChannel> channel = GetLockedChannel(
          direction, handle, local_cid, l2cap_channel_manager_);

      pw::Status recomb_status =
          recombiner.RecombineFragment(channel, acl_payload);
      if (!recomb_status.ok()) {
        // Given that RecombinationActive is checked above, the only way this
        // should fail is if the fragment is larger than expected, which can
        // only happen on a continuing fragment, because the first fragment
        // starts recombination above.
        PW_DCHECK(!is_first);

        PW_LOG_ERROR(
            "Received continuation packet %s for channel %#x on connection "
            "%#x over specified PDU length. Dropping entire PDU.",
            DirectionToString(direction),
            local_cid,
            handle);
        recombiner.EndRecombination(channel);
        return kHandled;  // We own the channel; drop.
      }

      if (!recombiner.IsComplete()) {
        // We are done with this packet and awaiting the remaining fragments.
        return kHandled;
      }

      // Recombination complete!
      // We will collect the recombination buffer from the channel below
      // (outside the connection mutex).

    }  // is_fragment. channel with channels_mutex_ released.
  }  // std::lock_guard lock(connection_mutex_)

  // At this point we have recombined a valid L2CAP frame. It may be
  // from a single first ACL packet or a series of recombined ones (in which
  // case we should be handling the last continuing packet).
  PW_CHECK((is_first && !is_fragment) || (!is_first && is_fragment));

  // But note, our return value only controls the disposition of the current ACL
  // packet.

  // If value, includes channels_mutex_ unique_lock.
  // We need channel for handling PDU and for recombine buffers.
  // channels_mutex_ must be held as long as `recombined_mbuf` and
  // `send_l2cap_pdu` are accessed to ensure channel is not destroyed.
  std::optional<LockedL2capChannel> channel =
      GetLockedChannel(direction, handle, local_cid, l2cap_channel_manager_);

  // If recombining, will be set with the recombined PDU. And must be held
  // as long as `send_l2cap_pdu` is accessed.
  std::optional<multibuf::MultiBuf> recombined_mbuf;

  // PDU we will actually send (will be set from first packet or from
  // recombination).
  pw::span<uint8_t> send_l2cap_pdu;

  if (!channel.has_value()) {
    // We don't have the channel anymore.  This indicates that the
    // channel instance that recombination was started with has since been
    // destroyed. So "drop" the PDU and handle the packet.
    PW_LOG_INFO(
        "Dropping first PDU %s original intended for channel %#x on connection "
        "%#x since channel instance was destroyed by client since first packet "
        "was received.",
        DirectionToString(direction),
        local_cid,
        handle);
    // TODO: https://pwbug.dev/402454277 - We might want to consider passing
    // kUnhandled for "signaling" channels, but since we don't have the channel
    // here we have no way to determine the channel type. Once we have shared
    // channel refs we should revisit.
    return kHandled;
  }

  if (is_first) {
    // We have whole PDU in first packet.
    send_l2cap_pdu = acl_payload;
  } else {
    // We are a fragment, so we need to collect the recombined PDU from the
    // channel.

    if (!Recombiner::HasBuf(channel, direction)) {
      // To get here we must have a `channel`, but now we have found `channel
      // doesn't have a recombination buf. This indicates `channel` is instance
      // other than the one we started recombination with. So "drop" the PDU and
      // handle the packet.
      PW_LOG_INFO(
          "Dropping recombined PDU %s original intended for channel %#x on "
          "connection %#x since channel instance was destroyed by client since "
          "first packet was received.",
          DirectionToString(direction),
          local_cid,
          handle);
      // TODO: https://pwbug.dev/392663102 - Revisit what best behavior is here
      // when we work on support for rejecting a recombined L2CAP PDU.
      return kHandled;
    }

    // Store the recombined multibuf.
    recombined_mbuf = Recombiner::TakeBuf(channel, direction);
    // We must have had IsComplete above to get here, so buf should always have
    // a value.
    PW_CHECK(recombined_mbuf.has_value());
    // Confirm the MultiBuf is contiguous as expected.
    PW_CHECK(recombined_mbuf->IsContiguous());

    send_l2cap_pdu =
        pw::span_cast<uint8_t>(recombined_mbuf->ContiguousSpan().value());

  }  // is_first else

  // Pass the L2CAP PDU on to the L2capChannel
  // TODO: https://pwbug.dev/403567488 - Look at sending MultiBuf here rather
  // than span. Channels at next level will create MultiBuf to pass on their
  // payload any.
  const bool result =
      (direction == Direction::kFromController)
          ? channel->channel().HandlePduFromController(send_l2cap_pdu)
          : channel->channel().HandlePduFromHost(send_l2cap_pdu);

  if ((result == kUnhandled) && is_fragment) {
    // Client rejected the entire PDU. So grab extra header for H4/ACL headers,
    // populate them, and pass that H4 packet on to the host.

    // Take back the extra header we reserved when starting the recombine.
    PW_CHECK(recombined_mbuf->ClaimPrefix(kH4AclHeaderSize));
    PW_CHECK(recombined_mbuf->IsContiguous());
    const pw::span<uint8_t> h4_span =
        pw::span_cast<uint8_t>(recombined_mbuf->ContiguousSpan().value());

    // TODO: https://pwbug.dev/438315637 - Also do this check for the BR/EDR
    // transport type once we know its max acl length.
    if (channel->channel().transport() == AclTransportType::kLe) {
      std::optional<uint16_t> le_packet_length =
          l2cap_channel_manager_.le_acl_data_packet_length();

      if (!le_packet_length) {
        PW_LOG_WARN(
            "le_acl_data_packet_length not known, so unable to check H4 "
            "length.");
      }
      const size_t kMaxH4Length =
          kH4PacketIndicatorSize + kH4AclHeaderSize + *le_packet_length;
      if (h4_span.size() > kMaxH4Length) {
        //  TODO: https://pwbug.dev/438543613 - Re-frag in this case.
        PW_LOG_WARN(
            "Recombined H4 length %zu is greater than allowed with "
            "le_acl_data_packet_length "
            "of %u for transport %u. Will still pass on single ACL packet.",
            h4_span.size(),
            *le_packet_length,
            cpp23::to_underlying(channel->channel().transport()));
      }
    }

    // Populate the H4 and ACL headers ahead of the recombined PDU.
    h4_span[0] = static_cast<uint8_t>(emboss::H4PacketType::ACL_DATA);
    pw::span<uint8_t> hci_span = h4_span.subspan(kH4PacketIndicatorSize);
    Result<emboss::AclDataFrameWriter> recombined_acl =
        MakeEmbossWriter<emboss::AclDataFrameWriter>(hci_span);
    PW_CHECK_OK(recombined_acl);
    recombined_acl->header().handle().Write(acl.header().handle().Read());
    recombined_acl->header().packet_boundary_flag().Write(
        emboss::AclDataPacketBoundaryFlag::FIRST_NON_FLUSHABLE);
    recombined_acl->header().broadcast_flag().Write(
        acl.header().broadcast_flag().Read());
    recombined_acl->data_total_length().Write(h4_span.size() -
                                              kH4AclHeaderSize);

    // Send onward to its final destination.
    switch (direction) {
      case Direction::kFromController: {
        H4PacketWithHci h4_packet{h4_span};
        hci_transport_.SendToHost(h4_span);
        break;
      }
      case Direction::kFromHost: {
        H4PacketWithH4 h4_packet{h4_span};
        hci_transport_.SendToController(std::move(h4_packet));
        break;
      }
    }

    // We still return kHandled here since the last fragment packet was already
    // passed on to the host as part of the recombined H4 packet.
    return kHandled;
  }  // (result == kUnhandled) && is_fragment

  // If value, releases unique_lock of channels_mutex_.
  channel.reset();

  //  It's possible for a channel handling rx traffic to have queued tx traffic
  //  or events. So call `DrainChannelQueuesIfNewTx` and `DeliverPendingEvents`
  // (outside of channels_mutex_ lock).
  l2cap_channel_manager_.DrainChannelQueuesIfNewTx();
  l2cap_channel_manager_.DeliverPendingEvents();

  return result;
}

}  // namespace pw::bluetooth::proxy
