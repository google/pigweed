// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "acl_data_channel.h"

#include <endian.h>
#include <lib/async/cpp/time.h>
#include <lib/async/default.h>

#include <iterator>

#include "lib/fit/function.h"
#include "pw_bluetooth/vendor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "transport.h"

namespace bt::hci {

class AclDataChannelImpl final : public AclDataChannel {
 public:
  AclDataChannelImpl(Transport* transport, pw::bluetooth::Controller* hci,
                     const DataBufferInfo& bredr_buffer_info, const DataBufferInfo& le_buffer_info);
  ~AclDataChannelImpl() override;

  // AclDataChannel overrides:
  void RegisterConnection(WeakPtr<ConnectionInterface> connection) override;
  void UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  void OnOutboundPacketAvailable() override;
  void AttachInspect(inspect::Node& parent, const std::string& name) override;
  void SetDataRxHandler(ACLPacketHandler rx_callback) override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override;
  const DataBufferInfo& GetBufferInfo() const override;
  const DataBufferInfo& GetLeBufferInfo() const override;
  void RequestAclPriority(pw::bluetooth::AclPriority priority, hci_spec::ConnectionHandle handle,
                          fit::callback<void(fit::result<fit::failed>)> callback) override;

 private:
  using ConnectionMap =
      std::unordered_map<hci_spec::ConnectionHandle, WeakPtr<ConnectionInterface>>;

  struct PendingPacketData {
    bt::LinkType ll_type = bt::LinkType::kACL;
    size_t count = 0;
  };

  // Handler for the HCI Number of Completed Packets Event, used for packet-based data flow control.
  CommandChannel::EventCallbackResult NumberOfCompletedPacketsCallback(const EventPacket& event);

  // Sends next queued packets over the ACL data channel while the controller has free buffer slots.
  // If controller buffers are free and some links have queued packets, we round-robin iterate
  // through links, sending a packet from each link with queued packets until the controller is full
  // or we run out of packets.
  void TrySendNextPackets();

  // Returns the number of free controller buffer slots for packets of type |link_type|, taking
  // shared buffers into account.
  size_t GetNumFreePacketsForLinkType(LinkType link_type) const;

  // Decreases the |link_type| pending packets count by |count|, taking shared buffers into account.
  void DecrementPendingPacketsForLinkType(LinkType link_type, size_t count);

  // Increments the  pending packets count for links of type |link_type|, taking shared buffers into
  // account.
  void IncrementPendingPacketsForLinkType(LinkType link_type);

  // Returns true if the LE data buffer is not available
  bool IsBrEdrBufferShared() const;

  // Called when a packet is received from the controller. Validates the packet and calls the
  // client's RX callback.
  void OnRxPacket(pw::span<const std::byte> packet);

  // Increment connection iterators using round robin scheduling.
  // If the BR/EDR buffer is shared, simply increment the iterator to the next connection.
  // If the BR/EDR buffer isn't shared, increment the iterator to the next connection of type
  // |connection_type|.
  // No-op if |conn_iter| is |registered_connections_.end()|.
  void IncrementRoundRobinIterator(ConnectionMap::iterator& conn_iter,
                                   bt::LinkType connection_type);

  // Increments count of pending packets that have been sent to the controller on |connection|.
  void IncrementPendingPacketsForLink(WeakPtr<ConnectionInterface>& connection);

  // Sends queued packets from links in a round-robin fashion, starting with |current_link|.
  // |current_link| will be incremented to the next link that should send packets (according to the
  // round-robin policy).
  void SendPackets(ConnectionMap::iterator& current_link);

  // Handler for HCI_Buffer_Overflow_event.
  CommandChannel::EventCallbackResult DataBufferOverflowCallback(const EventPacket& event);

  void ResetRoundRobinIterators();

  // Links this node to the inspect tree. Initialized as needed by AttachInspect.
  inspect::Node node_;

