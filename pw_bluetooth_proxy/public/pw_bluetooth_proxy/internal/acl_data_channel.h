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

#include <optional>

#include "pw_bluetooth/hci_data.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth_proxy/internal/hci_transport.h"
#include "pw_bluetooth_proxy/internal/l2cap_aclu_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_leu_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_bluetooth_proxy/internal/multibuf_writer.h"
#include "pw_containers/vector.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

// Represents the Bluetooth ACL Data channel and tracks the Host<->Controller
// ACL data flow control.
//
// This currently only supports LE Packet-based Data Flow Control as defined in
// Core Spec v5.0, Vol 2, Part E, Section 4.1.1. Does not support sharing BR/EDR
// buffers.
class AclDataChannel {
 public:
  // Direction a packet is traveling on ACL transport.
  enum class Direction : bool {
    kFromController,
    kFromHost,
  };
  // Must match the number of Direction enumerators.
  static constexpr size_t kNumDirections = 2;

  static const char* ToString(Direction direction);

  // Used to `SendAcl` packets.
  class SendCredit {
   public:
    friend class AclDataChannel;

    SendCredit(const SendCredit&) = delete;
    SendCredit& operator=(const SendCredit&) = delete;
    // Move-only
    SendCredit(SendCredit&& other);
    SendCredit& operator=(SendCredit&& other);

    ~SendCredit();

   private:
    // Dispensed via `AclDataChannel::ReserveSendCredit()`.
    SendCredit(AclTransportType transport,
               Function<void(AclTransportType transport)>&& relinquish_fn);

    // Indicate that credits has been used for Tx.
    void MarkUsed();

    AclTransportType transport_;
    // If `this` was not used or moved and is destructed, `relinquish_fn_` is
    // called to replenish the credit that was subtracted from `AclDataChannel`.
    Function<void(AclTransportType transport)> relinquish_fn_;
  };

  AclDataChannel(HciTransport& hci_transport,
                 L2capChannelManager& l2cap_channel_manager,
                 uint16_t le_acl_credits_to_reserve,
                 uint16_t br_edr_acl_credits_to_reserve)
      : hci_transport_(hci_transport),
        l2cap_channel_manager_(l2cap_channel_manager),
        le_credits_(le_acl_credits_to_reserve),
        br_edr_credits_(br_edr_acl_credits_to_reserve) {}

  AclDataChannel(const AclDataChannel&) = delete;
  AclDataChannel& operator=(const AclDataChannel&) = delete;
  AclDataChannel(AclDataChannel&&) = delete;
  AclDataChannel& operator=(AclDataChannel&&) = delete;

  // Returns the max number of active ACL connections supported.
  static constexpr size_t GetMaxNumAclConnections() { return kMaxConnections; }

  // Revert to uninitialized state, clearing credit reservation and connections,
  // but not the number of credits to reserve nor HCI transport.
  void Reset();

  void ProcessReadBufferSizeCommandCompleteEvent(
      emboss::ReadBufferSizeCommandCompleteEventWriter read_buffer_event);

  // Acquires LE ACL credits for proxy host use by removing the amount needed
  // from the amount that is passed to the host.
  void ProcessLEReadBufferSizeCommandCompleteEvent(
      emboss::LEReadBufferSizeV1CommandCompleteEventWriter read_buffer_event) {
    ProcessSpecificLEReadBufferSizeCommandCompleteEvent(read_buffer_event);
  }

  void ProcessLEReadBufferSizeCommandCompleteEvent(
      emboss::LEReadBufferSizeV2CommandCompleteEventWriter read_buffer_event) {
    ProcessSpecificLEReadBufferSizeCommandCompleteEvent(read_buffer_event);
  }

  // Remove completed packets from `nocp_event` as necessary to reclaim LE ACL
  // credits that are associated with our credit-allocated connections.
  void HandleNumberOfCompletedPacketsEvent(H4PacketWithHci&& h4_packet);

  // Reclaim any credits we have associated with the removed connection and
  // notify `L2capChannelManager` of disconnection. This function just processes
  // the event; it does not handle forwarding it on.
  void ProcessDisconnectionCompleteEvent(pw::span<uint8_t> hci_span);

  // Create new tracked connection and pass on to host.
  void HandleConnectionCompleteEvent(H4PacketWithHci&& h4_packet);

  // Create new tracked connection and pass on to host.
  void HandleLeConnectionCompleteEvent(H4PacketWithHci&& h4_packet);

