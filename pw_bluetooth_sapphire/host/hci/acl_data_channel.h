// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_CHANNEL_H_

#include <lib/async/cpp/wait.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/zx/channel.h>
#include <zircon/compiler.h>

#include <list>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace hci {

class Transport;

// Represents the controller data buffer settings for the BR/EDR or LE
// transports.
class DataBufferInfo {
 public:
  // Initialize fields to non-zero values.
  DataBufferInfo(size_t max_data_length, size_t max_num_packets);

  // The default constructor sets all fields to zero. This can be used to
  // represent a data buffer that does not exist (e.g. the controller has a
  // single shared buffer and no dedicated LE buffer.
  DataBufferInfo();

  // The maximum length (in octets) of the data portion of each HCI ACL data
  // packet that the controller is able to accept.
  size_t max_data_length() const { return max_data_length_; }

  // Returns the total number of HCI ACL data packets that can be stored in the
  // data buffer represented by this object.
  size_t max_num_packets() const { return max_num_packets_; }

  // Returns true if both fields are set to zero.
  bool IsAvailable() const { return max_data_length_ && max_num_packets_; }

  // Comparison operators.
  bool operator==(const DataBufferInfo& other) const;
  bool operator!=(const DataBufferInfo& other) const { return !(*this == other); }

 private:
  size_t max_data_length_;
  size_t max_num_packets_;
};

// Represents the Bluetooth ACL Data channel and manages the Host<->Controller
// ACL data flow control.
//
// This currently only supports the Packet-based Data Flow Control as defined in
// Core Spec v5.0, Vol 2, Part E, Section 4.1.1.
using ACLPacketPredicate =
    fit::function<bool(const ACLDataPacketPtr& packet, l2cap::ChannelId channel_id)>;

class ACLDataChannel final {
 public:
  enum class PacketPriority { kHigh, kLow };

  ACLDataChannel(Transport* transport, zx::channel hci_acl_channel);
  ~ACLDataChannel();

  // Starts listening on the HCI ACL data channel and starts handling data flow
  // control. |bredr_buffer_info| represents the controller's data buffering
  // capacity for the BR/EDR transport and the |le_buffer_info| represents Low
  // Energy buffers. At least one of these (BR/EDR vs LE) must contain non-zero
  // values per Core Spec v5.0 Vol 2, Part E, Sec 4.1.1:
  //
  //   - A LE only controller will have LE buffers only.
  //   - A BR/EDR-only controller will have BR/EDR buffers only.
  //   - A dual-mode controller will have BR/EDR buffers and MAY have LE buffers
  //     if the BR/EDR buffer is not shared between the transports.
  //
  // As this class is intended to support flow-control for both, this function
  // should be called based on what is reported by the controller.
  void Initialize(const DataBufferInfo& bredr_buffer_info, const DataBufferInfo& le_buffer_info);

  // Unregisters event handlers and cleans up.
  // NOTE: Initialize() and ShutDown() MUST be called on the same thread. These
  // methods are not thread-safe.
  void ShutDown();

  // Assigns a handler callback for received ACL data packets. |rx_callback| will be posted on
  // |dispatcher| and shall take ownership of each packet received from the controller.
  void SetDataRxHandler(ACLPacketHandler rx_callback, async_dispatcher_t* rx_dispatcher);

  // Queues the given ACL data packet to be sent to the controller. Returns
  // false if the packet cannot be queued up, e.g. if the size of |data_packet|
  // exceeds the MTU for the link type set in RegisterLink().
  //
  // |data_packet| is passed by value, meaning that ACLDataChannel will take
  // ownership of it. |data_packet| must represent a valid ACL data packet.
  //
  // |channel_id| must match the l2cap channel that the packet is being sent to. It is needed to
  // determine what channel l2cap packet fragments are being sent to when revoking queued packets
  // for specific channels that have closed. If the packet does not contain a fragment of an l2cap
  // packet, |channel_id| should be set to |l2cap::kInvalidChannelId|.
  //
  // |priority| indicates the order this packet should be dispatched off of the queue relative to
  // packets of other priorities. Note that high priority packets may still wait behind low priority
  // packets that have already been sent to the controller.
  bool SendPacket(ACLDataPacketPtr data_packet, l2cap::ChannelId channel_id,
                  PacketPriority priority = PacketPriority::kLow);

