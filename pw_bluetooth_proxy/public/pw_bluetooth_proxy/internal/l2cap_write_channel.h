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

#include "pw_bluetooth_proxy/h4_packet.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/intrusive_forward_list.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

class L2capChannelManager;

// Base class for peer-to-peer L2CAP-based channels supporting writing.
//
// Derived channels should expose, at minimum, a "write" operation that
// takes a variable information payload. Protocol-dependent information that
// is fixed per channel, such as addresses, flags, handles, etc. should be
// provided at construction to derived channels.
class L2capWriteChannel : public IntrusiveForwardList<L2capWriteChannel>::Item {
 public:
  L2capWriteChannel(const L2capWriteChannel& other) = delete;
  L2capWriteChannel& operator=(const L2capWriteChannel& other) = delete;
  L2capWriteChannel(L2capWriteChannel&& other);
  // TODO: https://pwbug.dev/360929142 - Define move assignment operator so
  // write channels can be erased from containers.
  L2capWriteChannel& operator=(L2capWriteChannel&& other) = delete;

  virtual ~L2capWriteChannel();

  // Max number of Tx L2CAP packets that can be waiting to send.
  static constexpr size_t QueueCapacity() { return kQueueCapacity; }

  // Queue L2CAP `packet` for sending and `ReportPacketsMayBeReadyToSend()`.
  //
  // Returns PW_STATUS_UNAVAILABLE if queue is full (transient error).
  [[nodiscard]] virtual Status QueuePacket(H4PacketWithH4&& packet);

  // Dequeue a packet if one is available to send.
  [[nodiscard]] virtual std::optional<H4PacketWithH4> DequeuePacket();

  // Get the destination L2CAP channel ID.
  uint16_t remote_cid() const { return remote_cid_; }

  // Get the ACL connection handle.
  uint16_t connection_handle() const { return connection_handle_; }

 protected:
  explicit L2capWriteChannel(L2capChannelManager& l2cap_channel_manager,
                             uint16_t connection_handle,
                             uint16_t remote_cid);

  // Returns whether or not ACL connection handle & destination L2CAP channel
  // identifier are valid parameters for a packet.
  [[nodiscard]] static bool AreValidParameters(uint16_t connection_handle,
                                               uint16_t remote_cid);

  // Reserve an L2CAP over ACL over H4 packet, with those three headers
  // populated for an L2CAP PDU payload of `data_length` bytes addressed to
  // `connection_handle_`.
  //
  // Returns PW_STATUS_INVALID_ARGUMENT if payload is too large for a buffer.
  // Returns PW_STATUS_UNAVAILABLE if all buffers are currently occupied.
  pw::Result<H4PacketWithH4> PopulateTxL2capPacket(uint16_t data_length);

  // Returns the maximum size supported for Tx L2CAP PDU payloads.
  uint16_t MaxL2capPayloadSize() const;

  // Alert `L2capChannelManager` that queued packets may be ready to send.
  // When calling this method, ensure no locks are held that are also acquired
  // in `Dequeue()` overrides.
  void ReportPacketsMayBeReadyToSend();

  // Remove all packets from queue.
  void ClearQueue();

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // TODO: https://pwbug.dev/349700888 - Make capacity configurable.
  static constexpr size_t kQueueCapacity = 5;

  // ACL connection handle on remote peer to which packets are sent.
  uint16_t connection_handle_;

  // L2CAP channel ID on remote peer to which packets are sent.
  uint16_t remote_cid_;

  // `L2capChannelManager` and channel may concurrently call functions that
  // access queue.
  sync::Mutex send_queue_mutex_;

  // Stores Tx L2CAP packets.
  InlineQueue<H4PacketWithH4, kQueueCapacity> send_queue_
      PW_GUARDED_BY(send_queue_mutex_);

  L2capChannelManager& l2cap_channel_manager_;
};

}  // namespace pw::bluetooth::proxy
