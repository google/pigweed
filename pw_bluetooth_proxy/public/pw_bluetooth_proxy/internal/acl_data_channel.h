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

#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth_proxy/internal/hci_transport.h"
#include "pw_bluetooth_proxy/internal/l2cap_aclu_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_leu_signaling_channel.h"
#include "pw_bluetooth_proxy/internal/logical_transport.h"
#include "pw_containers/vector.h"
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

  // Returns the max number of simultaneous LE ACL connections supported.
  static constexpr size_t GetMaxNumLeAclConnections() {
    return kMaxConnections;
  }

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

  // Reclaim any credits we have associated with the removed connection.
  void HandleDisconnectionCompleteEvent(H4PacketWithHci&& h4_packet);

  /// Indicates whether the proxy has the capability of sending ACL packets.
  /// Note that this indicates intention, so it can be true even if the proxy
  /// has not yet or has been unable to reserve credits from the host.
  bool HasSendAclCapability(AclTransport transport) const;

  /// @deprecated Use HasSendAclCapability with transport parameter instead.
  bool HasSendAclCapability() const {
    return HasSendAclCapability(AclTransport::kLe);
  }

  // Returns the number of available ACL send credits for the proxy.
  // Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeAclPackets(AclTransport transport) const;

  // Send an ACL data packet contained in an H4 packet to the controller.
  //
  // Returns PW_STATUS_UNAVAILABLE if no ACL send credits were available.
  // Returns PW_STATUS_INVALID_ARGUMENT if ACL packet was ill-formed.
  // Returns PW_NOT_FOUND if ACL connection does not exist.
  pw::Status SendAcl(H4PacketWithH4&& h4_packet);

  // Register a new logical link on ACL logical transport.
  //
  // Returns PW_STATUS_OK if a connection was added.
  // Returns PW_STATUS_ALREADY EXISTS if a connection already exists.
  // Returns PW_STATUS_RESOURCE_EXHAUSTED if no space for additional connection.
  pw::Status CreateAclConnection(uint16_t connection_handle,
                                 AclTransport transport);

  // Sets `is_receiving_fragmented_pdu` flag for connection.
  //
  // Returns PW_STATUS_NOT_FOUND if connection does not exist.
  // Returns PW_STATUS_FAILED_PRECONDITION if already receiving fragmented PDU.
  pw::Status FragmentedPduStarted(uint16_t connection_handle);

  // Returns `is_receiving_fragmented_pdu` flag for connection.
  //
  // Returns PW_STATUS_NOT_FOUND if connection does not exist.
  pw::Result<bool> IsReceivingFragmentedPdu(uint16_t connection_handle);

  // Unsets `is_receiving_fragmented_pdu` flag for connection.
  //
  // Returns PW_STATUS_NOT_FOUND if connection does not exist.
  // Returns PW_STATUS_FAILED_PRECONDITION if not receiving fragmented PDU.
  pw::Status FragmentedPduFinished(uint16_t connection_handle);

 private:
  // An active logical link on ACL logical transport.
  // TODO: https://pwbug.dev/360929142 - Encapsulate all logic related to this
  // within a new LogicalLinkManager class?
  class AclConnection {
   public:
    AclConnection(AclTransport transport,
                  uint16_t connection_handle,
                  uint16_t num_pending_packets,
                  L2capChannelManager& l2cap_channel_manager)
        : transport_(transport),
          connection_handle_(connection_handle),
          num_pending_packets_(num_pending_packets),
          leu_signaling_channel_(l2cap_channel_manager, connection_handle),
          aclu_signaling_channel_(l2cap_channel_manager, connection_handle),
          is_receiving_fragmented_pdu_(false) {}

    AclConnection& operator=(AclConnection&& other) = default;

    uint16_t connection_handle() const { return connection_handle_; }

    uint16_t num_pending_packets() const { return num_pending_packets_; }

    AclTransport transport() const { return transport_; }

    void set_num_pending_packets(uint16_t new_val) {
      num_pending_packets_ = new_val;
    }

    bool is_receiving_fragmented_pdu() const {
      return is_receiving_fragmented_pdu_;
    }

    void set_is_receiving_fragmented_pdu(bool new_val) {
      is_receiving_fragmented_pdu_ = new_val;
    }

   private:
    AclTransport transport_;
    uint16_t connection_handle_;
    uint16_t num_pending_packets_;
    L2capLeUSignalingChannel leu_signaling_channel_;
    // TODO: https://pwbug.dev/379172336 - Create correct signaling channel
    // type based on link type.
    L2capAclUSignalingChannel aclu_signaling_channel_;
    // Set when a fragmented PDU is received. Continuing fragments are dropped
    // until the PDU has been consumed, then this is unset.
    // TODO: https://pwbug.dev/365179076 - Support recombination.
    bool is_receiving_fragmented_pdu_;
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

   private:
    const uint16_t to_reserve_;
    // The local number of HCI ACL Data packets that we have reserved for
    // this proxy host to use.
    uint16_t proxy_max_ = 0;
    // The number of HCI ACL Data packets that we have sent to the controller
    // and have not yet completed.
    uint16_t proxy_pending_ = 0;
  };

  // Returns pointer to AclConnection with provided `connection_handle` in
  // `active_acl_connections_`. Returns nullptr if no such connection exists.
  AclConnection* FindAclConnection(uint16_t connection_handle)
      PW_EXCLUSIVE_LOCKS_REQUIRED(credit_allocation_mutex_);

  Credits& LookupCredits(AclTransport transport)
      PW_EXCLUSIVE_LOCKS_REQUIRED(credit_allocation_mutex_);

  const Credits& LookupCredits(AclTransport transport) const
      PW_EXCLUSIVE_LOCKS_REQUIRED(credit_allocation_mutex_);

  // Maximum number of simultaneous credit-allocated LE connections supported.
  // TODO: https://pwbug.dev/349700888 - Make size configurable.
  static constexpr size_t kMaxConnections = 10;

  // Reference to the transport owned by the host.
  HciTransport& hci_transport_;

  // TODO: https://pwbug.dev/360929142 - Remove this circular dependency.
  L2capChannelManager& l2cap_channel_manager_;

  // Credit allocation will happen inside a mutex since it crosses thread
  // boundaries. The mutex also guards interactions with ACL connection objects.
  mutable pw::sync::Mutex credit_allocation_mutex_;

  Credits le_credits_ PW_GUARDED_BY(credit_allocation_mutex_);
  Credits br_edr_credits_ PW_GUARDED_BY(credit_allocation_mutex_);

  // List of credit-allocated ACL connections.
  pw::Vector<AclConnection, kMaxConnections> active_acl_connections_
      PW_GUARDED_BY(credit_allocation_mutex_);

  // Instantiated in acl_data_channel.cc for
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter` and
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter`.
  template <class EventT>
  void ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
      EventT read_buffer_event);
};

}  // namespace pw::bluetooth::proxy
