// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "acl_data_channel.h"

#include <endian.h>
#include <lib/async/default.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include "lib/fit/function.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/run_task_sync.h"
#include "src/lib/fxl/strings/string_printf.h"
#include "transport.h"

namespace bt::hci {

DataBufferInfo::DataBufferInfo(size_t max_data_length, size_t max_num_packets)
    : max_data_length_(max_data_length), max_num_packets_(max_num_packets) {}

DataBufferInfo::DataBufferInfo() : max_data_length_(0u), max_num_packets_(0u) {}

bool DataBufferInfo::operator==(const DataBufferInfo& other) const {
  return max_data_length_ == other.max_data_length_ && max_num_packets_ == other.max_num_packets_;
}

zx_status_t AclDataChannel::ReadAclDataPacketFromChannel(const zx::channel& channel,
                                                         const ACLDataPacketPtr& packet) {
  uint32_t read_size;
  auto packet_bytes = packet->mutable_view()->mutable_data();
  zx_status_t read_status = channel.read(0u, packet_bytes.mutable_data(), nullptr,
                                         packet_bytes.size(), 0, &read_size, nullptr);
  if (read_status < 0) {
    bt_log(DEBUG, "hci", "failed to read RX bytes: %s", zx_status_get_string(read_status));
    // Clear the handler so that we stop receiving events from it.
    // TODO(jamuraa): signal failure to the consumer so it can do something.
    return ZX_ERR_IO;
  }

  if (read_size < sizeof(ACLDataHeader)) {
    bt_log(ERROR, "hci", "malformed data packet - expected at least %zu bytes, got %u",
           sizeof(ACLDataHeader), read_size);
    // TODO(jamuraa): signal stream error somehow
    return ZX_ERR_INVALID_ARGS;
  }

  const size_t rx_payload_size = read_size - sizeof(ACLDataHeader);
  const size_t size_from_header = le16toh(packet->view().header().data_total_length);
  if (size_from_header != rx_payload_size) {
    bt_log(ERROR, "hci",
           "malformed packet - payload size from header (%zu) does not match"
           " received payload size: %zu",
           size_from_header, rx_payload_size);
    // TODO(jamuraa): signal stream error somehow
    return ZX_ERR_INVALID_ARGS;
  }

  packet->InitializeFromBuffer();
  return ZX_OK;
}

class AclDataChannelImpl final : public AclDataChannel {
 public:
  AclDataChannelImpl(Transport* transport, zx::channel hci_acl_channel);
  ~AclDataChannelImpl() override;

  // AclDataChannel overrides
  void Initialize(const DataBufferInfo& bredr_buffer_info,
                  const DataBufferInfo& le_buffer_info) override;
  void ShutDown() override;
  void SetDataRxHandler(ACLPacketHandler rx_callback) override;
  bool SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                  PacketPriority priority) override;
  bool SendPackets(LinkedList<ACLDataPacket> packets, UniqueChannelId channel_id,
                   PacketPriority priority) override;
  void RegisterLink(hci::ConnectionHandle handle, Connection::LinkType ll_type) override;
  void UnregisterLink(hci::ConnectionHandle handle) override;
  void DropQueuedPackets(AclPacketPredicate predicate) override;
  void ClearControllerPacketCount(hci::ConnectionHandle handle) override;
  const DataBufferInfo& GetBufferInfo() const override;
  const DataBufferInfo& GetLeBufferInfo() const override;
  void RequestAclPriority(hci::AclPriority priority, hci::ConnectionHandle handle,
                          fit::callback<void(fit::result<>)> callback) override;

 private:
  // Represents a queued ACL data packet.
  struct QueuedDataPacket {
    QueuedDataPacket(Connection::LinkType ll_type, UniqueChannelId channel_id,
                     PacketPriority priority, ACLDataPacketPtr packet)
        : ll_type(ll_type), channel_id(channel_id), priority(priority), packet(std::move(packet)) {}

    QueuedDataPacket() = default;
    QueuedDataPacket(QueuedDataPacket&& other) = default;
    QueuedDataPacket& operator=(QueuedDataPacket&& other) = default;

    Connection::LinkType ll_type;
    UniqueChannelId channel_id;
    PacketPriority priority;
    ACLDataPacketPtr packet;
  };

