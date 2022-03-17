// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "acl_data_channel.h"

#include <endian.h>
#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/sys/inspect/cpp/component.h>
#include <zircon/assert.h>
#include <zircon/status.h>

#include <iterator>
#include <numeric>

#include "lib/fit/function.h"
#include "lib/inspect/cpp/vmo/types.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/pipeline_monitor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/retire_log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/windowed_inspect_numeric_property.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "transport.h"

namespace bt::hci {

zx_status_t AclDataChannel::ReadAclDataPacketFromChannel(const zx::channel& channel,
                                                         const ACLDataPacketPtr& packet) {
  uint32_t read_size;
  auto packet_bytes = packet->mutable_view()->mutable_data();
  zx_status_t read_status =
      channel.read(0u, packet_bytes.mutable_data(), /*handles=*/nullptr, packet_bytes.size(), 0,
                   &read_size, /*actual_handles=*/nullptr);
  if (read_status < 0) {
    bt_log(DEBUG, "hci", "failed to read RX bytes: %s", zx_status_get_string(read_status));
    // Clear the handler so that we stop receiving events from it.
    // TODO(jamuraa): signal failure to the consumer so it can do something.
    return ZX_ERR_IO;
  }

  if (read_size < sizeof(hci_spec::ACLDataHeader)) {
    bt_log(ERROR, "hci", "malformed data packet - expected at least %zu bytes, got %u",
           sizeof(hci_spec::ACLDataHeader), read_size);
    // TODO(jamuraa): signal stream error somehow
    return ZX_ERR_INVALID_ARGS;
  }

  const size_t rx_payload_size = read_size - sizeof(hci_spec::ACLDataHeader);
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
  void AttachInspect(inspect::Node& parent, std::string name) override;
  void ShutDown() override;
  void SetDataRxHandler(ACLPacketHandler rx_callback) override;
  bool SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                  PacketPriority priority) override;
  bool SendPackets(LinkedList<ACLDataPacket> packets, UniqueChannelId channel_id,
                   PacketPriority priority) override;
  void RegisterLink(hci_spec::ConnectionHandle handle, bt::LinkType ll_type) override;
  void UnregisterLink(hci_spec::ConnectionHandle handle) override;
  void DropQueuedPackets(AclPacketPredicate predicate) override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override;
  const DataBufferInfo& GetBufferInfo() const override;
  const DataBufferInfo& GetLeBufferInfo() const override;
  void RequestAclPriority(hci::AclPriority priority, hci_spec::ConnectionHandle handle,
                          fit::callback<void(fitx::result<fitx::failed>)> callback) override;
  void SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout, hci_spec::ConnectionHandle handle,
                                     ResultCallback<> callback) override;

 private:
  // Represents a queued ACL data packet.
  struct QueuedDataPacket {
    QueuedDataPacket(bt::LinkType ll_type, UniqueChannelId channel_id, PacketPriority priority,
                     ACLDataPacketPtr packet, PipelineMonitor::Token token)
        : ll_type(ll_type),
          channel_id(channel_id),
          priority(priority),
          packet(std::move(packet)),
          token(std::move(token)) {}

    QueuedDataPacket() = delete;
    QueuedDataPacket(QueuedDataPacket&& other) = default;
    QueuedDataPacket& operator=(QueuedDataPacket&& other) = default;

    bt::LinkType ll_type;
    UniqueChannelId channel_id;
    PacketPriority priority;
    ACLDataPacketPtr packet;
    PipelineMonitor::Token token;
  };

  using DataPacketQueue = std::list<QueuedDataPacket>;

  // Take packets from |send_queue| up to and equal to the available capacities given in
  // |avail_bredr_packets| and |avail_le_packets| respectively
  static DataPacketQueue TakePacketsToSend(DataPacketQueue& send_queue, size_t avail_bredr_packets,
                                           size_t avail_le_packets);

  // Returns the data buffer MTU for the given connection.
  size_t GetBufferMtu(bt::LinkType ll_type) const;

  // Handler for the HCI Number of Completed Packets Event, used for
  // packet-based data flow control.
  CommandChannel::EventCallbackResult NumberOfCompletedPacketsCallback(const EventPacket& event);

  // Searches send queue for packets with the same link and channel as |archetypal_packet| and
  // removes the least recently-inserted full PDU if necessary. Called before inserting
  // |archetypal_packet| to ensure that there are only up to |kMaxAclPacketsPerChannel| PDUs queued
  // for the given connection and handle.
  void DropOverflowPacket(const QueuedDataPacket& archetypal_packet);

  // Drains |dropped_packet_counts_| and logs any recorded drops. Should only be called by
  // |log_dropped_overflow_task_|.
  void LogDroppedOverflowPackets();

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

  // Compute and write quantiles of send latency metrics to Inspect properties. Should only be
  // called by |write_send_metrics_task_|.
  void WriteSendMetrics();

  // Handler for HCI_Buffer_Overflow_event
  CommandChannel::EventCallbackResult DataBufferOverflowCallback(const EventPacket& event);

  // Links this node to the inspect tree. Initialized as needed by AttachInspect.
  inspect::Node node_;

  // Contents of |node_|. Retained as members so that they last as long as a class instance.
  inspect::Node le_subnode_;
  inspect::BoolProperty le_subnode_shared_with_bredr_property_;
  inspect::Node bredr_subnode_;
  inspect::Node metrics_subnode_;
  inspect::Node send_latency_subnode_;
  struct {
    const double quantile;
    const char* const name;
    IntInspectable<zx::duration> property{std::mem_fn(&zx::duration::to_usecs)};
  } send_latency_properties_[3] = {
      {0.5, "50th_percentile_us"}, {0.95, "95th_percentile_us"}, {0.99, "99th_percentile_us"}};
  inspect::Node send_size_subnode_;
  struct {
    const double quantile;
    const char* const name;
    UintInspectable<size_t> property{};
  } send_size_properties_[3] = {{0.1, "10th_percentile_bytes"},
                                {0.5, "50th_percentile_bytes"},
                                {0.9, "90th_percentile_bytes"}};

  async::TaskClosureMethod<AclDataChannelImpl, &AclDataChannelImpl::WriteSendMetrics>
      write_send_metrics_task_{this};

  // Used to assert that certain public functions are only called on the creation thread.
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
  UintInspectable<size_t> num_sent_packets_;
  UintInspectable<size_t> le_num_sent_packets_;

  // Tracks statistics related to the lifetime of queued outbound data. Data is exported to the
  // Inspect hierarchy using WriteSendMetrics.
  //
  // TODO(fxbug.dev/71342): When PipelineMonitor tokens can better represent chunked data with life
  // stages, hoist PipelineMonitor into a more visible state holder (Adapter?) so tokens are issued
  // when we pull data from the profile channel socket.
  PipelineMonitor send_monitor_;

  // Counts of automatically-discarded packets on each channel due to overflow. Cleared by
  // LogDroppedOverflowPackets.
  std::map<std::pair<hci_spec::ConnectionHandle, UniqueChannelId>, int64_t> dropped_packet_counts_;

  // Lifetime and recent counts of overflow packets that have been dropped.
  UintInspectable<size_t> num_overflow_packets_;
  WindowedInspectUintProperty num_recent_overflow_packets_{/*expiry_duration=*/zx::min(3),
                                                           /*min_resolution=*/zx::min(3) / 100};

  async::TaskClosureMethod<AclDataChannelImpl, &AclDataChannelImpl::LogDroppedOverflowPackets>
      log_dropped_overflow_task_{this};

  // The ACL data packet queue contains the data packets that are waiting to be
  // sent to the controller.
  // TODO(armansito): Use priority_queue based on L2CAP channel priority.
  // TODO(fxbug.dev/944): Keep a separate queue for each open connection. Benefits:
  //   * Helps address the packet-prioritization TODO above.
  //   * Also: having separate queues, which know their own
  //     bt::LinkType, would let us replace std::list<QueuedDataPacket>
  //     with LinkedList<ACLDataPacket> which has a more efficient
  //     memory layout.
  using InspectableDataPacketQueue = Inspectable<DataPacketQueue, inspect::UintProperty, size_t>;
  InspectableDataPacketQueue send_queue_{std::mem_fn(&DataPacketQueue::size)};

  // Returns an iterator to the location new packets should be inserted into |send_queue_| based on
  // their |priority|:
  // If |priority| is |kLow|: returns past-the-end of |send_queue_|.
  // If |priority| is |kHigh|: returns the location of the first |kLow| priority packet.
  DataPacketQueue::const_iterator SendQueueInsertLocationForPriority(PacketPriority priority);

  // Stores the link type of connections on which we have a pending packet that
  // has been sent to the controller. Entries are removed on the HCI Number Of
  // Completed Packets event.
  struct PendingPacketData {
    PendingPacketData() = default;

    // We initialize the packet count at 1 since a new entry will only be
    // created once.
    explicit PendingPacketData(bt::LinkType ll_type) : ll_type(ll_type), count(1u) {}

    bt::LinkType ll_type;
    size_t count;
  };
  std::unordered_map<hci_spec::ConnectionHandle, PendingPacketData> pending_links_;

  // Stores links registered by RegisterLink
  std::unordered_map<hci_spec::ConnectionHandle, bt::LinkType> registered_links_;

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
      io_dispatcher_(async_get_default_dispatcher()),
      send_monitor_(fit::nullable(io_dispatcher_),
                    // Buffer depth for ~3 minutes of audio assuming ~50 ACL fragments/s send rate
                    internal::RetireLog(/*min_depth=*/100, /*max_depth=*/1 << 13)) {
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

  zx_status_t wait_status = channel_wait_.Begin(async_get_default_dispatcher());
  if (wait_status != ZX_OK) {
    bt_log(ERROR, "hci", "failed channel setup %s", zx_status_get_string(wait_status));
    channel_wait_.set_object(ZX_HANDLE_INVALID);
    // TODO(jamuraa): return whether we successfully initialized?
    return;
  }
  bt_log(DEBUG, "hci", "started I/O handler");

  num_completed_packets_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      hci_spec::kNumberOfCompletedPacketsEventCode,
      fit::bind_member<&AclDataChannelImpl::NumberOfCompletedPacketsCallback>(this));
  ZX_DEBUG_ASSERT(num_completed_packets_event_handler_id_);

  data_buffer_overflow_event_handler_id_ = transport_->command_channel()->AddEventHandler(
      hci_spec::kDataBufferOverflowEventCode,
      fit::bind_member<&AclDataChannelImpl::DataBufferOverflowCallback>(this));
  ZX_DEBUG_ASSERT(data_buffer_overflow_event_handler_id_);

  is_initialized_ = true;

  bt_log(INFO, "hci", "initialized");
}