  // Contents of |node_|. Retained as members so that they last as long as a class instance.
  inspect::Node le_subnode_;
  inspect::BoolProperty le_subnode_shared_with_bredr_property_;
  inspect::Node bredr_subnode_;

  // The Transport object that owns this instance.
  Transport* const transport_;  // weak;

  // Controller is owned by Transport and will outlive this object.
  pw::bluetooth::Controller* const hci_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_ = 0;

  // The event handler ID for the Data Buffer Overflow event.
  CommandChannel::EventHandlerId data_buffer_overflow_event_handler_id_ = 0;

  // The dispatcher used for posting tasks on the HCI transport I/O thread.
  async_dispatcher_t* io_dispatcher_ = async_get_default_dispatcher();

  // The current handler for incoming data.
  ACLPacketHandler rx_callback_;

  // BR/EDR data buffer information. This buffer will not be available on LE-only controllers.
  const DataBufferInfo bredr_buffer_info_;

  // LE data buffer information. This buffer will not be available on BR/EDR-only controllers (which
  // we do not support) and MAY be available on dual-mode controllers. We maintain that if this
  // buffer is not available, then the BR/EDR buffer MUST be available.
  const DataBufferInfo le_buffer_info_;

  // The current count of the number of ACL data packets that have been sent to the controller.
  // |num_pending_le_packets_| is ignored if the controller uses one buffer for LE and BR/EDR.
  UintInspectable<size_t> num_pending_bredr_packets_;
  UintInspectable<size_t> num_pending_le_packets_;

  // Stores per-connection information of unacknowledged packets sent to the controller. Entries are
  // updated/removed on the HCI Number Of Completed Packets event and when a connection is
  // unregistered (the controller does not acknowledge packets of disconnected links).
  std::unordered_map<hci_spec::ConnectionHandle, PendingPacketData> pending_links_;

  // Stores connections registered by RegisterConnection().
  ConnectionMap registered_connections_;

  // Iterators used to round-robin through links for sending packets. When the BR/EDR buffer is
  // shared with LE, |current_le_link_| is ignored
  ConnectionMap::iterator current_bredr_link_ = registered_connections_.end();
  ConnectionMap::iterator current_le_link_ = registered_connections_.end();

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AclDataChannelImpl);
};

std::unique_ptr<AclDataChannel> AclDataChannel::Create(Transport* transport,
                                                       pw::bluetooth::Controller* hci,
                                                       const DataBufferInfo& bredr_buffer_info,
                                                       const DataBufferInfo& le_buffer_info) {
  return std::make_unique<AclDataChannelImpl>(transport, hci, bredr_buffer_info, le_buffer_info);
}

AclDataChannelImpl::AclDataChannelImpl(Transport* transport, pw::bluetooth::Controller* hci,
                                       const DataBufferInfo& bredr_buffer_info,
                                       const DataBufferInfo& le_buffer_info)
    : transport_(transport),
      hci_(hci),
      bredr_buffer_info_(bredr_buffer_info),
      le_buffer_info_(le_buffer_info) {
  BT_DEBUG_ASSERT(transport_);
  BT_ASSERT(hci_);

  BT_DEBUG_ASSERT(bredr_buffer_info.IsAvailable() || le_buffer_info.IsAvailable());

  num_completed_packets_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      hci_spec::kNumberOfCompletedPacketsEventCode,
      fit::bind_member<&AclDataChannelImpl::NumberOfCompletedPacketsCallback>(this));
  BT_DEBUG_ASSERT(num_completed_packets_event_handler_id_);

  data_buffer_overflow_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      hci_spec::kDataBufferOverflowEventCode,
      fit::bind_member<&AclDataChannelImpl::DataBufferOverflowCallback>(this));
  BT_DEBUG_ASSERT(data_buffer_overflow_event_handler_id_);

  bt_log(DEBUG, "hci", "AclDataChannel initialized");
}