  // Returns the data buffer MTU for the given connection.
  size_t GetBufferMtu(Connection::LinkType ll_type) const;

  // Handler for the HCI Number of Completed Packets Event, used for
  // packet-based data flow control.
  CommandChannel::EventCallbackResult NumberOfCompletedPacketsCallback(const EventPacket& event);

  // Tries to send the next batch of queued data packets if the controller has
  // any space available. All packets in higher priority queues will be sent before packets in lower
  // priority queues.
  void TrySendNextQueuedPackets();

  // Returns the number of BR/EDR packets for which the controller has available
  // space to buffer.
  size_t GetNumFreeBREDRPackets() const;

  // Returns the number of LE packets for which controller has available space
  // to buffer.
  size_t GetNumFreeLEPackets() const;

  // Decreases the total number of sent packets count by the given amount.
  void DecrementTotalNumPackets(size_t count);

  // Decreases the total number of sent packets count for LE by the given
  // amount.
  void DecrementLETotalNumPackets(size_t count);

  // Increments the total number of sent packets count by the given amount.
  void IncrementTotalNumPackets(size_t count);

  // Increments the total number of sent LE packets count by the given amount.
  void IncrementLETotalNumPackets(size_t count);

  // Read Ready Handler for |channel_|
  void OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                      const zx_packet_signal_t* signal);

  // Handler for HCI_Buffer_Overflow_event
  CommandChannel::EventCallbackResult DataBufferOverflowCallback(const EventPacket& event);

  // Used to assert that certain public functions are only called on the
  // creation thread.
  fit::thread_checker thread_checker_;

  // The Transport object that owns this instance.
  Transport* transport_;  // weak;

  // The channel that we use to send/receive HCI ACL data packets.
  zx::channel channel_;

  // Wait object for |channel_|
  async::WaitMethod<AclDataChannelImpl, &AclDataChannelImpl::OnChannelReady> channel_wait_{this};

  // True if this instance has been initialized through a call to Initialize().
  std::atomic_bool is_initialized_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_;

  // The event handler ID for the Data Buffer Overflow event.
  CommandChannel::EventHandlerId data_buffer_overflow_event_handler_id_;

  // The dispatcher used for posting tasks on the HCI transport I/O thread.
  async_dispatcher_t* io_dispatcher_;

  // The current handler for incoming data.
  ACLPacketHandler rx_callback_;

  // BR/EDR data buffer information. This buffer will not be available on
  // LE-only controllers.
  DataBufferInfo bredr_buffer_info_;

  // LE data buffer information. This buffer will not be available on
  // BR/EDR-only controllers (which we do not support) and MAY be available on
  // dual-mode controllers. We maintain that if this buffer is not available,
  // then the BR/EDR buffer MUST be available.
  DataBufferInfo le_buffer_info_;

  // The current count of the number of ACL data packets that have been sent to
  // the controller. |le_num_sent_packets_| is ignored if the controller uses
  // one buffer for LE and BR/EDR.
  size_t num_sent_packets_;
  size_t le_num_sent_packets_;

  // The ACL data packet queue contains the data packets that are waiting to be
  // sent to the controller.
  // TODO(armansito): Use priority_queue based on L2CAP channel priority.
  // TODO(fxbug.dev/944): Keep a separate queue for each open connection. Benefits:
  //   * Helps address the packet-prioritization TODO above.
  //   * Also: having separate queues, which know their own
  //     Connection::LinkType, would let us replace std::list<QueuedDataPacket>
  //     with LinkedList<ACLDataPacket> which has a more efficient
  //     memory layout.
  using DataPacketQueue = std::list<QueuedDataPacket>;
  DataPacketQueue send_queue_;

  // Returns an iterator to the location new packets should be inserted into |send_queue_| based on
  // their |priority|:
  // If |priority| is |kLow|: returns past-the-end of |send_queue_|.
  // If |priority| is |kHigh|: returns the location of the first |kLow| priority packet.
  DataPacketQueue::iterator SendQueueInsertLocationForPriority(PacketPriority priority);

  // Stores the link type of connections on which we have a pending packet that
  // has been sent to the controller. Entries are removed on the HCI Number Of
  // Completed Packets event.
  struct PendingPacketData {
    PendingPacketData() = default;

    // We initialize the packet count at 1 since a new entry will only be
    // created once.
    PendingPacketData(Connection::LinkType ll_type) : ll_type(ll_type), count(1u) {}

    Connection::LinkType ll_type;
    size_t count;
  };
  std::unordered_map<ConnectionHandle, PendingPacketData> pending_links_;

  // Stores links registered by RegisterLink
  std::unordered_map<hci::ConnectionHandle, Connection::LinkType> registered_links_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AclDataChannelImpl);
};