void AclDataChannelImpl::AttachInspect(inspect::Node& parent, std::string name) {
  ZX_ASSERT_MSG(is_initialized_, "Must be initialized before attaching to inspect tree");
  node_ = parent.CreateChild(std::move(name));
  send_queue_.SetProperty(node_.CreateUint("num_queued_packets", 0));
  num_overflow_packets_.AttachInspect(node_, "num_overflow_packets");
  num_recent_overflow_packets_.AttachInspect(node_, "num_recent_overflow_packets");

  bredr_subnode_ = node_.CreateChild("bredr");
  num_sent_packets_.AttachInspect(bredr_subnode_, "num_sent_packets");

  le_subnode_ = node_.CreateChild("le");
  le_num_sent_packets_.AttachInspect(le_subnode_, "num_sent_packets");
  le_subnode_shared_with_bredr_property_ =
      le_subnode_.CreateBool("independent_from_bredr", le_buffer_info_.IsAvailable());

  metrics_subnode_ = node_.CreateChild("metrics");
  send_latency_subnode_ = metrics_subnode_.CreateChild("send_latency");
  for (auto& [_, name, property] : send_latency_properties_) {
    property.AttachInspect(send_latency_subnode_, name);
  }
  send_size_subnode_ = metrics_subnode_.CreateChild("send_size");
  for (auto& [_, name, property] : send_size_properties_) {
    property.AttachInspect(send_size_subnode_, name);
  }
}

