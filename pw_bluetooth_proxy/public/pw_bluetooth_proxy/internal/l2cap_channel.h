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

#pragma once

#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

// Base class for peer-to-peer L2CAP-based channels.
//
// Protocol-dependent information that is fixed per channel, such as addresses,
// flags, handles, etc. should be provided at construction to derived channels.
class L2capChannel : public IntrusiveForwardList<L2capChannel>::Item {
 public:
  enum class State {
    kRunning,
    // Channel is stopped, but the L2CAP connection has not been closed.
    kStopped,
    // L2CAP connection has been closed as the result of an
    // HCI_Disconnection_Complete event, L2CAP_DISCONNECTION_RSP packet, or
    // HCI_Reset Command packet; or `ProxyHost` dtor has been called.
    kClosed,
    // Channel has been moved from and is no longer a valid object.
    kUndefined,
  };

  L2capChannel(const L2capChannel& other) = delete;
  L2capChannel& operator=(const L2capChannel& other) = delete;
  // Channels are moved to the client after construction.
  L2capChannel(L2capChannel&& other);
  // Move assignment operator allows channels to be erased from pw::Vector.
  L2capChannel& operator=(L2capChannel&& other);

  virtual ~L2capChannel();

  // Enter `State::kStopped`. This means
  //   - Queue is cleared so pending sends will not complete.
  //   - Calls to `QueuePacket()` will return PW_STATUS_FAILED_PRECONDITION, so
  //     derived channels should not accept client writes.
  //   - Rx packets will be dropped & trigger `kRxWhileStopped` events.
  //   - Container is responsible for closing L2CAP connection & destructing
  //     the channel object to free its resources.
  void Stop();

  // Deregister the channel and enter `State::kClosed`. Closing a channel has
  // the same effects as stopping the channel and triggers
  // `L2capChannelEvent::kChannelClosedByOther`.
  //
  // Deregistered channels are not managed by the proxy, so any traffic
  // addressed to/from them passes through `ProxyHost` unaffected. (Rx packets
  // do not trigger `kRxWhileStopped` events.)
  void Close();

  //-------------
  //  Tx (public)
  //-------------

  // TODO: https://pwbug.dev/388082771 - Most of these APIs should be in a
  // public header. Will probably do that as part of moving them to
  // ClientChannel.