AclDataChannelImpl::~AclDataChannelImpl() {
  bt_log(INFO, "hci", "AclDataChannel shutting down");

  transport_->command_channel()->RemoveEventHandler(num_completed_packets_event_handler_id_);
  transport_->command_channel()->RemoveEventHandler(data_buffer_overflow_event_handler_id_);

  hci_->SetReceiveAclFunction(nullptr);
}

void AclDataChannelImpl::RegisterConnection(WeakPtr<ConnectionInterface> connection) {
  bt_log(DEBUG, "hci", "ACL register connection (handle: %#.4x)", connection->handle());
  auto [_, inserted] = registered_connections_.emplace(connection->handle(), connection);
  BT_ASSERT_MSG(inserted, "connection with handle %#.4x already registered", connection->handle());

  // Reset the round-robin iterators because they have been invalidated.
  ResetRoundRobinIterators();
}

void AclDataChannelImpl::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "ACL unregister link (handle: %#.4x)", handle);
  auto iter = registered_connections_.find(handle);
  if (iter == registered_connections_.end()) {
    bt_log(WARN, "hci", "attempt to unregister link that is not registered (handle: %#.4x)",
           handle);
    return;
  }
  registered_connections_.erase(iter);

  // Reset the round-robin iterators because they have been invalidated.
  ResetRoundRobinIterators();
}

bool AclDataChannelImpl::IsBrEdrBufferShared() const { return !le_buffer_info_.IsAvailable(); }

void AclDataChannelImpl::IncrementRoundRobinIterator(ConnectionMap::iterator& conn_iter,
                                                     bt::LinkType connection_type) {
  // Only update iterator if |registered_connections_| is non-empty
  if (conn_iter == registered_connections_.end()) {
    bt_log(DEBUG, "hci", "no registered connections, cannot increment iterator");
    return;
  }

  // Prevent infinite looping by tracking |original_conn_iter|
  const ConnectionMap::iterator original_conn_iter = conn_iter;
  do {
    conn_iter++;
    if (conn_iter == registered_connections_.end()) {
      conn_iter = registered_connections_.begin();
    }
  } while (!IsBrEdrBufferShared() && conn_iter->second->type() != connection_type &&
           conn_iter != original_conn_iter);

  // When buffer isn't shared, we must ensure |conn_iter| is assigned to a link of the same type.
  if (!IsBrEdrBufferShared() && conn_iter->second->type() != connection_type) {
    // There are no connections of |connection_type| in |registered_connections_|.
    conn_iter = registered_connections_.end();
  }
}

void AclDataChannelImpl::IncrementPendingPacketsForLink(WeakPtr<ConnectionInterface>& connection) {
  auto [iter, _] =
      pending_links_.try_emplace(connection->handle(), PendingPacketData{connection->type()});
  iter->second.count++;
  IncrementPendingPacketsForLinkType(connection->type());
}

void AclDataChannelImpl::SendPackets(ConnectionMap::iterator& current_link) {
  BT_DEBUG_ASSERT(current_link != registered_connections_.end());
  const ConnectionMap::iterator original_link = current_link;
  const LinkType link_type = original_link->second->type();
  size_t free_buffer_packets = GetNumFreePacketsForLinkType(link_type);
  bool is_packet_queued = true;

  // Send packets as long as a link may have a packet queued and buffer space is available.
  for (; free_buffer_packets != 0; IncrementRoundRobinIterator(current_link, link_type)) {
    if (current_link == original_link) {
      if (!is_packet_queued) {
        // All links are empty
        break;
      }
      is_packet_queued = false;
    }

    if (!current_link->second->HasAvailablePacket()) {
      continue;
    }

    // If there is an available packet, send and update packet counts
    ACLDataPacketPtr packet = current_link->second->GetNextOutboundPacket();
    BT_DEBUG_ASSERT(packet);
    hci_->SendAclData(packet->view().data().subspan());

    is_packet_queued = true;
    free_buffer_packets--;
    IncrementPendingPacketsForLink(current_link->second);
  }
}