void AclDataChannelImpl::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
  if (!is_initialized_)
    return;

  bt_log(INFO, "hci", "shutting down");

  write_send_metrics_task_.Cancel();
  log_dropped_overflow_task_.Cancel();

  bt_log(DEBUG, "hci", "removing I/O handler");
  zx_status_t cancel_status = channel_wait_.Cancel();
  if (cancel_status != ZX_OK) {
    bt_log(WARN, "hci", "couldn't cancel wait on channel: %s", zx_status_get_string(cancel_status));
  }

  transport_->command_channel()->RemoveEventHandler(num_completed_packets_event_handler_id_);
  transport_->command_channel()->RemoveEventHandler(data_buffer_overflow_event_handler_id_);

  is_initialized_ = false;
  send_queue_.Mutable()->clear();
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
  ZX_ASSERT(data_packet);
  LinkedList<ACLDataPacket> packets;
  packets.push_back(std::move(data_packet));
  return SendPackets(std::move(packets), channel_id, priority);
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

  // This call assumes that each call includes a full PDU, which means that there can't be a
  // continuing fragment at the head. There is no check for whether |packets| have enough data to
  // form whole PDUs because queue management doesn't require that and it would break abstraction
  // even more.
  ZX_ASSERT_MSG(packets.front().packet_boundary_flag() !=
                    hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                "expected full PDU");

  for (const auto& packet : packets) {
    // This call assumes that all packets in each call are for the same connection
    ZX_ASSERT_MSG(packet.connection_handle() == handle,
                  "expected only fragments for one connection (%#.4x, got %#.4x)", handle,
                  packet.connection_handle());

    // Make sure that all packets are within the MTU.
    if (packet.view().payload_size() >
        GetBufferMtu(registered_links_[packet.connection_handle()])) {
      bt_log(ERROR, "hci", "ACL data packet too large!");
      return false;
    }
  }

  auto insert_iter = SendQueueInsertLocationForPriority(priority);
  for (int i = 0; !packets.is_empty(); i++) {
    auto packet = packets.pop_front();
    auto ll_type = registered_links_[packet->connection_handle()];
    const size_t payload_size = packet->view().payload_size();
    auto queue_packet = QueuedDataPacket(ll_type, channel_id, priority, std::move(packet),
                                         send_monitor_.Issue(payload_size));
    if (i == 0) {
      if (queue_packet.priority == PacketPriority::kLow && ll_type == bt::LinkType::kACL) {
        DropOverflowPacket(queue_packet);
      }
    }
    send_queue_.Mutable()->insert(insert_iter, std::move(queue_packet));
  }

  TrySendNextQueuedPackets();

  return true;
}