std::unique_ptr<AclDataChannel> AclDataChannel::Create(Transport* transport,
                                                       zx::channel hci_acl_channel) {
  return std::make_unique<AclDataChannelImpl>(transport, std::move(hci_acl_channel));
}

AclDataChannelImpl::AclDataChannelImpl(Transport* transport, zx::channel hci_acl_channel)
    : transport_(transport),
      channel_(std::move(hci_acl_channel)),
      channel_wait_(this, channel_.get(), ZX_CHANNEL_READABLE),
      is_initialized_(false),
      num_completed_packets_event_handler_id_(0u),
      data_buffer_overflow_event_handler_id_(0u),
      io_dispatcher_(nullptr),
      num_sent_packets_(0u),
      le_num_sent_packets_(0u) {
  // TODO(armansito): We'll need to pay attention to ZX_CHANNEL_WRITABLE as
  // well.
  ZX_DEBUG_ASSERT(transport_);
  ZX_DEBUG_ASSERT(channel_.is_valid());
}

AclDataChannelImpl::~AclDataChannelImpl() {
  // Do nothing. Since Transport is shared across threads, this can be called
  // from any thread and calling ShutDown() would be unsafe.
}

void AclDataChannelImpl::Initialize(const DataBufferInfo& bredr_buffer_info,
                                    const DataBufferInfo& le_buffer_info) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  ZX_DEBUG_ASSERT(!is_initialized_);
  ZX_DEBUG_ASSERT(bredr_buffer_info.IsAvailable() || le_buffer_info.IsAvailable());

  bredr_buffer_info_ = bredr_buffer_info;
  le_buffer_info_ = le_buffer_info;

  auto setup_handler_task = [this] {
    zx_status_t status = channel_wait_.Begin(async_get_default_dispatcher());
    if (status != ZX_OK) {
      bt_log(ERROR, "hci", "failed channel setup %s", zx_status_get_string(status));
      channel_wait_.set_object(ZX_HANDLE_INVALID);
      return;
    }
    bt_log(DEBUG, "hci", "started I/O handler");
  };

  io_dispatcher_ = async_get_default_dispatcher();
  RunTaskSync(setup_handler_task, io_dispatcher_);

  // TODO(jamuraa): return whether we successfully initialized?
  if (channel_wait_.object() == ZX_HANDLE_INVALID)
    return;

  num_completed_packets_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      kNumberOfCompletedPacketsEventCode,
      fit::bind_member(this, &AclDataChannelImpl::NumberOfCompletedPacketsCallback));
  ZX_DEBUG_ASSERT(num_completed_packets_event_handler_id_);

  data_buffer_overflow_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      kDataBufferOverflowEventCode,
      fit::bind_member(this, &AclDataChannelImpl::DataBufferOverflowCallback));
  ZX_DEBUG_ASSERT(data_buffer_overflow_event_handler_id_);

  is_initialized_ = true;

  bt_log(INFO, "hci", "initialized");
}

void AclDataChannelImpl::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  if (!is_initialized_)
    return;

  bt_log(INFO, "hci", "shutting down");

  auto handler_cleanup_task = [this] {
    bt_log(DEBUG, "hci", "removing I/O handler");
    zx_status_t status = channel_wait_.Cancel();
    if (status != ZX_OK) {
      bt_log(WARN, "hci", "couldn't cancel wait on channel: %s", zx_status_get_string(status));
    }
  };

  RunTaskSync(handler_cleanup_task, io_dispatcher_);

  transport_->command_channel()->RemoveEventHandler(num_completed_packets_event_handler_id_);
  transport_->command_channel()->RemoveEventHandler(data_buffer_overflow_event_handler_id_);

  is_initialized_ = false;
  send_queue_.clear();
  io_dispatcher_ = nullptr;
  num_completed_packets_event_handler_id_ = 0u;
  data_buffer_overflow_event_handler_id_ = 0u;
  SetDataRxHandler(nullptr);
}