  // Create new tracked connection and pass on to host.
  void HandleLeEnhancedConnectionCompleteV1Event(H4PacketWithHci&& h4_packet);

  // Create new tracked connection and pass on to host.
  void HandleLeEnhancedConnectionCompleteV2Event(H4PacketWithHci&& h4_packet);

  /// Indicates whether the proxy has the capability of sending ACL packets.
  /// Note that this indicates intention, so it can be true even if the proxy
  /// has not yet or has been unable to reserve credits from the host.
  bool HasSendAclCapability(AclTransportType transport) const;

  /// @deprecated Use HasSendAclCapability with transport parameter instead.
  bool HasSendAclCapability() const {
    return HasSendAclCapability(AclTransportType::kLe);
  }

  // Returns the number of available ACL send credits for the proxy.
  // Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeAclPackets(AclTransportType transport) const;

  // In order to `SendAcl`, a `SendCredit` for the desired transport must be
  // provided.
  //
  // Returns std::nullopt if no credits are available for the desired transport.
  std::optional<SendCredit> ReserveSendCredit(AclTransportType transport);

  // Send an ACL data packet contained in an H4 packet to the controller.
  // Requires a reserved `SendCredit` that matches the transport of the
  // connection on which `h4_packet` is to be sent.
  //
  // Returns PW_STATUS_UNAVAILABLE if no ACL send credits were available.
  // Returns PW_STATUS_INVALID_ARGUMENT if ACL packet was ill-formed or `credit`
  // was provided for the wrong transport. See logs.
  // Returns PW_NOT_FOUND if ACL connection does not exist.
  pw::Status SendAcl(H4PacketWithH4&& h4_packet, SendCredit&& credit);

  // Register a new logical link on ACL logical transport.
  //
  // Returns PW_STATUS_OK if a connection was added.
  // Returns PW_STATUS_ALREADY EXISTS if a connection already exists.
  // Returns PW_STATUS_RESOURCE_EXHAUSTED if no space for additional connection.
  pw::Status CreateAclConnection(uint16_t connection_handle,
                                 AclTransportType transport);

  // Returns the signaling channel for this link if `connection_handle`
  // references a tracked connection and `local_cid` matches its id.
  L2capSignalingChannel* FindSignalingChannel(uint16_t connection_handle,
                                              uint16_t local_cid);

  // Handles an ACL Data frame.
  // Returns true if the frame was handled and is consumed by the proxy.
  // Returns false if the frame should be passed on to the other side.
  bool HandleAclData(AclDataChannel::Direction direction,
                     emboss::AclDataFrameWriter& acl);

 private:
  // An active logical link on ACL logical transport.
  // TODO: https://pwbug.dev/360929142 - Encapsulate all logic related to this
  // within a new LogicalLinkManager class?
  class AclConnection {
   public:
    AclConnection(AclTransportType transport,
                  uint16_t connection_handle,
                  uint16_t num_pending_packets,
                  L2capChannelManager& l2cap_channel_manager);

    AclConnection(const AclConnection&) = delete;
    AclConnection& operator=(const AclConnection&) = delete;
    AclConnection(AclConnection&&) = delete;
    AclConnection& operator=(AclConnection&&) = default;

    uint16_t connection_handle() const { return connection_handle_; }

    uint16_t num_pending_packets() const { return num_pending_packets_; }

    AclTransportType transport() const { return transport_; }

    void set_num_pending_packets(uint16_t new_val) {
      num_pending_packets_ = new_val;
    }

    L2capSignalingChannel* signaling_channel() {
      if (transport_ == AclTransportType::kLe) {
        return &leu_signaling_channel_;
      } else {
        return &aclu_signaling_channel_;
      }
    }

    // Returns true if recombination is active
    // (currently receiving and recombining fragments).
    bool RecombinationActive(Direction direction) {
      return bool(get_recombination_buffer(direction));
    }

    // Starts a new recombination session.
    //
    // Precondition: Recombination must not already be active
    // (RecombinationActive must be false).
    //
    // Returns:
    // * FAILED_PRECONDITION if recombination is already active.
    // * Any error from MultiBufWriter::Create(), namely RESOURCE_EXHAUSTED.
    // * OK if recombination is started.
    pw::Status StartRecombination(
        Direction direction,
        multibuf::MultiBufAllocator& multibuf_allocator,
        size_t size);