void AclDataChannelImpl::RegisterLink(hci_spec::ConnectionHandle handle, bt::LinkType ll_type) {
  bt_log(DEBUG, "hci", "ACL register link (handle: %#.4x)", handle);
  ZX_DEBUG_ASSERT(registered_links_.find(handle) == registered_links_.end());
  registered_links_[handle] = ll_type;
}

void AclDataChannelImpl::UnregisterLink(hci_spec::ConnectionHandle handle) {
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
  const size_t before_count = send_queue_->size();
  send_queue_.Mutable()->remove_if([&predicate](const QueuedDataPacket& packet) {
    return predicate(packet.packet, packet.channel_id);
  });
  const size_t removed_count = before_count - send_queue_->size();
  if (removed_count > 0) {
    bt_log(TRACE, "hci", "packets dropped from send queue (count: %lu)", removed_count);
  }
}

void AclDataChannelImpl::ClearControllerPacketCount(hci_spec::ConnectionHandle handle) {
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
  if (data.ll_type == bt::LinkType::kLE) {
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

void AclDataChannelImpl::RequestAclPriority(
    hci::AclPriority priority, hci_spec::ConnectionHandle handle,
    fit::callback<void(fitx::result<fitx::failed>)> callback) {
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
    callback(fitx::failed());
    return;
  }
  auto encoded = encode_result.take_value();
  if (encoded.size() < sizeof(hci_spec::CommandHeader)) {
    bt_log(TRACE, "hci", "encoded ACL priority command too small (size: %zu)", encoded.size());
    callback(fitx::failed());
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
          cb(fitx::failed());
          return;
        }

        bt_log(DEBUG, "hci", "acl priority updated (priority: %#.8x)",
               static_cast<uint32_t>(priority));
        cb(fitx::ok());
      });
}