  // Queues the given list of ACL data packets to be sent to the controller. The
  // behavior is identical to that of SendPacket() with the guarantee that all
  // packets that are in |packets| are queued atomically. If any packet's handle is not registered
  // in the allowlist, then none will be queued.
  //
  // Takes ownership of the contents of |packets|. Returns false if |packets|
  // contains an element that exceeds the MTU for its link type or |packets| is empty.
  //
  // |channel_id| must match the l2cap channel that all packets is being sent to. It is needed to
  // determine what channel l2cap packet fragments are being sent to when revoking queued packets
  // for channels that have closed. If the packets do not contain a fragment of an l2cap
  // packet, |channel_id| should be set to |l2cap::kInvalidChannelId|.
  //
  // |priority| indicates the order this packet should be dispatched off of the queue relative to
  // packets of other priorities. Note that high priority packets may still wait behind low priority
  // packets that have already been sent to the controller.
  bool SendPackets(LinkedList<ACLDataPacket> packets, l2cap::ChannelId channel_id,
                   PacketPriority priority = PacketPriority::kLow);

  // Allowlist packets destined for the link identified by |handle| (of link type |ll_type|) for
  // submission to the controller.
  //
  // Failure to register a link before sending packets will result in the packets
  // being dropped immediately. A handle must not be registered again until after UnregisterLink has
  // been called on that handle.
  void RegisterLink(hci::ConnectionHandle handle, Connection::LinkType ll_type);

  // Cleans up all outgoing data buffering state related to the logical link
  // with the given |handle|. This must be called upon disconnection of a link
  // to ensure that stale outbound packets are filtered out of the send queue.
  // All future packets sent to this link will be dropped.
  //
  // |RegisterLink| must be called before |UnregisterLink| for the same handle.
  //
  // |UnregisterLink| does not clear the controller packet count, so |ClearControllerPacketCount|
  // must be called after |UnregisterLink| and the HCI Disconnection Complete event has been
  // received.
  void UnregisterLink(hci::ConnectionHandle handle);

  // Removes all queued data packets for which |predicate| returns true.
  void DropQueuedPackets(ACLPacketPredicate predicate);

  // Resets controller packet count for |handle| so that controller buffer credits can be reused.
  // This must be called on the HCI_Disconnection_Complete event to notify ACLDataChannel that
  // packets in the controller's buffer for |handle| have been flushed. See Core Spec v5.1, Vol 2,
  // Part E, Section 4.3. This must be called after |UnregisterLink|.
  void ClearControllerPacketCount(hci::ConnectionHandle handle);

  // Returns the underlying channel handle.
  const zx::channel& channel() const { return channel_; }

  // Returns the BR/EDR buffer information that the channel was initialized
  // with.
  const DataBufferInfo& GetBufferInfo() const;

  // Returns the LE buffer information that the channel was initialized with.
  // This defaults to the BR/EDR buffers if the controller does not have a
  // dedicated LE buffer.
  const DataBufferInfo& GetLEBufferInfo() const;

 private:
  // Represents a queued ACL data packet.
  struct QueuedDataPacket {
    QueuedDataPacket(Connection::LinkType ll_type, l2cap::ChannelId channel_id,
                     PacketPriority priority, ACLDataPacketPtr packet)
        : ll_type(ll_type), channel_id(channel_id), priority(priority), packet(std::move(packet)) {}

    QueuedDataPacket() = default;
    QueuedDataPacket(QueuedDataPacket&& other) = default;
    QueuedDataPacket& operator=(QueuedDataPacket&& other) = default;

    Connection::LinkType ll_type;
    l2cap::ChannelId channel_id;
    PacketPriority priority;
    ACLDataPacketPtr packet;
  };

  // Drops all packets that |predicate| returns true for. This locked version is required for
  // UnregisterLink to call this method.
  void DropQueuedPacketsLocked(ACLPacketPredicate predicate) __TA_REQUIRES(send_mutex_);

  // Returns the data buffer MTU for the given connection.
  size_t GetBufferMTU(Connection::LinkType ll_type) const;

  // Handler for the HCI Number of Completed Packets Event, used for
  // packet-based data flow control.
  CommandChannel::EventCallbackResult NumberOfCompletedPacketsCallback(const EventPacket& event);

  // Tries to send the next batch of queued data packets if the controller has
  // any space available. All packets in higher priority queues will be sent before packets in lower
  // priority queues.
  void TrySendNextQueuedPacketsLocked() __TA_REQUIRES(send_mutex_);

  // Returns the number of BR/EDR packets for which the controller has available
  // space to buffer.
  size_t GetNumFreeBREDRPacketsLocked() const __TA_REQUIRES(send_mutex_);

  // Returns the number of LE packets for which controller has available space
  // to buffer. Must be called from a locked context.
  size_t GetNumFreeLEPacketsLocked() const __TA_REQUIRES(send_mutex_);