void AclDataChannelImpl::TrySendNextPackets() {
  if (current_bredr_link_ != registered_connections_.end()) {
    // If the BR/EDR buffer is shared, this will also send LE packets.
    SendPackets(current_bredr_link_);
  }

  if (!IsBrEdrBufferShared() && current_le_link_ != registered_connections_.end()) {
    SendPackets(current_le_link_);
  }
}

void AclDataChannelImpl::OnOutboundPacketAvailable() { TrySendNextPackets(); }

void AclDataChannelImpl::AttachInspect(inspect::Node& parent, const std::string& name) {
  node_ = parent.CreateChild(std::move(name));

  bredr_subnode_ = node_.CreateChild("bredr");
  num_pending_bredr_packets_.AttachInspect(bredr_subnode_, "num_sent_packets");

  le_subnode_ = node_.CreateChild("le");
  num_pending_le_packets_.AttachInspect(le_subnode_, "num_sent_packets");
  le_subnode_shared_with_bredr_property_ =
      le_subnode_.CreateBool("independent_from_bredr", !IsBrEdrBufferShared());
}

void AclDataChannelImpl::SetDataRxHandler(ACLPacketHandler rx_callback) {
  BT_ASSERT(rx_callback);
  rx_callback_ = std::move(rx_callback);
  hci_->SetReceiveAclFunction(fit::bind_member<&AclDataChannelImpl::OnRxPacket>(this));
}

void AclDataChannelImpl::ClearControllerPacketCount(hci_spec::ConnectionHandle handle) {
  // Ensure link has already been unregistered. Otherwise, queued packets for this handle could be
  // sent after clearing packet count, and the packet count could become corrupted.
  BT_ASSERT(registered_connections_.find(handle) == registered_connections_.end());

  bt_log(DEBUG, "hci", "clearing pending packets (handle: %#.4x)", handle);

  // subtract removed packets from sent packet counts, because controller does not send HCI Number
  // of Completed Packets event for disconnected link
  auto iter = pending_links_.find(handle);
  if (iter == pending_links_.end()) {
    bt_log(DEBUG, "hci", "no pending packets on connection (handle: %#.4x)", handle);
    return;
  }

  const PendingPacketData& data = iter->second;
  DecrementPendingPacketsForLinkType(data.ll_type, data.count);

  pending_links_.erase(iter);

  // Try sending the next batch of packets in case buffer space opened up.
  TrySendNextPackets();
}

const DataBufferInfo& AclDataChannelImpl::GetBufferInfo() const { return bredr_buffer_info_; }

const DataBufferInfo& AclDataChannelImpl::GetLeBufferInfo() const {
  return !IsBrEdrBufferShared() ? le_buffer_info_ : bredr_buffer_info_;
}

void AclDataChannelImpl::RequestAclPriority(
    pw::bluetooth::AclPriority priority, hci_spec::ConnectionHandle handle,
    fit::callback<void(fit::result<fit::failed>)> callback) {
  bt_log(TRACE, "hci", "sending ACL priority command");

  hci_->EncodeVendorCommand(
      pw::bluetooth::SetAclPriorityCommandParameters{.connection_handle = handle,
                                                     .priority = priority},
      [this, priority, callback = std::move(callback)](
          pw::Result<pw::span<const std::byte>> encode_result) mutable {
        if (!encode_result.ok()) {
          bt_log(TRACE, "hci", "encoding ACL priority command failed");
          callback(fit::failed());
          return;
        }

        DynamicByteBuffer encoded(BufferView(encode_result->data(), encode_result->size()));
        if (encoded.size() < sizeof(hci_spec::CommandHeader)) {
          bt_log(TRACE, "hci", "encoded ACL priority command too small (size: %zu)",
                 encoded.size());
          callback(fit::failed());
          return;
        }

        hci_spec::OpCode op_code = letoh16(encoded.ReadMember<&hci_spec::CommandHeader::opcode>());
        auto packet =
            bt::hci::CommandPacket::New(op_code, encoded.size() - sizeof(hci_spec::CommandHeader));
        auto packet_view = packet->mutable_view()->mutable_data();
        encoded.Copy(&packet_view);

        transport_->command_channel()->SendCommand(
            std::move(packet),
            [cb = std::move(callback), priority](auto id, const hci::EventPacket& event) mutable {
              if (hci_is_error(event, WARN, "hci", "acl priority failed")) {
                cb(fit::failed());
                return;
              }

              bt_log(DEBUG, "hci", "acl priority updated (priority: %#.8x)",
                     static_cast<uint32_t>(priority));
              cb(fit::ok());
            });
      });
}