void AclDataChannelImpl::SetBrEdrAutomaticFlushTimeout(zx::duration flush_timeout,
                                                       hci_spec::ConnectionHandle handle,
                                                       ResultCallback<> callback) {
  auto link_iter = registered_links_.find(handle);
  ZX_ASSERT(link_iter != registered_links_.end());
  ZX_ASSERT(link_iter->second == bt::LinkType::kACL);

  if (flush_timeout < zx::msec(1) || (flush_timeout > hci_spec::kMaxAutomaticFlushTimeoutDuration &&
                                      flush_timeout != zx::duration::infinite())) {
    callback(ToResult(hci_spec::StatusCode::kInvalidHCICommandParameters));
    return;
  }

  uint16_t converted_flush_timeout;
  if (flush_timeout == zx::duration::infinite()) {
    // The command treats a flush timeout of 0 as infinite.
    converted_flush_timeout = 0;
  } else {
    // Slight imprecision from casting or converting to ms is fine for the flush timeout (a few
    // ms difference from the requested value doesn't matter). Overflow is not possible because of
    // the max value check above.
    converted_flush_timeout =
        static_cast<uint16_t>(static_cast<float>(flush_timeout.to_msecs()) *
                              hci_spec::kFlushTimeoutMsToCommandParameterConversionFactor);
    ZX_ASSERT(converted_flush_timeout != 0);
    ZX_ASSERT(converted_flush_timeout <= hci_spec::kMaxAutomaticFlushTimeoutCommandParameterValue);
  }

  auto packet = CommandPacket::New(hci_spec::kWriteAutomaticFlushTimeout,
                                   sizeof(hci_spec::WriteAutomaticFlushTimeoutCommandParams));
  auto packet_view = packet->mutable_payload<hci_spec::WriteAutomaticFlushTimeoutCommandParams>();
  packet_view->connection_handle = htole16(handle);
  packet_view->flush_timeout = htole16(converted_flush_timeout);

  transport_->command_channel()->SendCommand(
      std::move(packet),
      [cb = std::move(callback), handle, flush_timeout](auto, const EventPacket& event) mutable {
        if (hci_is_error(event, WARN, "hci", "WriteAutomaticFlushTimeout command failed")) {
        } else {
          bt_log(DEBUG, "hci", "automatic flush timeout updated (handle: %#.4x, timeout: %ld ms)",
                 handle, flush_timeout.to_msecs());
        }
        cb(event.ToResult());
      });
}

size_t AclDataChannelImpl::GetBufferMtu(bt::LinkType ll_type) const {
  if (ll_type == bt::LinkType::kACL)
    return bredr_buffer_info_.max_data_length();
  return GetLeBufferInfo().max_data_length();
}

CommandChannel::EventCallbackResult AclDataChannelImpl::NumberOfCompletedPacketsCallback(
    const EventPacket& event) {
  if (!is_initialized_) {
    return CommandChannel::EventCallbackResult::kContinue;
  }

  ZX_DEBUG_ASSERT(async_get_default_dispatcher() == io_dispatcher_);
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kNumberOfCompletedPacketsEventCode);

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

    ZX_DEBUG_ASSERT(iter->second.count);
    if (iter->second.count < comp_packets) {
      bt_log(WARN, "hci", "packet tx count mismatch! (handle: %#.4x, expected: %zu, actual : %u)",
             le16toh(data->connection_handle), iter->second.count, comp_packets);

      iter->second.count = 0u;

      // On debug builds it's better to assert and crash so that we can catch
      // controller bugs. On release builds we log the warning message above and
      // continue.
      ZX_PANIC("controller reported incorrect packet count!");
    } else {
      iter->second.count -= comp_packets;
    }

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
  TrySendNextQueuedPackets();
  return CommandChannel::EventCallbackResult::kContinue;
}

void AclDataChannelImpl::DropOverflowPacket(const QueuedDataPacket& archetypal_packet) {
  TRACE_DURATION("bluetooth", "ACLDataChannel::DropOverflowPacket", "send_queue_->size",
                 send_queue_->size());

  // TODO(fxbug.dev/71061): Performance of these O(N) searches is not amazing, taking tens of µs
  // on low-end ARM when kMaxAclPacketsPerChannel=64. The std::list nodes do seem to have decent
  // cache locality because that time increases sublinearly up to at least 64, and overall
  // performance is acceptable, with the above TRACE_DURATION taking <15% of the total
  // l2cap::channel_send flow duration. Performance optimization should be a future goal of work
  // that reorganizes channel and link data flow layouts.

  // predicate that checks if a QDP is the head of a PDU (not a continuing fragment)
  auto is_head = [](const QueuedDataPacket& p) {
    return p.packet->packet_boundary_flag() == hci_spec::ACLPacketBoundaryFlag::kFirstFlushable ||
           p.packet->packet_boundary_flag() == hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable;
  };
  // predicate that checks if a QDP is like |archetypal_packet| and is the head of a PDU
  auto is_similar_and_head = [&a = archetypal_packet, is_head](const QueuedDataPacket& b) {
    return a.packet->connection_handle() == b.packet->connection_handle() &&
           a.channel_id == b.channel_id && is_head(b);
  };
  const size_t queued_similar_pdu_count =
      std::count_if(send_queue_->begin(), send_queue_->end(), is_similar_and_head);
  if (queued_similar_pdu_count < kMaxAclPacketsPerChannel) {
    return;
  }

  const auto to_drop_iter =
      std::find_if(send_queue_->begin(), send_queue_->end(), is_similar_and_head);
  ZX_ASSERT(to_drop_iter != send_queue_->end());
  const auto next_packet_iter = std::find_if(std::next(to_drop_iter), send_queue_->end(), is_head);

  const size_t num_dropped = std::distance(to_drop_iter, next_packet_iter);
  dropped_packet_counts_[{archetypal_packet.packet->connection_handle(),
                          archetypal_packet.channel_id}] += num_dropped;
  *num_overflow_packets_.Mutable() += num_dropped;
  num_recent_overflow_packets_.Add(num_dropped);
  send_queue_.Mutable()->erase(to_drop_iter, next_packet_iter);

  // Schedule a deadline to log this drop and any other drops that occur until the logging drains
  // the counters.
  if (!log_dropped_overflow_task_.is_pending()) {
    constexpr zx::duration kMinLogInterval = zx::sec(1);
    log_dropped_overflow_task_.PostDelayed(io_dispatcher_, kMinLogInterval);
  }
}