void AclDataChannelImpl::SetDataRxHandler(ACLPacketHandler rx_callback) {
  rx_callback_ = std::move(rx_callback);
}

bool AclDataChannelImpl::SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                                    PacketPriority priority) {
  if (!is_initialized_) {
    bt_log(DEBUG, "hci", "cannot send packets while uninitialized");
    return false;
  }

  ZX_DEBUG_ASSERT(data_packet);

  const auto handle = data_packet->connection_handle();

  auto link_iter = registered_links_.find(handle);

  if (link_iter == registered_links_.end()) {
    bt_log(TRACE, "hci", "dropping packet for unregistered connection (handle: %#.4x)", handle);
    return false;
  }

  Connection::LinkType ll_type = link_iter->second;

  if (data_packet->view().payload_size() > GetBufferMtu(ll_type)) {
    bt_log(ERROR, "hci", "ACL data packet too large!");
    return false;
  }

  send_queue_.insert(SendQueueInsertLocationForPriority(priority),
                     QueuedDataPacket(ll_type, channel_id, priority, std::move(data_packet)));

  TrySendNextQueuedPackets();

  return true;
}

bool AclDataChannelImpl::SendPackets(LinkedList<ACLDataPacket> packets, UniqueChannelId channel_id,
                                     PacketPriority priority) {
  if (!is_initialized_) {
    bt_log(DEBUG, "hci", "cannot send packets while uninitialized");
    return false;
  }

  if (packets.is_empty()) {
    bt_log(DEBUG, "hci", "no packets to send!");
    return false;
  }

  auto handle = packets.front().connection_handle();

  if (registered_links_.find(handle) == registered_links_.end()) {
    bt_log(TRACE, "hci", "dropping packets for unregistered connection (handle: %#.4x, count: %lu)",
           handle, packets.size_slow());
    return false;
  }

  for (const auto& packet : packets) {
    // Make sure that all packets have registered connection handles.
    if (registered_links_.find(packet.connection_handle()) == registered_links_.end()) {
      bt_log(TRACE, "hci",
             "dropping packets for unregistered connection (handle: %#.4x, count: %lu)",
             packet.connection_handle(), packets.size_slow());
      return false;
    }

    // Make sure that all packets are within the MTU.
    if (packet.view().payload_size() >
        GetBufferMtu(registered_links_[packet.connection_handle()])) {
      bt_log(ERROR, "hci", "ACL data packet too large!");
      return false;
    }
  }

  auto insert_iter = SendQueueInsertLocationForPriority(priority);
  while (!packets.is_empty()) {
    auto packet = packets.pop_front();
    auto ll_type = registered_links_[packet->connection_handle()];
    auto queue_packet = QueuedDataPacket(ll_type, channel_id, priority, std::move(packet));
    send_queue_.insert(insert_iter, std::move(queue_packet));
  }

  TrySendNextQueuedPackets();

  return true;
}

void AclDataChannelImpl::RegisterLink(hci::ConnectionHandle handle, Connection::LinkType ll_type) {
  bt_log(DEBUG, "hci", "ACL register link (handle: %#.4x)", handle);
  ZX_DEBUG_ASSERT(registered_links_.find(handle) == registered_links_.end());
  registered_links_[handle] = ll_type;
}

void AclDataChannelImpl::UnregisterLink(hci::ConnectionHandle handle) {
  bt_log(DEBUG, "hci", "ACL unregister link (handle: %#.4x)", handle);

  if (registered_links_.erase(handle) == 0) {
    // handle not registered
    bt_log(WARN, "hci", "attempt to unregister link that is not registered (handle: %#.4x)",
           handle);
    return;
  }

  // remove packets with matching connection handle in send queue
  auto filter = [handle](const ACLDataPacketPtr& packet, UniqueChannelId channel_id) {
    return packet->connection_handle() == handle;
  };
  DropQueuedPackets(filter);
}

void AclDataChannelImpl::DropQueuedPackets(AclPacketPredicate predicate) {
  const size_t before_count = send_queue_.size();
  send_queue_.remove_if([&predicate](const QueuedDataPacket& packet) {
    return predicate(packet.packet, packet.channel_id);
  });
  const size_t removed_count = before_count - send_queue_.size();
  if (removed_count > 0) {
    bt_log(TRACE, "hci", "packets dropped from send queue (count: %lu)", removed_count);
  }
}

