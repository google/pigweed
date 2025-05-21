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

#include <cstdint>

#include "pw_assert/assert.h"
#include "pw_assert/check.h"
#include "pw_bluetooth_proxy/direction.h"
#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/try.h"
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

  // L2capChannels can be held by a Holder that the channel will provide tx
  // packets to and accept rx packets and events from. This is typically a
  // ChannelProxy or an object that manages ChannelProxies.
  class Holder {
   public:
    explicit Holder(L2capChannel* underlying_channel) {
      SetUnderlyingChannel(underlying_channel);
    }
    virtual ~Holder() = default;

    Holder(const Holder& other) = delete;
    Holder& operator=(const Holder& other) = delete;
    Holder(Holder&& other) { MoveFields(other); }
    Holder& operator=(Holder&& other) {
      MoveFields(other);

      return *this;
    }

   protected:
    // Allow L2capChannel to access underlying channel and call
    // HandleUnderlyingEvent without exposing it to clients in subclass
    // ChannelProxy.
    friend L2capChannel;

    void SetUnderlyingChannel(L2capChannel* underlying_channel) {
      underlying_channel_ = underlying_channel;
      underlying_channel_->SetHolder(this);
    }

    // TODO: https://pwbug.dev/388082771 - Being used for WIP testing. Delete
    // when done.
    void CheckUnderlyingChannel(L2capChannel* expected) {
      PW_CHECK(underlying_channel_ == expected);
    }

   private:
    void MoveFields(Holder& other) {
      underlying_channel_ = other.underlying_channel_;
      other.underlying_channel_ = nullptr;
      if (underlying_channel_) {
        underlying_channel_->SwitchHolder(&other, this);
      }
    }

    // Underlying L2capChannel that this object is holding.
    L2capChannel* underlying_channel_ = nullptr;
  };

  L2capChannel(const L2capChannel& other) = delete;
  L2capChannel& operator=(const L2capChannel& other) = delete;
  L2capChannel(L2capChannel&& other);
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
  StatusWithMultiBuf Write(pw::multibuf::MultiBuf&& payload);

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
      ChannelEventCallback&& event_fn);

  // Returns whether or not ACL connection handle & L2CAP channel identifiers
  // are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t local_cid,
                                               uint16_t remote_cid);

  // Set the holder for this L2capChannel. Should not already be set.
  // Should only be called L2capChannel::Holder.
  void SetHolder(Holder* holder) {
    PW_ASSERT(holder);
    holder_ = holder;
    // Verify holder has this as its underlying channel.
    holder_->CheckUnderlyingChannel(this);
  }

  // Switch to indicated holder for this L2capChannel.
  // Intended to only be be called by L2capChannel::Holder move functions.
  void SwitchHolder(Holder* old_holder, Holder* new_holder) {
    PW_ASSERT(holder_ == old_holder);
    holder_ = new_holder;
    //
    holder_->CheckUnderlyingChannel(this);
  }

  // Verifies current holder matches `expected`.
  void CheckHolder(Holder* expected) { PW_CHECK(holder_ == expected); }

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

  // Called on channel closure, i.e. when the ACL connection or L2CAP connection
  // is being dropped. Derived channels should override this to clean up state
  // that is being invalidated, such as dangling references to the channel's
  // underlying `AclConnection`.
  virtual void DoClose() = 0;

  // Enter `State::kClosed` without deregistering. This has all the same effects
  // as stopping the channel and triggers `event`. No-op if channel is already
  // `State::kClosed`.
  void InternalClose(
      L2capChannelEvent event = L2capChannelEvent::kChannelClosedByOther);

  // For derived channels to use in lock annotations.
  const sync::Mutex& l2cap_tx_mutex() const PW_LOCK_RETURNED(tx_mutex_) {
    return tx_mutex_;
  }

  //----------------
  //  Tx (protected)
  //----------------

  /// Check if the passed Write parameter is acceptable.
  virtual Status DoCheckWriteParameter(pw::multibuf::MultiBuf& payload) = 0;

  // Channels that need to send a payload during handling a received packet
  // directly (for instance to replenish credits) should use this function which
  // does not take the L2capChannelManager channels lock.
  inline StatusWithMultiBuf WriteDuringRx(pw::multibuf::MultiBuf&& payload) {
    return WriteLocked(std::move(payload));
  }

  // Write payload to queue but don't drain the queue as this would require
  // taking L2capChannelManager channel_mutex_ lock.
  StatusWithMultiBuf WriteLocked(pw::multibuf::MultiBuf&& payload);

  // Pop front buffer. Queue must be nonempty.
  void PopFrontPayload() PW_EXCLUSIVE_LOCKS_REQUIRED(tx_mutex_);

  // Returns span over front buffer. Queue must be nonempty.
  ConstByteSpan GetFrontPayloadSpan() const
      PW_EXCLUSIVE_LOCKS_REQUIRED(tx_mutex_);

  bool PayloadQueueEmpty() const PW_EXCLUSIVE_LOCKS_REQUIRED(tx_mutex_);

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

  // Returns the maximum size supported for Tx L2CAP PDU payloads.
  //
  // Returns std::nullopt if LE_ACL_Data_Packet_Length was not yet provided in
  // an LE_Read_Buffer_Size command complete event.
  std::optional<uint16_t> MaxL2capPayloadSize() const;

  // Alert `L2capChannelManager` that queued packets may be ready to send.
  void ReportNewTxPacketsOrCredits();

  // Tell `L2capChannelManager` to try and send all available queued
  // packets. When calling this method, ensure no locks are held that are
  // also acquired in `Dequeue()` overrides, and that the channels lock is
  // not held either.
  void DrainChannelQueuesIfNewTx() PW_LOCKS_EXCLUDED(tx_mutex_);

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

  // Returns false if payload should be forwarded to host instead.
  bool SendPayloadToClient(pw::span<uint8_t> payload,
                           OptionalPayloadReceiveCallback& callback);

  // Enter `State::kUndefined`, indicating that the channel has been moved from
  // and is no longer a valid object.
  void Undefine();

  // Helper for move constructor and move assignment.
  void MoveFields(L2capChannel& other) PW_LOCKS_EXCLUDED(tx_mutex_);

  // Reserve an L2CAP packet over ACL over H4 packet.
  pw::Result<H4PacketWithH4> PopulateL2capPacket(uint16_t data_length);

  //--------------
  //  Tx (private)
  //--------------

  // Queue a client `buf` for sending and `ReportNewTxPacketsOrCredits()`.
  // Must be a contiguous MultiBuf.
  //
  // Returns PW_STATUS_UNAVAILABLE if queue is full (transient error).
  // Returns PW_STATUS_FAILED_PRECONDITION if channel is not `State::kRunning`.
  StatusWithMultiBuf QueuePayload(multibuf::MultiBuf&& buf)
      PW_LOCKS_EXCLUDED(tx_mutex_);

  // Return the next Tx H4 based on the client's queued payloads. If the
  // returned PDU will complete the transmission of a payload, that payload
  // should be popped from the queue. If no payloads are queued, return
  // std::nullopt.
  // Subclasses should override to generate correct H4 packet from their
  // payload.
  virtual std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_EXCLUSIVE_LOCKS_REQUIRED(tx_mutex_) = 0;

  // Mutex for guarding tx state.
  sync::Mutex tx_mutex_;

  // Stores client Tx payload buffers.
  InlineQueue<multibuf::MultiBuf, kQueueCapacity> payload_queue_
      PW_GUARDED_BY(tx_mutex_);

  // True if the last queue attempt didn't have space. Will be cleared on
  // successful dequeue.
  bool notify_on_dequeue_ PW_GUARDED_BY(tx_mutex_) = false;

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

  //-------------
  //  Rx recombine - private, used by friend Recombiner only
  //-------------
  // TODO: https://pwbug.dev/404094475 - This section (and underlying data
  // member) could eventually move up into a AclChannel class.

  // The Rx combine functions private and only used by friend Recombiner.
  friend class Recombiner;

  // Create MultiBuf that Recombiner can use to store in-progress payload when
  // being recombined.
  pw::Status StartRecombinationBuf(Direction direction, size_t payload_size);

  // Returns true if this channel has a recombination MultiBuf (which means
  // recombination is active for this channel).
  bool HasRecombinationBuf(Direction direction) {
    return GetRecombinationBufOptRef(direction).has_value();
  }

  // Return the recombination buf to the caller.
  //
  // Channel no longer has buf after this call.
  multibuf::MultiBuf TakeRecombinationBuf(Direction direction) {
    PW_CHECK(GetRecombinationBufOptRef(direction).has_value());
    return std::exchange(GetRecombinationBufOptRef(direction), std::nullopt)
        .value();
  }

  // Copy the passed span to the recombination buf.
  //
  // Precondition: `HasCombinationBuf()`.
  pw::Status CopyToRecombinationBuf(Direction direction,
                                    ConstByteSpan data,
                                    uint16_t write_offset) {
    PW_CHECK(HasRecombinationBuf(direction));

    PW_TRY(GetRecombinationBufOptRef(direction)->CopyFrom(as_bytes(data),
                                                          write_offset));
    return pw::OkStatus();
  }

  // Return reference to the recombination MultiBuf.
  //
  // Intended for use just within L2capChannel functions.
  std::optional<multibuf::MultiBuf>& GetRecombinationBufOptRef(
      Direction direction) {
    return recombination_mbufs_[cpp23::to_underlying(direction)];
  }

  // Destroy the recombination MultiBuf.
  void EndRecombinationBuf(Direction direction);

  //--------------
  //  Data members
  //--------------

  // Holder to pass on rx and events to (if present).
  // Currently set explicitly by derived class during ctor/move since it's a
  // pointer to that derived instance.
  // TODO: https://pwbug.dev/388082771 - Update comment once we are composing
  // L2capChannel rather than inheriting from it.
  Holder* holder_ = nullptr;

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
  ChannelEventCallback event_fn_;

  // Optional client-provided multibuf allocator.
  multibuf::MultiBufAllocator* rx_multibuf_allocator_;

  // Client-provided controller read callback.
  OptionalPayloadReceiveCallback payload_from_controller_fn_;
  // Client-provided host read callback.
  OptionalPayloadReceiveCallback payload_from_host_fn_;

  // Recombination MultiBufs used by Recombiner to store in-progress
  // payloads when they are being recombined.
  // They are stored here so that they can be allocated with the channel's
  // allocator and also properly destroyed with the channel.
  std::array<std::optional<multibuf::MultiBuf>, kNumDirections>
      recombination_mbufs_{};
};

}  // namespace pw::bluetooth::proxy
