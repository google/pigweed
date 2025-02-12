// Copyright 2025 The Pigweed Authors
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
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_multibuf/multibuf.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

// Base class for client owned channels that can send payloads on some
// underlying connection type.
class ClientChannel : public IntrusiveForwardList<ClientChannel>::Item {
 public:
  enum class State {
    kRunning,
    // Channel is stopped, but the underlying connection has not been closed.
    kStopped,
    // Underlying connection has been closed, controller has been reset, or
    // `ProxyHost` dtor has been called.
    kClosed,
    // Channel has been moved from and is no longer a valid object.
    kUndefined,
  };

  ClientChannel(const ClientChannel& other) = delete;
  ClientChannel& operator=(const ClientChannel& other) = delete;
  // Channels are moved to the client after construction.
  ClientChannel(ClientChannel&& other);
  // Move assignment operator allows channels to be erased from pw::Vector.
  ClientChannel& operator=(ClientChannel&& other);

  virtual ~ClientChannel();

  // Enter `State::kStopped`. This means
  //   - Queue is cleared so pending sends will not complete.
  //   - Calls to `QueuePacket()` will return PW_STATUS_FAILED_PRECONDITION, so
  //     derived channels should not accept client writes.
  //   - Rx packets will be dropped & trigger `kRxWhileStopped` events.
  //   - Container is responsible for closing underlying connection &
  //     destructing the channel object to free its resources.
  void Stop();

  // Deregister the channel and enter `State::kClosed`. Closing a channel has
  // the same effects as stopping the channel and triggers
  // `ClientChannelEvent::kChannelClosedByOther`.
  //
  // Deregistered channels are not managed by the proxy, so any traffic
  // addressed to/from them passes through `ProxyHost` unaffected. (Rx packets
  // do not trigger `kRxWhileStopped` events.)
  void Close();

  //-------------
  //  Tx (public)
  //-------------

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
  ///                       `ClientChannelEvent::kWriteAvailable` when there is
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

  /// Send a payload to the remote peer.
  ///
  /// @param[in] payload The payload to be sent. Payload will be copied
  ///                    before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error). If an
  ///                       `event_fn` has been provided it will be called with
  ///                       `ClientChannelEvent::kWriteAvailable` when there is
  ///                       queue space available again.
  ///  INVALID_ARGUMENT:    If payload is too large.
  ///  FAILED_PRECONDITION  If channel is not `State::kRunning`.
  ///  UNIMPLEMENTED:       If channel does not support Write(MultiBuf).
  /// @endrst
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
  ///    will be called with `ClientChannelEvent::kWriteAvailable` when there is
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

  //--------------
  //  Accessors:
  //--------------

  State state() const { return state_; }

 protected:
  //----------------------
  //  Creation (protected)
  //----------------------

  explicit ClientChannel(Function<void(ClientChannelEvent event)>&& event_fn);

  //-------------------
  //  Other (protected)
  //-------------------

  // Called by Stop() to provide hook for child classes to customize stop
  // behavior.
  virtual void HandleStop() {}

  // Send `event` to client if an event callback was provided.
  void SendEvent(ClientChannelEvent event);

  // Helper since these operations should typically be coupled.
  void StopAndSendEvent(ClientChannelEvent event) {
    Stop();
    SendEvent(event);
  }

  // Called by Close() to provide hook for child classes to customize close
  // behavior.
  virtual void HandleClose() {}

  // Enter `State::kClosed` without deregistering. This has all the same effects
  // as stopping the channel and triggers `event`. No-op if channel is already
  // `State::kClosed`.
  void InternalClose(
      ClientChannelEvent event = ClientChannelEvent::kChannelClosedByOther);

  // For derived channels to use in lock annotations.
  const sync::Mutex& send_queue_mutex() const
      PW_LOCK_RETURNED(send_queue_mutex_) {
    return send_queue_mutex_;
  }

  //----------------
  //  Tx (protected)
  //----------------

  // Queue `packet` for sending and `ReportPacketsMayBeReadyToSend()`.
  //
  // Returns PW_STATUS_UNAVAILABLE if queue is full (transient error).
  // Returns PW_STATUS_FAILED_PRECONDITION if channel is not `State::kRunning`.
  //
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to QueuePayload.
  [[nodiscard]] virtual Status QueuePacket(H4PacketWithH4&& packet);

  // Pop front buffer. Queue must be nonempty.
  void PopFrontPayload() PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  // Returns span over front buffer. Queue must be nonempty.
  ConstByteSpan GetFrontPayloadSpan() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  bool PayloadQueueEmpty() const PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  // Signal that queued packets may be ready to send.
  // When calling this method, ensure no locks are held that are also acquired
  // in `Dequeue()` overrides.
  void ReportPacketsMayBeReadyToSend() PW_LOCKS_EXCLUDED(send_queue_mutex_);

  // Should be implemented in child classes to signal to underlying channel
  // manager that packets are ready.
  virtual void HandlePacketsMayBeReadyToSend() {}

  void SetNotifyOnDequeue() PW_LOCKS_EXCLUDED(send_queue_mutex_);

  // Remove all packets from queue.
  void ClearQueue();

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // TODO: https://pwbug.dev/349700888 - Make capacity configurable.
  static constexpr size_t kQueueCapacity = 5;

  // Return true if the current object uses payload_queue_.
  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to payload_queue_.
  virtual bool UsesPayloadQueue() = 0;

  // Enter `State::kUndefined`, indicating that the channel has been moved from
  // and is no longer a valid object.
  void Undefine();

  // Helper for move constructor and move assignment.
  void MoveFields(ClientChannel& other) PW_LOCKS_EXCLUDED(send_queue_mutex_);

  State state_;

  // Notify clients of asynchronous events encountered such as errors.
  Function<void(ClientChannelEvent event)> event_fn_;

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
  // TODO: https://pwbug.dev/379337272 - Make pure virtual once all derived
  // channels implement this method.
  virtual std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex_);

  sync::Mutex send_queue_mutex_;

  // Stores Tx packets.
  //
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
};

}  // namespace pw::bluetooth::proxy