void AclDataChannelImpl::ClearControllerPacketCount(hci::ConnectionHandle handle) {
  // Ensure link has already been unregistered. Otherwise, queued packets for this handle
  // could be sent after clearing packet count, and the packet count could become corrupted.
  ZX_ASSERT(registered_links_.find(handle) == registered_links_.end());

  bt_log(DEBUG, "hci", "clearing pending packets (handle: %#.4x)", handle);

  // subtract removed packets from sent packet counts, because controller
  // does not send HCI Number of Completed Packets event for disconnected link
  auto iter = pending_links_.find(handle);
  if (iter == pending_links_.end()) {
    bt_log(DEBUG, "hci", "no pending packets on connection (handle: %#.4x)", handle);
    return;
  }

  const PendingPacketData& data = iter->second;
  if (data.ll_type == Connection::LinkType::kLE) {
    DecrementLETotalNumPackets(data.count);
  } else {
    DecrementTotalNumPackets(data.count);
  }

  pending_links_.erase(iter);

  // Try sending the next batch of packets in case buffer space opened up.
  TrySendNextQueuedPackets();
}

const DataBufferInfo& AclDataChannelImpl::GetBufferInfo() const { return bredr_buffer_info_; }

const DataBufferInfo& AclDataChannelImpl::GetLeBufferInfo() const {
  return le_buffer_info_.IsAvailable() ? le_buffer_info_ : bredr_buffer_info_;
}

void AclDataChannelImpl::RequestAclPriority(hci::AclPriority priority, hci::ConnectionHandle handle,
                                            fit::callback<void(fit::result<>)> callback) {
  bt_log(TRACE, "hci", "sending ACL priority command");

  bt_vendor_set_acl_priority_params_t priority_params = {
      .connection_handle = handle,
      .priority = static_cast<bt_vendor_acl_priority_t>((priority == AclPriority::kNormal)
                                                            ? BT_VENDOR_ACL_PRIORITY_NORMAL
                                                            : BT_VENDOR_ACL_PRIORITY_HIGH),
      .direction = static_cast<bt_vendor_acl_direction_t>((priority == AclPriority::kSource)
                                                              ? BT_VENDOR_ACL_DIRECTION_SOURCE
                                                              : BT_VENDOR_ACL_DIRECTION_SINK)};
  bt_vendor_params_t cmd_params = {.set_acl_priority = priority_params};
  auto encode_result =
      transport_->EncodeVendorCommand(BT_VENDOR_COMMAND_SET_ACL_PRIORITY, cmd_params);
  if (encode_result.is_error()) {
    bt_log(TRACE, "hci", "encoding ACL priority command failed");
    callback(fit::error());
    return;
  }
  auto encoded = encode_result.take_value();
  if (encoded.size() < sizeof(hci::CommandHeader)) {
    bt_log(TRACE, "hci", "encoded ACL priority command too small (size: %zu)", encoded.size());
    callback(fit::error());
    return;
  }

  hci::OpCode op_code = letoh16(encoded.As<hci::CommandHeader>().opcode);
  auto packet = bt::hci::CommandPacket::New(op_code, encoded.size() - sizeof(hci::CommandHeader));
  auto packet_view = packet->mutable_view()->mutable_data();
  encoded.Copy(&packet_view);

  transport_->command_channel()->SendCommand(
      std::move(packet),
      [cb = std::move(callback), priority](auto id, const hci::EventPacket& event) mutable {
        if (hci_is_error(event, WARN, "hci", "acl priority failed")) {
          cb(fit::error());
          return;
        }

        bt_log(DEBUG, "hci", "acl priority updated (priority: %#.8x)",
               static_cast<uint32_t>(priority));
        cb(fit::ok());
      });
}

size_t AclDataChannelImpl::GetBufferMtu(Connection::LinkType ll_type) const {
  if (ll_type == Connection::LinkType::kACL)
    return bredr_buffer_info_.max_data_length();
  return GetLeBufferInfo().max_data_length();
}