CommandChannel::EventCallbackResult AclDataChannelImpl::NumberOfCompletedPacketsCallback(
    const EventPacket& event) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kNumberOfCompletedPacketsEventCode);
  const auto& payload = event.params<hci_spec::NumberOfCompletedPacketsEventParams>();

  size_t handles_in_packet =
      (event.view().payload_size() - sizeof(hci_spec::NumberOfCompletedPacketsEventParams)) /
      sizeof(hci_spec::NumberOfCompletedPacketsEventData);

  if (payload.number_of_handles != handles_in_packet) {
    bt_log(WARN, "hci", "packets handle count (%d) doesn't match params size (%zu)",
           payload.number_of_handles, handles_in_packet);
  }

  for (uint8_t i = 0; i < payload.number_of_handles && i < handles_in_packet; ++i) {
    const hci_spec::NumberOfCompletedPacketsEventData* data = payload.data + i;

    auto iter = pending_links_.find(le16toh(data->connection_handle));
    if (iter == pending_links_.end()) {
      // This is expected if the completed packet is a SCO packet.
      bt_log(TRACE, "hci",
             "controller reported completed packets for connection handle without pending packets: "
             "%#.4x",
             data->connection_handle);
      continue;
    }

    uint16_t comp_packets = le16toh(data->hc_num_of_completed_packets);

    if (iter->second.count < comp_packets) {
      // TODO(fxbug.dev/2795): This can be caused by the controller reusing the connection handle
      // of a connection that just disconnected. We should somehow avoid sending the controller
      // packets for a connection that has disconnected. AclDataChannel already dequeues such
      // packets, but this is insufficient: packets can be queued in the channel to the transport
      // driver, and possibly in the transport driver or USB/UART drivers.
      bt_log(ERROR, "hci",
             "ACL packet tx count mismatch! (handle: %#.4x, expected: %zu, actual : %u)",
             le16toh(data->connection_handle), iter->second.count, comp_packets);
      // This should eventually result in convergence with the correct pending packet count. If it
      // undercounts the true number of pending packets, this branch will be reached again when
      // the controller sends an updated Number of Completed Packets event. However,
      // AclDataChannel may overflow the controller's buffer in the meantime!
      comp_packets = static_cast<uint16_t>(iter->second.count);
    }

    iter->second.count -= comp_packets;
    DecrementPendingPacketsForLinkType(iter->second.ll_type, comp_packets);
    if (!iter->second.count) {
      pending_links_.erase(iter);
    }
  }

  TrySendNextPackets();

  return CommandChannel::EventCallbackResult::kContinue;
}

size_t AclDataChannelImpl::GetNumFreePacketsForLinkType(LinkType link_type) const {
  if (link_type == LinkType::kACL || IsBrEdrBufferShared()) {
    BT_DEBUG_ASSERT(bredr_buffer_info_.max_num_packets() >= *num_pending_bredr_packets_);
    return bredr_buffer_info_.max_num_packets() - *num_pending_bredr_packets_;
  } else if (link_type == LinkType::kLE) {
    BT_DEBUG_ASSERT(le_buffer_info_.max_num_packets() >= *num_pending_le_packets_);
    return le_buffer_info_.max_num_packets() - *num_pending_le_packets_;
  }
  return 0;
}

