// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "acl_data_channel.h"

#include <endian.h>
#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/fit/defer.h>

#include <iterator>
#include <numeric>

#include "lib/fit/function.h"
#include "pw_bluetooth/vendor.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/pipeline_monitor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/retire_log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/windowed_inspect_numeric_property.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "transport.h"

namespace bt::hci {

class AclDataChannelImpl final : public AclDataChannel {
 public:
  AclDataChannelImpl(Transport* transport, pw::bluetooth::Controller* hci,
                     const DataBufferInfo& bredr_buffer_info, const DataBufferInfo& le_buffer_info);
  ~AclDataChannelImpl() override;

  // AclDataChannel overrides
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
  // Handler for the HCI Number of Completed Packets Event, used for packet-based data flow control.
  CommandChannel::EventCallbackResult NumberOfCompletedPacketsCallback(const EventPacket& event);

  // Sends next queued packets over the ACL data channel while the controller has free buffer slots
  // If controller buffers are free and some links have queued packets, we round-robin iterate
  // through links, sending a packet from each link with queued packets until the controller is full
  // or we run out of packets
  void TrySendNextPackets();

  // Returns the number of BR/EDR packets for which the controller has available space to buffer.
  size_t GetNumFreeBREDRPackets() const;

  // Returns the number of LE packets for which controller has available space to buffer.
  // This will be 0 if the BR/EDR buffer is shared with LE.
  size_t GetNumFreeLEPackets() const;

  // Decreases the total number of sent packets count by the given amount.
  void DecrementTotalNumPackets(size_t count);

  // Decreases the total number of sent packets count for LE by the given amount.
  void DecrementLETotalNumPackets(size_t count);

  // Increments the total number of sent packets count by the given amount.
  void IncrementTotalNumPackets(size_t count);

  // Increments the total number of sent LE packets count by the given amount.
  void IncrementLETotalNumPackets(size_t count);

  // Returns true if the LE data buffer is not available
  bool IsBrEdrBufferShared() const;

  void OnRxPacket(pw::span<const std::byte> packet);

  using ConnectionMap =
      std::unordered_map<hci_spec::ConnectionHandle, WeakPtr<ConnectionInterface>>;

  // Update logical link iterators using round robin scheduling.
  // If the BR/EDR buffer is shared, we can scan through the map consecutively by simply
  // incrementing the iterator to the next link in the map (either kACL or kLE type).
  // If the BR/EDR buffer isn't shared, we need to do extra work to ensure
  // |current_bredr_link_| is initialized to a link of BR/EDR type.
  // The same applies for |current_le_link_|.
  void IncrementRoundRobinIterator(ConnectionMap::iterator& conn_iter,
                                   bt::LinkType connection_type);

  // Increments count of pending packets that have been sent to the controller on connection
  void IncrementPendingPacketsForLink(WeakPtr<ConnectionInterface>& connection);

  // Returns number of packets sent
  size_t SendPackets(size_t avail_packets, ConnectionMap::iterator& current_link);

  // Handler for HCI_Buffer_Overflow_event.
  CommandChannel::EventCallbackResult DataBufferOverflowCallback(const EventPacket& event);

  // Links this node to the inspect tree. Initialized as needed by AttachInspect.
  inspect::Node node_;

  // Contents of |node_|. Retained as members so that they last as long as a class instance.
  inspect::Node le_subnode_;
  inspect::BoolProperty le_subnode_shared_with_bredr_property_;
  inspect::Node bredr_subnode_;

  // The Transport object that owns this instance.
  Transport* transport_;  // weak;

  // Controller is owned by Transport and will outlive this object.
  pw::bluetooth::Controller* hci_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_ = 0;

  // The event handler ID for the Data Buffer Overflow event.
  CommandChannel::EventHandlerId data_buffer_overflow_event_handler_id_ = 0;

  // The dispatcher used for posting tasks on the HCI transport I/O thread.
  async_dispatcher_t* io_dispatcher_;

  // The current handler for incoming data.
  ACLPacketHandler rx_callback_;

  // BR/EDR data buffer information. This buffer will not be available on LE-only controllers.
  DataBufferInfo bredr_buffer_info_;

  // LE data buffer information. This buffer will not be available on BR/EDR-only controllers (which
  // we do not support) and MAY be available on dual-mode controllers. We maintain that if this
  // buffer is not available, then the BR/EDR buffer MUST be available.
  DataBufferInfo le_buffer_info_;

  // The current count of the number of ACL data packets that have been sent to the controller.
  // |le_num_sent_packets_| is ignored if the controller uses one buffer for LE and BR/EDR.
  UintInspectable<size_t> num_sent_packets_;
  UintInspectable<size_t> le_num_sent_packets_;