CommandChannel::EventCallbackResult AclDataChannelImpl::NumberOfCompletedPacketsCallback(
    const EventPacket& event) {
  if (!is_initialized_) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  ZX_DEBUG_ASSERT(async_get_default_dispatcher() == io_dispatcher_);
  ZX_DEBUG_ASSERT(event.event_code() == kNumberOfCompletedPacketsEventCode);

  const auto& payload = event.params<NumberOfCompletedPacketsEventParams>();
  size_t total_comp_packets = 0;
  size_t le_total_comp_packets = 0;

  size_t handles_in_packet =
      (event.view().payload_size() - sizeof(NumberOfCompletedPacketsEventParams)) /
      sizeof(NumberOfCompletedPacketsEventData);

  if (payload.number_of_handles != handles_in_packet) {
    bt_log(WARN, "hci", "packets handle count (%d) doesn't match params size (%zu)",
           payload.number_of_handles, handles_in_packet);
  }

  for (uint8_t i = 0; i < payload.number_of_handles && i < handles_in_packet; ++i) {
    const NumberOfCompletedPacketsEventData* data = payload.data + i;

    auto iter = pending_links_.find(le16toh(data->connection_handle));
    if (iter == pending_links_.end()) {
      bt_log(WARN, "hci", "controller reported sent packets on unknown connection handle!");
      continue;
    }

    uint16_t comp_packets = le16toh(data->hc_num_of_completed_packets);

    ZX_DEBUG_ASSERT(iter->second.count);
    if (iter->second.count < comp_packets) {
      bt_log(WARN, "hci",
             "packet tx count mismatch! (handle: %#.4x, expected: %zu, "
             "actual : %u)",
             le16toh(data->connection_handle), iter->second.count, comp_packets);

      iter->second.count = 0u;

      // On debug builds it's better to assert and crash so that we can catch
      // controller bugs. On release builds we log the warning message above and
      // continue.
      ZX_PANIC("controller reported incorrect packet count!");
    } else {
      iter->second.count -= comp_packets;
    }

    if (iter->second.ll_type == Connection::LinkType::kACL) {
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
  TrySendNextQueuedPackets();
  return CommandChannel::EventCallbackResult::kContinue;
}

void AclDataChannelImpl::TrySendNextQueuedPackets() {
  if (!is_initialized_)
    return;

  size_t avail_bredr_packets = GetNumFreeBREDRPackets();
  size_t avail_le_packets = GetNumFreeLEPackets();

  // Based on what we know about controller buffer availability, we process
  // packets that are currently in |send_queue_|. The packets that can be sent
  // are added to |to_send|. Packets that cannot be sent remain in
  // |send_queue_|.
  DataPacketQueue to_send;
  for (auto iter = send_queue_.begin(); iter != send_queue_.end();) {
    if (!avail_bredr_packets && !avail_le_packets)
      break;

    if (send_queue_.front().ll_type == Connection::LinkType::kACL && avail_bredr_packets) {
      --avail_bredr_packets;
    } else if (send_queue_.front().ll_type == Connection::LinkType::kLE && avail_le_packets) {
      --avail_le_packets;
    } else {
      // Cannot send packet yet, so skip it.
      ++iter;
      continue;
    }

    to_send.push_back(std::move(*iter));
    send_queue_.erase(iter++);
  }

  if (to_send.empty())
    return;

  size_t bredr_packets_sent = 0;
  size_t le_packets_sent = 0;
  while (!to_send.empty()) {
    const QueuedDataPacket& packet = to_send.front();

    auto packet_bytes = packet.packet->view().data();
    zx_status_t status = channel_.write(0, packet_bytes.data(), packet_bytes.size(), nullptr, 0);
    if (status < 0) {
      bt_log(ERROR, "hci", "failed to send data packet to HCI driver (%s) - dropping packet",
             zx_status_get_string(status));
      to_send.pop_front();
      continue;
    }

    if (packet.ll_type == Connection::LinkType::kACL) {
      ++bredr_packets_sent;
    } else {
      ++le_packets_sent;
    }

    auto iter = pending_links_.find(packet.packet->connection_handle());
    if (iter == pending_links_.end()) {
      pending_links_[packet.packet->connection_handle()] = PendingPacketData(packet.ll_type);
    } else {
      iter->second.count++;
    }

    to_send.pop_front();
  }

  IncrementTotalNumPackets(bredr_packets_sent);
  IncrementLETotalNumPackets(le_packets_sent);
}

size_t AclDataChannelImpl::GetNumFreeBREDRPackets() const {
  ZX_DEBUG_ASSERT(bredr_buffer_info_.max_num_packets() >= num_sent_packets_);
  return bredr_buffer_info_.max_num_packets() - num_sent_packets_;
}

size_t AclDataChannelImpl::GetNumFreeLEPackets() const {
  if (!le_buffer_info_.IsAvailable())
    return GetNumFreeBREDRPackets();

  ZX_DEBUG_ASSERT(le_buffer_info_.max_num_packets() >= le_num_sent_packets_);
  return le_buffer_info_.max_num_packets() - le_num_sent_packets_;
}

void AclDataChannelImpl::DecrementTotalNumPackets(size_t count) {
  ZX_DEBUG_ASSERT(num_sent_packets_ >= count);
  num_sent_packets_ -= count;
}

void AclDataChannelImpl::DecrementLETotalNumPackets(size_t count) {
  if (!le_buffer_info_.IsAvailable()) {
    DecrementTotalNumPackets(count);
    return;
  }

  ZX_DEBUG_ASSERT(le_num_sent_packets_ >= count);
  le_num_sent_packets_ -= count;
}

void AclDataChannelImpl::IncrementTotalNumPackets(size_t count) {
  ZX_DEBUG_ASSERT(num_sent_packets_ + count <= bredr_buffer_info_.max_num_packets());
  num_sent_packets_ += count;
}

void AclDataChannelImpl::IncrementLETotalNumPackets(size_t count) {
  if (!le_buffer_info_.IsAvailable()) {
    IncrementTotalNumPackets(count);
    return;
  }

  ZX_DEBUG_ASSERT(le_num_sent_packets_ + count <= le_buffer_info_.max_num_packets());
  le_num_sent_packets_ += count;
}

void AclDataChannelImpl::OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait,
                                        zx_status_t status, const zx_packet_signal_t* signal) {
  TRACE_DURATION("bluetooth", "AclDataChannelImpl::OnChannelReady");
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "channel error: %s", zx_status_get_string(status));
    return;
  }

  if (!is_initialized_) {
    return;
  }

  ZX_DEBUG_ASSERT(async_get_default_dispatcher() == io_dispatcher_);
  ZX_DEBUG_ASSERT(signal->observed & ZX_CHANNEL_READABLE);

  for (size_t count = 0; count < signal->count; count++) {
    TRACE_DURATION("bluetooth", "AclDataChannelImpl::OnChannelReady read packet");
    if (!rx_callback_) {
      continue;
    }
    // Allocate a buffer for the event. Since we don't know the size beforehand
    // we allocate the largest possible buffer.
    auto packet = ACLDataPacket::New(slab_allocators::kLargeACLDataPayloadSize);
    if (!packet) {
      bt_log(ERROR, "hci", "failed to allocate buffer received ACL data packet!");
      return;
    }
    zx_status_t status = ReadAclDataPacketFromChannel(channel_, packet);
    if (status == ZX_ERR_INVALID_ARGS) {
      continue;
    } else if (status != ZX_OK) {
      return;
    }
    {
      TRACE_DURATION("bluetooth", "AclDataChannelImpl->rx_callback_");
      rx_callback_(std::move(packet));
    }
  }

  status = wait->Begin(dispatcher);
  if (status != ZX_OK) {
    bt_log(ERROR, "hci", "wait error: %s", zx_status_get_string(status));
  }
}

AclDataChannelImpl::DataPacketQueue::iterator
AclDataChannelImpl::SendQueueInsertLocationForPriority(PacketPriority priority) {
  // insert low priority packets at the end of the queue
  if (priority == PacketPriority::kLow) {
    return send_queue_.end();
  }

  // insert high priority packets before first low priority packet
  return std::find_if(send_queue_.begin(), send_queue_.end(), [&](const QueuedDataPacket& packet) {
    return packet.priority == PacketPriority::kLow;
  });
}

CommandChannel::EventCallbackResult AclDataChannelImpl::DataBufferOverflowCallback(
    const EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci::kDataBufferOverflowEventCode);

  const auto& params = event.params<hci::ConnectionRequestEventParams>();

  // Internal buffer state must be invalid and no further transmissions are possible.
  ZX_PANIC("controller data buffer overflow event received (link type: %hhu)", params.link_type);

  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