void AclDataChannelImpl::LogDroppedOverflowPackets() {
  // This exchange clears the accumulated counts since the previous call (which should be at least
  // kMinLogInterval ago) and gets the number of dropped packets for each channel (if any).
  const auto dropped_packet_counts = std::exchange(dropped_packet_counts_, {});

  // This logs at most once per channel, at a temporal period of kMinLogInterval, and only for the
  // channels where overflow occurred.
  for (auto& [ids, count] : dropped_packet_counts) {
    bt_log(WARN, "hci", "%#.4x:%#.4x dropped %zu fragments(s)", ids.first, ids.second, count);
  }
}

AclDataChannelImpl::DataPacketQueue AclDataChannelImpl::TakePacketsToSend(
    DataPacketQueue& send_queue, size_t avail_bredr_packets, size_t avail_le_packets) {
  // Based on what we know about controller buffer availability, we process
  // packets that are currently in |send_queue|. The packets that can be sent
  // are added to |to_send|. Packets that cannot be sent remain in
  // |send_queue|.
  DataPacketQueue to_send;
  for (auto iter = send_queue.begin(); iter != send_queue.end();) {
    if (!avail_bredr_packets && !avail_le_packets)
      break;

    if (iter->ll_type == bt::LinkType::kACL && avail_bredr_packets) {
      --avail_bredr_packets;
    } else if (iter->ll_type == bt::LinkType::kLE && avail_le_packets) {
      --avail_le_packets;
    } else {
      // Cannot send packet yet, so skip it.
      ++iter;
      continue;
    }

    to_send.push_back(std::move(*iter));
    iter = send_queue.erase(iter);
  }

  return to_send;
}