  // Stores per-connection information of unacknowledged packets sent to the controller. Entries are
  // updated/removed on the HCI Number Of Completed Packets event and when a connection is
  // unregistered (the controller does not acknowledge packets of disconnected links).
  struct PendingPacketData {
    PendingPacketData() = default;

    // We initialize the packet count at 1 since a new entry will only be created once.
    explicit PendingPacketData(bt::LinkType ll_type) : ll_type(ll_type), count(0u) {}

    bt::LinkType ll_type;
    size_t count;
  };
  std::unordered_map<hci_spec::ConnectionHandle, PendingPacketData> pending_links_;

  // Stores connections registered by RegisterConnection.
  ConnectionMap registered_connections_;

  // Iterators used to round robin through links for sending packets.
  // When the BR/EDR buffer is shared, |current_le_link_| is ignored
  ConnectionMap::iterator current_bredr_link_;
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
      io_dispatcher_(async_get_default_dispatcher()),
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

  bt_log(DEBUG, "hci", "initialized");
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

  current_bredr_link_ = registered_connections_.begin();

  // If the BR/EDR buffer isn't shared, we need to do extra work to ensure
  // |current_bredr_link_| is initialized to a link of BR/EDR type.
  // The same applies for |current_le_link_|.
  if (!IsBrEdrBufferShared()) {
    current_le_link_ = registered_connections_.begin();

    IncrementRoundRobinIterator(current_bredr_link_, bt::LinkType::kACL);
    IncrementRoundRobinIterator(current_le_link_, bt::LinkType::kLE);
  }
}

void AclDataChannelImpl::UnregisterConnection(hci_spec::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "ACL unregister link (handle: %#.4x)", handle);
  auto iter = registered_connections_.find(handle);
  if (iter == registered_connections_.end()) {
    // handle not registered
    bt_log(WARN, "hci", "attempt to unregister link that is not registered (handle: %#.4x)",
           handle);
    return;
  }
  registered_connections_.erase(iter);

  current_bredr_link_ = registered_connections_.begin();

  // If the BR/EDR buffer isn't shared, we need to do extra work to ensure
  // |current_bredr_link_| is initialized to a link of BR/EDR type.
  // The same applies for |current_le_link_|.
  if (!IsBrEdrBufferShared()) {
    current_le_link_ = registered_connections_.begin();

    IncrementRoundRobinIterator(current_bredr_link_, bt::LinkType::kACL);
    IncrementRoundRobinIterator(current_le_link_, bt::LinkType::kLE);
  }
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
  ConnectionMap::iterator original_conn_iter = conn_iter;
  do {
    if (std::next(conn_iter) == registered_connections_.end()) {
      conn_iter = registered_connections_.begin();
    } else {
      conn_iter++;
    }
  } while (!IsBrEdrBufferShared() && conn_iter->second->type() != connection_type &&
           conn_iter != original_conn_iter);

  // When buffer isn't shared, we must ensure |conn_iter| is assigned to a link of the same type
  // There are no connections of |connection_type| in |registered_connections_|
  if (!IsBrEdrBufferShared() && conn_iter->second->type() != connection_type) {
    conn_iter = registered_connections_.end();
  }
}

void AclDataChannelImpl::IncrementPendingPacketsForLink(WeakPtr<ConnectionInterface>& connection) {
  auto [iter, _] =
      pending_links_.try_emplace(connection->handle(), PendingPacketData(connection->type()));
  iter->second.count++;
}

size_t AclDataChannelImpl::SendPackets(size_t avail_packets,
                                       ConnectionMap::iterator& current_link) {
  bool is_packet_queued = true;
  size_t num_packets_sent = 0;
  ConnectionMap::iterator original_link = current_link;

  // Only send packets if there is buffer space available
  while (avail_packets) {
    if (current_link == original_link) {
      if (!is_packet_queued) {  // All links are empty
        return num_packets_sent;
      }
      is_packet_queued = false;
    }

    if (!current_link->second->HasAvailablePacket()) {
      IncrementRoundRobinIterator(current_link, current_link->second->type());
      continue;
    }

    // If there is an available packet, send and update packet counts
    ACLDataPacketPtr packet = current_link->second->GetNextOutboundPacket();
    BT_DEBUG_ASSERT(packet);
    hci_->SendAclData(packet->view().data().subspan());

    is_packet_queued = true;
    avail_packets--;
    num_packets_sent++;
    IncrementPendingPacketsForLink(current_link->second);
    IncrementRoundRobinIterator(current_link, current_link->second->type());
  }
  return num_packets_sent;
}