  /// Send a payload to the remote peer.
  ///
  /// @param[in] payload The client payload to be sent. Payload will be
  /// destroyed once its data has been used.
  ///
  /// @returns A StatusWithMultiBuf with one of the statuses below. If status is
  /// not OK then payload is also returned in StatusWithMultiBuf.
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error). If an
  ///                       `event_fn` has been provided it will be called with
  ///                       `L2capChannelEvent::kWriteAvailable` when there is
  ///                       queue space available again.
  ///  INVALID_ARGUMENT:    If payload is too large or if payload is not a
  ///                       contiguous MultiBuf.
  ///  FAILED_PRECONDITION: If channel is not `State::kRunning`.
  ///  UNIMPLEMENTED:       If channel does not support Write(MultiBuf).
  /// @endrst
  // TODO: https://pwbug.dev/388082771 - Plan to eventually move this to
  // ClientChannel.
  inline virtual StatusWithMultiBuf Write(pw::multibuf::MultiBuf&& payload) {
    if (UsesPayloadQueue()) {
      return WriteToPayloadQueue(std::move(payload));
    } else {
      return WriteToPduQueue(std::move(payload));
    }
  }

  /// Send an L2CAP payload to the remote peer.
  ///
  /// @param[in] payload The L2CAP payload to be sent. Payload will be copied
  ///                    before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error). If an
  ///                       `event_fn` has been provided it will be called with
  ///                       `L2capChannelEvent::kWriteAvailable` when there is
  ///                       queue space available again.
  ///  INVALID_ARGUMENT:    If payload is too large.
  ///  FAILED_PRECONDITION  If channel is not `State::kRunning`.
  ///  UNIMPLEMENTED:       If channel does not support Write(MultiBuf).
  /// @endrst
  // Channels other than `L2capCoc` use this Write, but plan is to move them
  // all to using Write(MultiBuf).
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to Write(MultiBuf).
  virtual pw::Status Write(pw::span<const uint8_t> payload);

  /// Determine if channel is ready to accept one or more Write payloads.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///    OK: Channel is ready to accept one or more Write payloads.
  ///
  ///    UNAVAILABLE: Channel does not yet have the resources to queue a Write
  ///    at this time (transient error). If an `event_fn` has been provided it
  ///    will be called with `L2capChannelEvent::kWriteAvailable` when there is
  ///    queue space available again.
  ///
  ///    FAILED_PRECONDITION: If channel is not `State::kRunning`.
  ///
  /// @endrst
  ///
  Status IsWriteAvailable();

  // Dequeue a packet if one is available to send.
  [[nodiscard]] virtual std::optional<H4PacketWithH4> DequeuePacket();

  // Max number of Tx L2CAP packets that can be waiting to send.
  static constexpr size_t QueueCapacity() { return kQueueCapacity; }

  //-------------
  //  Rx (public)
  //-------------

  // Handle a Tx L2CAP PDU.
  //
  // Implementations should call `SendPayloadFromHostToClient` after processing
  // the PDU.
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth
  // controller.
  [[nodiscard]] virtual bool HandlePduFromHost(pw::span<uint8_t> l2cap_pdu) = 0;

  // Called when an L2CAP PDU is received on this channel. If channel is
  // `kRunning`, returns `HandlePduFromController(l2cap_pdu)`. If channel is not
  // `State::kRunning`, sends `kRxWhileStopped` event to client and drops PDU.
  // This function will call DoHandlePduFromController on its subclass.
  [[nodiscard]] bool HandlePduFromController(pw::span<uint8_t> l2cap_pdu);

  // Handle fragmented Rx L2CAP PDU. Default implementation stops channel and
  // sends `kRxFragmented` event to client.
  // TODO: https://pwbug.dev/365179076 - Support recombination & delete this.
  virtual void HandleFragmentedPdu();

  //--------------
  //  Accessors:
  //--------------

  State state() const { return state_; }

  uint16_t local_cid() const { return local_cid_; }

  uint16_t remote_cid() const { return remote_cid_; }

  uint16_t connection_handle() const { return connection_handle_; }

  AclTransportType transport() const { return transport_; }

 protected:
  friend class L2capChannelManager;

  //----------------------
  //  Creation (protected)
  //----------------------

  explicit L2capChannel(
      L2capChannelManager& l2cap_channel_manager,
      multibuf::MultiBufAllocator* rx_multibuf_allocator,
      uint16_t connection_handle,
      AclTransportType transport,
      uint16_t local_cid,
      uint16_t remote_cid,
      OptionalPayloadReceiveCallback&& payload_from_controller_fn,
      OptionalPayloadReceiveCallback&& payload_from_host_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  // Returns whether or not ACL connection handle & L2CAP channel identifiers
  // are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t local_cid,
                                               uint16_t remote_cid);

  //-------------------
  //  Other (protected)
  //-------------------

  // Send `event` to client if an event callback was provided.
  void SendEvent(L2capChannelEvent event);

  // Helper since these operations should typically be coupled.
  void StopAndSendEvent(L2capChannelEvent event) {
    Stop();
    SendEvent(event);
  }

  // Enter `State::kClosed` without deregistering. This has all the same effects
  // as stopping the channel and triggers `event`. No-op if channel is already
  // `State::kClosed`.
  void InternalClose(
      L2capChannelEvent event = L2capChannelEvent::kChannelClosedByOther);

  // For derived channels to use in lock annotations.
  const sync::Mutex& send_queue_mutex() const
      PW_LOCK_RETURNED(send_queue_mutex_) {
    return send_queue_mutex_;
  }

  //----------------
  //  Tx (protected)
  //----------------

  // Queue L2CAP `packet` for sending and `ReportPacketsMayBeReadyToSend()`.
  //
  // Returns PW_STATUS_UNAVAILABLE if queue is full (transient error).
  // Returns PW_STATUS_FAILED_PRECONDITION if channel is not `State::kRunning`.
  //
  // Channels other than `L2capCoc` use QueuePacket(), but plan is to move them
  // all to using QueuePayload().
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to QueuePayload.
  [[nodiscard]] virtual Status QueuePacket(H4PacketWithH4&& packet);

  // Pop front buffer. Queue must be nonempty.
  void PopFrontPayload() PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  // Returns span over front buffer. Queue must be nonempty.
  ConstByteSpan GetFrontPayloadSpan() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  bool PayloadQueueEmpty() const PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  // Reserve an L2CAP over ACL over H4 packet, with those three headers
  // populated for an L2CAP PDU payload of `data_length` bytes addressed to
  // `connection_handle_`.
  //
  // Returns PW_STATUS_INVALID_ARGUMENT if payload is too large for a buffer.
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  pw::Result<H4PacketWithH4> PopulateTxL2capPacket(uint16_t data_length);

  // Return if we can generally handle the provided data length.
  // Note PopulateTxL2capPacket can still fail if buffers or memory are not
  // available at that time.
  bool IsOkL2capDataLength(uint16_t data_length);

  // If all H4 buffers are occupied, this variant primes the kWriteAvailable
  // event to be sent once buffer space becomes available again.
  //
  // TODO: https://pwbug.dev/379337272 - Once derived channels migrate to
  // queueing client payloads on Write() instead of populating Tx packets, then
  // delete this variant.
  pw::Result<H4PacketWithH4> PopulateTxL2capPacketDuringWrite(
      uint16_t data_length) PW_LOCKS_EXCLUDED(send_queue_mutex_);

  // Returns the maximum size supported for Tx L2CAP PDU payloads.
  //
  // Returns std::nullopt if LE_ACL_Data_Packet_Length was not yet provided in
  // an LE_Read_Buffer_Size command complete event.
  std::optional<uint16_t> MaxL2capPayloadSize() const;

  // Alert `L2capChannelManager` that queued packets may be ready to send.
  // When calling this method, ensure no locks are held that are also acquired
  // in `Dequeue()` overrides.
  void ReportPacketsMayBeReadyToSend() PW_LOCKS_EXCLUDED(send_queue_mutex_);

  // Remove all packets from queue.
  void ClearQueue();

  //-------
  //  Rx (protected)
  //-------

  // Returns false if payload should be forwarded to controller instead.
  virtual bool SendPayloadFromHostToClient(pw::span<uint8_t> payload);

  // Returns false if payload should be forwarded to host instead.
  virtual bool SendPayloadFromControllerToClient(pw::span<uint8_t> payload);

  multibuf::MultiBufAllocator* rx_multibuf_allocator() const {
    return rx_multibuf_allocator_;
  }

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // TODO: https://pwbug.dev/349700888 - Make capacity configurable.
  static constexpr size_t kQueueCapacity = 5;

  // Return true if the current object uses payload_queue_.
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to payload_queue_.
  virtual bool UsesPayloadQueue() = 0;

  // Returns false if payload should be forwarded to host instead.
  bool SendPayloadToClient(pw::span<uint8_t> payload,
                           OptionalPayloadReceiveCallback& callback);

  // Enter `State::kUndefined`, indicating that the channel has been moved from
  // and is no longer a valid object.
  void Undefine();

  // Helper for move constructor and move assignment.
  void MoveFields(L2capChannel& other) PW_LOCKS_EXCLUDED(send_queue_mutex_);

  L2capChannelManager& l2cap_channel_manager_;

  State state_;

  // ACL connection handle.
  uint16_t connection_handle_;

  AclTransportType transport_;

  // L2CAP channel ID of local endpoint.
  uint16_t local_cid_;

  // L2CAP channel ID of remote endpoint.
  uint16_t remote_cid_;

  // Notify clients of asynchronous events encountered such as errors.
  Function<void(L2capChannelEvent event)> event_fn_;

  // Reserve an L2CAP packet over ACL over H4 packet.
  pw::Result<H4PacketWithH4> PopulateL2capPacket(uint16_t data_length);

  //--------------
  //  Tx (private)
  //--------------

  // Queue a client `buf` for sending and `ReportPacketsMayBeReadyToSend()`.
  // Must be a contiguous MultiBuf.
  //
  // Returns PW_STATUS_UNAVAILABLE if queue is full (transient error).
  // Returns PW_STATUS_FAILED_PRECONDITION if channel is not `State::kRunning`.
  StatusWithMultiBuf QueuePayload(multibuf::MultiBuf&& buf)
      PW_LOCKS_EXCLUDED(send_queue_mutex_);

  // Writes the contents of MultiBuf to the PDU queue (send_queue_).
  //
  // The contents of the MultiBuf are copied during the call and the MultiBuf
  // is destroyed.
  //
  // Called for subclasses that don't do payload queueing (as determined by
  // UsesPayloadQueue) during the transition.
  // TODO: https://pwbug.dev/379337272 - Delete when all channels are
  // transitioned to using payload queues.
  StatusWithMultiBuf WriteToPduQueue(multibuf::MultiBuf&& payload);

  // Writes the MultiBuf to the payload queue (payload_queue_).
  //
  // Called for subclasses that don't do payload queueing (as determined by
  // UsesPayloadQueue) during the transition.
  // TODO: https://pwbug.dev/379337272 - Delete when all channels are
  // transitioned to using payload queues.
  StatusWithMultiBuf WriteToPayloadQueue(multibuf::MultiBuf&& payload);

  // Return the next Tx PDU based on the client's queued payloads. If the
  // returned PDU will complete the transmission of a payload, that payload
  // should be popped from the queue. If no payloads are queued, return
  // std::nullopt.
  //
  // Note this is overrode by `L2capCoc` which uses `payload_queue_` rather than
  // `send_queue_`. The plan is to move all channels to using `payload_queue_`.
  // TODO: https://pwbug.dev/379337272 - Make pure virtual once all derived
  // channels implement this method.
  virtual std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  // `L2capChannelManager` and channel may concurrently call functions that
  // access queue.
  sync::Mutex send_queue_mutex_;

  // Stores Tx L2CAP packets.
  //
  // This queue is used for channels other than `L2capCoc`, but we plan to
  // transition all channels to using `payload_queue_` below.
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to payload_queue_.
  InlineQueue<H4PacketWithH4, kQueueCapacity> send_queue_
      PW_GUARDED_BY(send_queue_mutex_);

  // Stores client Tx payload buffers.
  InlineQueue<multibuf::MultiBuf, kQueueCapacity> payload_queue_
      PW_GUARDED_BY(send_queue_mutex_);

  // True if the last queue attempt didn't have space. Will be cleared on
  // successful dequeue.
  bool notify_on_dequeue_ PW_GUARDED_BY(send_queue_mutex_) = false;

  //--------------
  //  Rx (private)
  //--------------

  // Handle an Rx L2CAP PDU.
  //
  // Implementations should call `SendPayloadFromControllerToClient` after
  // recombining/processing the PDU (e.g. after updating channel state and
  // screening out certain PDUs).
  //
  // Return true if the PDU was consumed by the channel. Otherwise, return false
  // and the PDU will be forwarded by `ProxyHost` on to the Bluetooth host.
  [[nodiscard]] virtual bool DoHandlePduFromController(
      pw::span<uint8_t> l2cap_pdu) = 0;

  //--------------
  //  Data members
  //--------------

  // Optional client-provided multibuf allocator.
  multibuf::MultiBufAllocator* rx_multibuf_allocator_;

  // Client-provided controller read callback.
  OptionalPayloadReceiveCallback payload_from_controller_fn_;
  // Client-provided host read callback.
  OptionalPayloadReceiveCallback payload_from_host_fn_;
};

}  // namespace pw::bluetooth::proxy
