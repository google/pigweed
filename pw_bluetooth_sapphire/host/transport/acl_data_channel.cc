// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "acl_data_channel.h"

#include <endian.h>
#include <lib/async/cpp/time.h>
#include <lib/async/default.h>
#include <lib/fit/defer.h>
#include <zircon/status.h>

#include <iterator>
#include <numeric>

#include "lib/fit/function.h"
#include "slab_allocators.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/inspectable.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/pipeline_monitor.h"
#include "src/connectivity/bluetooth/core/bt-host/common/retire_log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/windowed_inspect_numeric_property.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "transport.h"

namespace bt::hci {

class AclDataChannelImpl final : public AclDataChannel {
 public:
  AclDataChannelImpl(Transport* transport, HciWrapper* hci, const DataBufferInfo& bredr_buffer_info,
                     const DataBufferInfo& le_buffer_info);
  ~AclDataChannelImpl() override;

  // AclDataChannel overrides
  void AttachInspect(inspect::Node& parent, const std::string& name) override;
  void SetDataRxHandler(ACLPacketHandler rx_callback) override;
  bool SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                  PacketPriority priority) override;
  bool SendPackets(std::list<ACLDataPacketPtr> packets, UniqueChannelId channel_id,
                   PacketPriority priority) override;
  void RegisterLink(hci_spec::ConnectionHandle handle, bt::LinkType ll_type) override;
  void UnregisterLink(hci_spec::ConnectionHandle handle) override;
  void DropQueuedPackets(AclPacketPredicate predicate) override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override;
  const DataBufferInfo& GetBufferInfo() const override;
  const DataBufferInfo& GetLeBufferInfo() const override;
  void RequestAclPriority(hci::AclPriority priority, hci_spec::ConnectionHandle handle,
                          fit::callback<void(fitx::result<fitx::failed>)> callback) override;

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

  // Take packets from |send_queue_| up to and equal to the free slots in the controller buffer(s).
  DataPacketQueue TakePacketsToSend();

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
  // to buffer. This will be 0 if the BR/EDR buffer is shared with LE.
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

  void OnRxPacket(std::unique_ptr<ACLDataPacket> packet);

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

  // The Transport object that owns this instance.
  Transport* transport_;  // weak;

  // HciWrapper is owned by Transport and will outlive this object.
  HciWrapper* hci_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_ = 0;

  // The event handler ID for the Data Buffer Overflow event.
  CommandChannel::EventHandlerId data_buffer_overflow_event_handler_id_ = 0;

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

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AclDataChannelImpl);
};

std::unique_ptr<AclDataChannel> AclDataChannel::Create(Transport* transport, HciWrapper* hci,
                                                       const DataBufferInfo& bredr_buffer_info,
                                                       const DataBufferInfo& le_buffer_info) {
  return std::make_unique<AclDataChannelImpl>(transport, hci, bredr_buffer_info, le_buffer_info);
}

AclDataChannelImpl::AclDataChannelImpl(Transport* transport, HciWrapper* hci,
                                       const DataBufferInfo& bredr_buffer_info,
                                       const DataBufferInfo& le_buffer_info)
    : transport_(transport),
      hci_(hci),
      io_dispatcher_(async_get_default_dispatcher()),
      bredr_buffer_info_(bredr_buffer_info),
      le_buffer_info_(le_buffer_info),
      send_monitor_(fit::nullable(io_dispatcher_),
                    // Buffer depth for ~3 minutes of audio assuming ~50 ACL fragments/s send rate
                    internal::RetireLog(/*min_depth=*/100, /*max_depth=*/1 << 13)) {
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

  bt_log(INFO, "hci", "initialized");
}

AclDataChannelImpl::~AclDataChannelImpl() {
  bt_log(INFO, "hci", "AclDataChannel shutting down");

  transport_->command_channel()->RemoveEventHandler(num_completed_packets_event_handler_id_);
  transport_->command_channel()->RemoveEventHandler(data_buffer_overflow_event_handler_id_);
}