void AclDataChannelImpl::TrySendNextPackets() {
  size_t avail_bredr_packets = GetNumFreeBREDRPackets();

  // On a dual mode controller that does not implement separate buffers, the BR/EDR buffer is
  // shared for both technologies, so we must take care not to double count here.
  size_t num_free_le_packets = GetNumFreeLEPackets();
  size_t& avail_le_packets = IsBrEdrBufferShared() ? avail_bredr_packets : num_free_le_packets;

  if (current_bredr_link_ != registered_connections_.end()) {
    size_t bredr_packets_sent = SendPackets(avail_bredr_packets, current_bredr_link_);
    IncrementTotalNumPackets(bredr_packets_sent);
  }

  if (!IsBrEdrBufferShared() && current_le_link_ != registered_connections_.end()) {
    size_t le_packets_sent = SendPackets(avail_le_packets, current_le_link_);
    IncrementLETotalNumPackets(le_packets_sent);
  }
}

void AclDataChannelImpl::OnOutboundPacketAvailable() { TrySendNextPackets(); }

void AclDataChannelImpl::AttachInspect(inspect::Node& parent, const std::string& name) {
  node_ = parent.CreateChild(std::move(name));

  bredr_subnode_ = node_.CreateChild("bredr");
  num_sent_packets_.AttachInspect(bredr_subnode_, "num_sent_packets");

  le_subnode_ = node_.CreateChild("le");
  le_num_sent_packets_.AttachInspect(le_subnode_, "num_sent_packets");
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
  if (data.ll_type == bt::LinkType::kLE) {
    DecrementLETotalNumPackets(data.count);
  } else {
    DecrementTotalNumPackets(data.count);
  }

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
  BT_DEBUG_ASSERT(async_get_default_dispatcher() == io_dispatcher_);
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kNumberOfCompletedPacketsEventCode);

  const auto& payload = event.params<hci_spec::NumberOfCompletedPacketsEventParams>();
  size_t total_comp_packets = 0;
  size_t le_total_comp_packets = 0;

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

    if (iter->second.ll_type == bt::LinkType::kACL) {
      total_comp_packets += comp_packets;
    } else {
      le_total_comp_packets += comp_packets;
    }

    if (!iter->second.count) {
      pending_links_.erase(iter);
    }
  }

  DecrementTotalNumPackets(total_comp_packets);
  DecrementLETotalNumPackets(le_total_comp_packets);
  TrySendNextPackets();

  return CommandChannel::EventCallbackResult::kContinue;
}

size_t AclDataChannelImpl::GetNumFreeBREDRPackets() const {
  BT_DEBUG_ASSERT(bredr_buffer_info_.max_num_packets() >= *num_sent_packets_);
  return bredr_buffer_info_.max_num_packets() - *num_sent_packets_;
}

size_t AclDataChannelImpl::GetNumFreeLEPackets() const {
  if (IsBrEdrBufferShared()) {
    return 0;
  }

  BT_DEBUG_ASSERT(le_buffer_info_.max_num_packets() >= *le_num_sent_packets_);
  return le_buffer_info_.max_num_packets() - *le_num_sent_packets_;
}

void AclDataChannelImpl::DecrementTotalNumPackets(size_t count) {
  BT_DEBUG_ASSERT(*num_sent_packets_ >= count);
  *num_sent_packets_.Mutable() -= count;
}

void AclDataChannelImpl::DecrementLETotalNumPackets(size_t count) {
  if (IsBrEdrBufferShared()) {
    DecrementTotalNumPackets(count);
    return;
  }

  BT_DEBUG_ASSERT(*le_num_sent_packets_ >= count);
  *le_num_sent_packets_.Mutable() -= count;
}

void AclDataChannelImpl::IncrementTotalNumPackets(size_t count) {
  BT_DEBUG_ASSERT(*num_sent_packets_ + count <= bredr_buffer_info_.max_num_packets());
  *num_sent_packets_.Mutable() += count;
}

void AclDataChannelImpl::IncrementLETotalNumPackets(size_t count) {
  if (IsBrEdrBufferShared()) {
    IncrementTotalNumPackets(count);
    return;
  }

  BT_DEBUG_ASSERT(*le_num_sent_packets_ + count <= le_buffer_info_.max_num_packets());
  *le_num_sent_packets_.Mutable() += count;
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

  const auto& params = event.params<hci_spec::ConnectionRequestEventParams>();

  // Internal buffer state must be invalid and no further transmissions are possible.
  BT_PANIC("controller data buffer overflow event received (link type: %hhu)", params.link_type);

  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