    // Adds a fragment of data to the recombination buffer.
    //
    // Precondition: Recombination must be active
    // (RecombinationActive must be true).
    //
    // Returns:
    // * FAILED_PRECONDITION if recombination is not active.
    // * Any error from MultiBufWriter::Write(), namely RESOURCE_EXHAUSTED.
    // * OK if the data was written, with value:
    //   * If recombination is incomplete, returns an empty MultiBuf.
    //   * If recombination is complete, returns a nonempty MultiBuf with the
    //     recombined data and ends recombination.
    pw::Result<multibuf::MultiBuf> RecombineFragment(
        Direction direction, pw::span<const uint8_t> data);

    // Ends recombination.
    void EndRecombination(Direction direction);

   private:
    AclTransportType transport_;
    uint16_t connection_handle_;
    uint16_t num_pending_packets_;
    L2capLeUSignalingChannel leu_signaling_channel_;
    // TODO: https://pwbug.dev/379172336 - Create correct signaling channel
    // type based on link type.
    L2capAclUSignalingChannel aclu_signaling_channel_;

    std::array<std::optional<MultiBufWriter>, kNumDirections>
        recombination_buffers_;

    MultiBufWriter* get_recombination_buffer(Direction direction) {
      auto& recomb = recombination_buffers_[cpp23::to_underlying(direction)];
      return recomb ? recomb.operator->() : nullptr;
    }
  };

  class Credits {
   public:
    explicit Credits(uint16_t to_reserve) : to_reserve_(to_reserve) {}

    void Reset();

    // Reserves credits from the controllers max and returns how many the host
    // can use.
    uint16_t Reserve(uint16_t controller_max);

    // Mark `num_credits` as pending.
    //
    // Returns Status::ResourceExhausted() if there aren't enough available
    // credits.
    Status MarkPending(uint16_t num_credits);

    // Return `num_credits` to available pool.
    void MarkCompleted(uint16_t num_credits);

    uint16_t Remaining() const { return proxy_max_ - proxy_pending_; }
    bool Available() const { return Remaining() > 0; }

    // If this class was initialized with some number of credits to reserve,
    // return true.
    bool HasSendCapability() const { return to_reserve_ > 0; }

    // If this class has already had credits reserved from the controller.
    bool Initialized() const { return proxy_max_ > 0; }

   private:
    const uint16_t to_reserve_;
    // The local number of HCI ACL Data packets that we have reserved for
    // this proxy host to use.
    uint16_t proxy_max_ = 0;
    // The number of HCI ACL Data packets that we have sent to the controller
    // and have not yet completed.
    uint16_t proxy_pending_ = 0;
  };

  // Guards interactions with ACL connection objects.
  mutable pw::sync::Mutex connection_mutex_ PW_ACQUIRED_BEFORE(credit_mutex_);

  // Returns pointer to AclConnection with provided `connection_handle` in
  // `acl_connections_`. Returns nullptr if no such connection exists.
  //
  // Pointer is assured valid only as long as `mutex_` is held.
  AclConnection* FindAclConnection(uint16_t connection_handle)
      PW_EXCLUSIVE_LOCKS_REQUIRED(connection_mutex_);

  Credits& LookupCredits(AclTransportType transport)
      PW_EXCLUSIVE_LOCKS_REQUIRED(credit_mutex_);

  const Credits& LookupCredits(AclTransportType transport) const
      PW_EXCLUSIVE_LOCKS_REQUIRED(credit_mutex_);

  void HandleLeConnectionCompleteEvent(uint16_t connection_handle,
                                       emboss::StatusCode status);

  // Data members

  // Maximum number of active ACL connections supported.
  // TODO: https://pwbug.dev/349700888 - Make size configurable.
  static constexpr size_t kMaxConnections = 10;

  // Reference to the transport owned by the host.
  HciTransport& hci_transport_;

  // TODO: https://pwbug.dev/360929142 - Remove this circular dependency.
  L2capChannelManager& l2cap_channel_manager_;

  // Credit allocation will happen inside a mutex since it crosses thread
  // boundaries.
  mutable pw::sync::Mutex credit_mutex_ PW_ACQUIRED_AFTER(connection_mutex_);

  Credits le_credits_ PW_GUARDED_BY(credit_mutex_);
  Credits br_edr_credits_ PW_GUARDED_BY(credit_mutex_);

  // List of credit-allocated ACL connections.
  pw::Vector<AclConnection, kMaxConnections> acl_connections_
      PW_GUARDED_BY(connection_mutex_);

  // Instantiated in acl_data_channel.cc for
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter` and
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter`.
  template <class EventT>
  void ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
      EventT read_buffer_event);
};

}  // namespace pw::bluetooth::proxy