void AclDataChannelImpl::DecrementPendingPacketsForLinkType(LinkType link_type, size_t count) {
  if (link_type == LinkType::kACL || IsBrEdrBufferShared()) {
    BT_DEBUG_ASSERT(*num_pending_bredr_packets_ >= count);
    *num_pending_bredr_packets_.Mutable() -= count;
  } else if (link_type == LinkType::kLE) {
    BT_DEBUG_ASSERT(*num_pending_le_packets_ >= count);
    *num_pending_le_packets_.Mutable() -= count;
  }
}

void AclDataChannelImpl::IncrementPendingPacketsForLinkType(LinkType link_type) {
  if (link_type == LinkType::kACL || IsBrEdrBufferShared()) {
    *num_pending_bredr_packets_.Mutable() += 1;
    BT_DEBUG_ASSERT(*num_pending_bredr_packets_ <= bredr_buffer_info_.max_num_packets());
  } else if (link_type == LinkType::kLE) {
    *num_pending_le_packets_.Mutable() += 1;
    BT_DEBUG_ASSERT(*num_pending_le_packets_ <= le_buffer_info_.max_num_packets());
  }
}

void AclDataChannelImpl::OnRxPacket(pw::span<const std::byte> buffer) {
  BT_ASSERT(rx_callback_);

  if (buffer.size() < sizeof(hci_spec::ACLDataHeader)) {
    // TODO(fxbug.dev/97362): Handle these types of errors by signaling Transport.
    bt_log(ERROR, "hci", "malformed packet - expected at least %zu bytes, got %zu",
           sizeof(hci_spec::ACLDataHeader), buffer.size());
    return;
  }

  const size_t payload_size = buffer.size() - sizeof(hci_spec::ACLDataHeader);

  ACLDataPacketPtr packet = ACLDataPacket::New(static_cast<uint16_t>(payload_size));
  packet->mutable_view()->mutable_data().Write(reinterpret_cast<const uint8_t*>(buffer.data()),
                                               buffer.size());
  packet->InitializeFromBuffer();

  if (packet->view().header().data_total_length != payload_size) {
    // TODO(fxbug.dev/97362): Handle these types of errors by signaling Transport.
    bt_log(ERROR, "hci",
           "malformed packet - payload size from header (%hu) does not match"
           " received payload size: %zu",
           packet->view().header().data_total_length, payload_size);
    return;
  }

  {
    TRACE_DURATION("bluetooth", "AclDataChannelImpl->rx_callback_");
    rx_callback_(std::move(packet));
  }
}

CommandChannel::EventCallbackResult AclDataChannelImpl::DataBufferOverflowCallback(
    const EventPacket& event) {
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kDataBufferOverflowEventCode);

  const auto& params = event.params<hci_spec::DataBufferOverflowEventParams>();

  // Internal buffer state must be invalid and no further transmissions are possible.
  BT_PANIC("controller data buffer overflow event received (link type: %hhu)",
           static_cast<unsigned char>(params.ll_type));

  return CommandChannel::EventCallbackResult::kContinue;
}

void AclDataChannelImpl::ResetRoundRobinIterators() {
  current_bredr_link_ = registered_connections_.begin();

  // If the BR/EDR buffer isn't shared, we need to do extra work to ensure |current_bredr_link_| is
  // initialized to a link of BR/EDR type. The same applies for |current_le_link_|.
  if (!IsBrEdrBufferShared()) {
    current_le_link_ = registered_connections_.begin();

    IncrementRoundRobinIterator(current_bredr_link_, bt::LinkType::kACL);
    IncrementRoundRobinIterator(current_le_link_, bt::LinkType::kLE);
  }
}

}  // namespace bt::hci