void AclDataChannelImpl::AttachInspect(inspect::Node& parent, const std::string& name) {
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

void AclDataChannelImpl::SetDataRxHandler(ACLPacketHandler rx_callback) {
  BT_ASSERT(rx_callback);
  rx_callback_ = std::move(rx_callback);
  hci_->SetAclCallback(fit::bind_member<&AclDataChannelImpl::OnRxPacket>(this));
}

bool AclDataChannelImpl::SendPacket(ACLDataPacketPtr data_packet, UniqueChannelId channel_id,
                                    PacketPriority priority) {
  BT_ASSERT(data_packet);
  std::list<ACLDataPacketPtr> packets;
  packets.push_back(std::move(data_packet));
  return SendPackets(std::move(packets), channel_id, priority);
}

bool AclDataChannelImpl::SendPackets(std::list<ACLDataPacketPtr> packets,
                                     UniqueChannelId channel_id, PacketPriority priority) {
  if (packets.empty()) {
    bt_log(DEBUG, "hci", "no packets to send!");
    return false;
  }

  auto handle = packets.front()->connection_handle();

  if (registered_links_.find(handle) == registered_links_.end()) {
    bt_log(TRACE, "hci", "dropping packets for unregistered connection (handle: %#.4x, count: %lu)",
           handle, packets.size());
    return false;
  }

  // This call assumes that each call includes a full PDU, which means that there can't be a
  // continuing fragment at the head. There is no check for whether |packets| have enough data to
  // form whole PDUs because queue management doesn't require that and it would break abstraction
  // even more.
  BT_ASSERT_MSG(packets.front()->packet_boundary_flag() !=
                    hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                "expected full PDU");

  for (const auto& packet : packets) {
    // This call assumes that all packets in each call are for the same connection
    BT_ASSERT_MSG(packet->connection_handle() == handle,
                  "expected only fragments for one connection (%#.4x, got %#.4x)", handle,
                  packet->connection_handle());

    // Make sure that all packets are within the MTU.
    if (packet->view().payload_size() >
        GetBufferMtu(registered_links_[packet->connection_handle()])) {
      bt_log(ERROR, "hci", "ACL data packet too large!");
      return false;
    }
  }

  auto insert_iter = SendQueueInsertLocationForPriority(priority);
  for (int i = 0; !packets.empty(); i++) {
    auto packet = std::move(packets.front());
    packets.pop_front();
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
  BT_DEBUG_ASSERT(registered_links_.find(handle) == registered_links_.end());
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
  BT_ASSERT(registered_links_.find(handle) == registered_links_.end());

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

  fitx::result<zx_status_t, DynamicByteBuffer> encode_result =
      hci_->EncodeSetAclPriorityCommand(handle, priority);
  if (encode_result.is_error()) {
    bt_log(TRACE, "hci", "encoding ACL priority command failed");
    callback(fitx::failed());
    return;
  }

  DynamicByteBuffer encoded = std::move(encode_result.value());
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

size_t AclDataChannelImpl::GetBufferMtu(bt::LinkType ll_type) const {
  if (ll_type == bt::LinkType::kACL)
    return bredr_buffer_info_.max_data_length();
  return GetLeBufferInfo().max_data_length();
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

    BT_DEBUG_ASSERT(iter->second.count);
    if (iter->second.count < comp_packets) {
      bt_log(WARN, "hci", "packet tx count mismatch! (handle: %#.4x, expected: %zu, actual : %u)",
             le16toh(data->connection_handle), iter->second.count, comp_packets);

      iter->second.count = 0u;

      // On debug builds it's better to assert and crash so that we can catch
      // controller bugs. On release builds we log the warning message above and
      // continue.
      BT_PANIC("controller reported incorrect packet count!");
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
  BT_ASSERT(to_drop_iter != send_queue_->end());
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

AclDataChannelImpl::DataPacketQueue AclDataChannelImpl::TakePacketsToSend() {
  size_t avail_bredr_packets = GetNumFreeBREDRPackets();

  // On a dual mode controller that does not implement separate buffers, the BR/EDR buffer is shared
  // for both techologies, so we must take care not to double count here.
  size_t num_free_le_packets = GetNumFreeLEPackets();
  const bool bredr_buffer_is_shared = !le_buffer_info_.IsAvailable();
  size_t& avail_le_packets = bredr_buffer_is_shared ? avail_bredr_packets : num_free_le_packets;

  // Based on what we know about controller buffer availability, we process
  // packets that are currently in |send_queue|. The packets that can be sent
  // are added to |to_send|. Packets that cannot be sent remain in
  // |send_queue|.
  DataPacketQueue to_send;
  InspectableGuard<DataPacketQueue> send_queue = send_queue_.Mutable();
  for (auto iter = send_queue->begin(); iter != send_queue->end();) {
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
    iter = send_queue->erase(iter);
  }

  return to_send;
}

void AclDataChannelImpl::TrySendNextQueuedPackets() {
  DataPacketQueue to_send = TakePacketsToSend();

  if (to_send.empty())
    return;

  size_t bredr_packets_sent = 0;
  size_t le_packets_sent = 0;
  while (!to_send.empty()) {
    QueuedDataPacket& packet = to_send.front();
    const hci_spec::ConnectionHandle connection_handle = packet.packet->connection_handle();

    zx_status_t status = hci_->SendAclPacket(std::move(packet.packet));
    if (status != ZX_OK) {
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

    auto iter = pending_links_.find(connection_handle);
    if (iter == pending_links_.end()) {
      pending_links_[connection_handle] = PendingPacketData(packet.ll_type);
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
  BT_DEBUG_ASSERT(bredr_buffer_info_.max_num_packets() >= *num_sent_packets_);
  return bredr_buffer_info_.max_num_packets() - *num_sent_packets_;
}

size_t AclDataChannelImpl::GetNumFreeLEPackets() const {
  if (!le_buffer_info_.IsAvailable()) {
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
  if (!le_buffer_info_.IsAvailable()) {
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
  if (!le_buffer_info_.IsAvailable()) {
    IncrementTotalNumPackets(count);
    return;
  }

  BT_DEBUG_ASSERT(*le_num_sent_packets_ + count <= le_buffer_info_.max_num_packets());
  *le_num_sent_packets_.Mutable() += count;
}

void AclDataChannelImpl::OnRxPacket(std::unique_ptr<ACLDataPacket> packet) {
  BT_ASSERT(rx_callback_);
  {
    TRACE_DURATION("bluetooth", "AclDataChannelImpl->rx_callback_");
    rx_callback_(std::move(packet));
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
  BT_DEBUG_ASSERT(event.event_code() == hci_spec::kDataBufferOverflowEventCode);

  const auto& params = event.params<hci_spec::ConnectionRequestEventParams>();

  // Internal buffer state must be invalid and no further transmissions are possible.
  BT_PANIC("controller data buffer overflow event received (link type: %hhu)", params.link_type);

  return CommandChannel::EventCallbackResult::kContinue;
}

}  // namespace bt::hci