  // Decreases the total number of sent packets count by the given amount. Must
  // be called from a locked context.
  void DecrementTotalNumPacketsLocked(size_t count) __TA_REQUIRES(send_mutex_);

  // Decreases the total number of sent packets count for LE by the given
  // amount. Must be called from a locked context.
  void DecrementLETotalNumPacketsLocked(size_t count) __TA_REQUIRES(send_mutex_);

  // Increments the total number of sent packets count by the given amount. Must
  // be called from a locked context.
  void IncrementTotalNumPacketsLocked(size_t count) __TA_REQUIRES(send_mutex_);

  // Increments the total number of sent LE packets count by the given amount.
  // Must be called from a locked context.
  void IncrementLETotalNumPacketsLocked(size_t count) __TA_REQUIRES(send_mutex_);

  // Read Ready Handler for |channel_|
  void OnChannelReady(async_dispatcher_t* dispatcher, async::WaitBase* wait, zx_status_t status,
                      const zx_packet_signal_t* signal);

  // Handler for HCI_Buffer_Overflow_event
  CommandChannel::EventCallbackResult DataBufferOverflowCallback(const EventPacket& event);

  // Used to assert that certain public functions are only called on the
  // creation thread.
  fxl::ThreadChecker thread_checker_;

  // The Transport object that owns this instance.
  Transport* transport_;  // weak;

  // The channel that we use to send/receive HCI ACL data packets.
  zx::channel channel_;

  // Wait object for |channel_|
  async::WaitMethod<ACLDataChannel, &ACLDataChannel::OnChannelReady> channel_wait_{this};

  // True if this instance has been initialized through a call to Initialize().
  std::atomic_bool is_initialized_;

  // The event handler ID for the Number Of Completed Packets event.
  CommandChannel::EventHandlerId num_completed_packets_event_handler_id_;

  // The event handler ID for the Data Buffer Overflow event.
  CommandChannel::EventHandlerId data_buffer_overflow_event_handler_id_;

  // The dispatcher used for posting tasks on the HCI transport I/O thread.
  async_dispatcher_t* io_dispatcher_;

  // The current handler for incoming data and the dispatcher on which to run
  // it.
  std::mutex rx_mutex_;
  ACLPacketHandler rx_callback_ __TA_GUARDED(rx_mutex_);
  async_dispatcher_t* rx_dispatcher_ __TA_GUARDED(rx_mutex_);

  // BR/EDR data buffer information. This buffer will not be available on
  // LE-only controllers.
  DataBufferInfo bredr_buffer_info_;

  // LE data buffer information. This buffer will not be available on
  // BR/EDR-only controllers (which we do not support) and MAY be available on
  // dual-mode controllers. We maintain that if this buffer is not available,
  // then the BR/EDR buffer MUST be available.
  DataBufferInfo le_buffer_info_;

  // Mutex that guards access to data transmission related members below.
  std::mutex send_mutex_;

  // The current count of the number of ACL data packets that have been sent to
  // the controller. |le_num_sent_packets_| is ignored if the controller uses
  // one buffer for LE and BR/EDR.
  size_t num_sent_packets_ __TA_GUARDED(send_mutex_);
  size_t le_num_sent_packets_ __TA_GUARDED(send_mutex_);

  // The ACL data packet queue contains the data packets that are waiting to be
  // sent to the controller.
  // TODO(armansito): Use priority_queue based on L2CAP channel priority.
  // TODO(NET-1211): Keep a separate queue for each open connection. Benefits:
  //   * Helps address the packet-prioritization TODO above.
  //   * Also: having separate queues, which know their own
  //     Connection::LinkType, would let us replace std::list<QueuedDataPacket>
  //     with LinkedList<ACLDataPacket> which has a more efficient
  //     memory layout.
  using DataPacketQueue = std::list<QueuedDataPacket>;
  DataPacketQueue send_queue_ __TA_GUARDED(send_mutex_);

  // Returns an iterator to the location new packets should be inserted into |send_queue_| based on
  // their |priority|:
  // If |priority| is |kLow|: returns past-the-end of |send_queue_|.
  // If |priority| is |kHigh|: returns the location of the first |kLow| priority packet.
  DataPacketQueue::iterator SendQueueInsertLocationForPriority(PacketPriority priority)
      __TA_REQUIRES(send_mutex_);

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
  std::unordered_map<ConnectionHandle, PendingPacketData> pending_links_ __TA_GUARDED(send_mutex_);

  // Stores links registered by RegisterLink
  std::unordered_map<hci::ConnectionHandle, Connection::LinkType> registered_links_
      __TA_GUARDED(send_mutex_);

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ACLDataChannel);
};

}  // namespace hci
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HCI_ACL_DATA_CHANNEL_H_
