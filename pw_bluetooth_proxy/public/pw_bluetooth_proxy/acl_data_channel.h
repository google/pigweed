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

#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth_proxy/hci_transport.h"

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
                 uint16_t le_acl_credits_to_reserve)
      : hci_transport_(hci_transport),
        le_acl_credits_to_reserve_(le_acl_credits_to_reserve) {}

  AclDataChannel(const AclDataChannel&) = delete;
  AclDataChannel& operator=(const AclDataChannel&) = delete;
  AclDataChannel(AclDataChannel&&) = delete;
  AclDataChannel& operator=(AclDataChannel&&) = delete;

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

  // Returns the number of LE ACL send credits reserved for the proxy.
  uint16_t GetLeAclCreditsToReserve() const;

  // Returns the number of available LE ACL send credits for the proxy.
  // Can be zero if the controller has not yet been initialized by the host.
  uint16_t GetNumFreeLeAclPackets() const;

  // Send an ACL data packet contained in an H4 packet to the controller.
  // Returns false if no LE ACL send credits were available to send the packet.
  bool SendAcl(H4PacketWithH4&& h4_packet);

 private:
  // Set to true if channel has been initialized by the host.
  bool initialized_ = false;

  // Reference to the transport owned by the host.
  HciTransport& hci_transport_;

  // The amount of credits this channel will try to reserve.
  const uint16_t le_acl_credits_to_reserve_;

  // The local number of HCI ACL Data packets that we have reserved for this
  // proxy host to use.
  // TODO: https://pwbug.dev/326499611 - Mutex once we are using for sends.
  uint16_t proxy_max_le_acl_packets_ = 0;

  // Instantiated in acl_data_channel.cc for
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter` and
  // `emboss::LEReadBufferSizeV1CommandCompleteEventWriter`.
  template <class EventT>
  void ProcessSpecificLEReadBufferSizeCommandCompleteEvent(
      EventT read_buffer_event);
};

}  // namespace pw::bluetooth::proxy