void AclDataChannelImpl::TrySendNextQueuedPackets() {
  if (!is_initialized_)
    return;

  // TODO(fxbug.dev/72582) - This logic is incorrect for a controller which uses a shared BrEdr &
  // LE buffer, as we will report the capacity twice, and possibly attempt to send up to double the
  // number of available packets, resulting in some being dropped.
  size_t avail_bredr_packets = GetNumFreeBREDRPackets();
  size_t avail_le_packets = GetNumFreeLEPackets();

  auto to_send = TakePacketsToSend(*send_queue_.Mutable(), avail_bredr_packets, avail_le_packets);

  if (to_send.empty())
    return;

  size_t bredr_packets_sent = 0;
  size_t le_packets_sent = 0;
  while (!to_send.empty()) {
    const QueuedDataPacket& packet = to_send.front();

    auto packet_bytes = packet.packet->view().data();
    zx_status_t status =
        channel_.write(0, packet_bytes.data(), packet_bytes.size(), /*handles=*/nullptr, 0);
    if (status < 0) {
      bt_log(ERROR, "hci", "failed to send data packet to HCI driver (%s) - dropping packet",
             zx_status_get_string(status));
      to_send.pop_front();
      continue;
    }

    if (packet.ll_type == bt::LinkType::kACL) {
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

  // Schedule a deadline to re-compute the send statistics and write them to Inspect. The scheduling
  // is performed lazily so that no expensive computation occurs if no data is being sent
  if (!write_send_metrics_task_.is_pending()) {
    // This backs off the statistic computation in order to limit the rate at which it runs
    constexpr zx::duration kMinStatisticsInterval = zx::sec(5);
    write_send_metrics_task_.PostDelayed(io_dispatcher_, kMinStatisticsInterval);
  }
}

size_t AclDataChannelImpl::GetNumFreeBREDRPackets() const {
  ZX_DEBUG_ASSERT(bredr_buffer_info_.max_num_packets() >= *num_sent_packets_);
  return bredr_buffer_info_.max_num_packets() - *num_sent_packets_;
}

size_t AclDataChannelImpl::GetNumFreeLEPackets() const {
  if (!le_buffer_info_.IsAvailable())
    return GetNumFreeBREDRPackets();

  ZX_DEBUG_ASSERT(le_buffer_info_.max_num_packets() >= *le_num_sent_packets_);
  return le_buffer_info_.max_num_packets() - *le_num_sent_packets_;
}

void AclDataChannelImpl::DecrementTotalNumPackets(size_t count) {
  ZX_DEBUG_ASSERT(*num_sent_packets_ >= count);
  *num_sent_packets_.Mutable() -= count;
}

void AclDataChannelImpl::DecrementLETotalNumPackets(size_t count) {
  if (!le_buffer_info_.IsAvailable()) {
    DecrementTotalNumPackets(count);
    return;
  }

  ZX_DEBUG_ASSERT(*le_num_sent_packets_ >= count);
  *le_num_sent_packets_.Mutable() -= count;
}

void AclDataChannelImpl::IncrementTotalNumPackets(size_t count) {
  ZX_DEBUG_ASSERT(*num_sent_packets_ + count <= bredr_buffer_info_.max_num_packets());
  *num_sent_packets_.Mutable() += count;
}

void AclDataChannelImpl::IncrementLETotalNumPackets(size_t count) {
  if (!le_buffer_info_.IsAvailable()) {
    IncrementTotalNumPackets(count);
    return;
  }

  ZX_DEBUG_ASSERT(*le_num_sent_packets_ + count <= le_buffer_info_.max_num_packets());
  *le_num_sent_packets_.Mutable() += count;
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

void AclDataChannelImpl::WriteSendMetrics() {
  if (!node_) {
    return;
  }

  auto compute_metrics_for_properties = [](auto properties, auto size_fn, auto compute_quantiles) {
    std::array<double, size_fn()> quantiles;
    for (size_t i = 0; i < quantiles.size(); i++) {
      quantiles[i] = properties[i].quantile;
    }
    auto samples = compute_quantiles(quantiles);
    if (samples.has_value()) {
      for (size_t i = 0; i < samples->size(); i++) {
        properties[i].property.Set(samples.value()[i]);
      }
    }
  };
  compute_metrics_for_properties(
      send_latency_properties_, []() { return std::extent_v<decltype(send_latency_properties_)>; },
      [this](auto quantiles) { return send_monitor_.retire_log().ComputeAgeQuantiles(quantiles); });
  compute_metrics_for_properties(
      send_size_properties_, []() { return std::extent_v<decltype(send_size_properties_)>; },
      [this](auto quantiles) {
        return send_monitor_.retire_log().ComputeByteCountQuantiles(quantiles);
      });
}

AclDataChannelImpl::DataPacketQueue::const_iterator
AclDataChannelImpl::SendQueueInsertLocationForPriority(PacketPriority priority) {
  // insert low priority packets at the end of the queue
  if (priority == PacketPriority::kLow) {
    return send_queue_->end();
  }

  // insert high priority packets before first low priority packet
  return std::find_if(
      send_queue_->begin(), send_queue_->end(),
      [&](const QueuedDataPacket& packet) { return packet.priority == PacketPriority::kLow; });
}

CommandChannel::EventCallbackResult AclDataChannelImpl::DataBufferOverflowCallback(
    const EventPacket& event) {
  ZX_DEBUG_ASSERT(event.event_code() == hci_spec::kDataBufferOverflowEventCode);

  const auto& params = event.params<hci_spec::ConnectionRequestEventParams>();

  // Internal buffer state must be invalid and no further transmissions are possible.
  ZX_PANIC("controller data buffer overflow event received (link type: %hhu)", params.link_type);

  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
