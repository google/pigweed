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
#include "pw_bluetooth_proxy/client_channel.h"
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
class L2capChannel : public ClientChannel {
 public:
  L2capChannel(const L2capChannel& other) = delete;
  L2capChannel& operator=(const L2capChannel& other) = delete;
  // Channels are moved to the client after construction.
  L2capChannel(L2capChannel&& other);
  // Move assignment operator allows channels to be erased from pw::Vector.
  L2capChannel& operator=(L2capChannel&& other);

  ~L2capChannel() override;

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

  uint16_t local_cid() const { return local_cid_; }

  uint16_t remote_cid() const { return remote_cid_; }

  uint16_t connection_handle() const { return connection_handle_; }

  AclTransportType transport() const { return transport_; }

  multibuf::MultiBufAllocator* rx_multibuf_allocator() const {
    return rx_multibuf_allocator_;
  }

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

  //----------------
  //  Tx (protected)
  //----------------

  void HandleStop() override;
  void HandleClose() override;
  void HandlePacketsMayBeReadyToSend() override
      PW_LOCKS_EXCLUDED(send_queue_mutex());

  // Reserve an L2CAP over ACL over H4 packet, with those three headers
  // populated for an L2CAP PDU payload of `data_length` bytes addressed to
  // `connection_handle_`.
  //
  // Returns PW_STATUS_INVALID_ARGUMENT if payload is too large for a
  // buffer. Returns PW_STATUS_UNAVAILABLE if all buffers are currently
  // occupied.
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
      uint16_t data_length) PW_LOCKS_EXCLUDED(send_queue_mutex());

  // Returns the maximum size supported for Tx L2CAP PDU payloads.
  //
  // Returns std::nullopt if LE_ACL_Data_Packet_Length was not yet provided in
  // an LE_Read_Buffer_Size command complete event.
  std::optional<uint16_t> MaxL2capPayloadSize() const;

  //-------
  //  Rx (protected)
  //-------

  // Returns false if payload should be forwarded to controller instead.
  virtual bool SendPayloadFromHostToClient(pw::span<uint8_t> payload);

  // Returns false if payload should be forwarded to host instead.
  virtual bool SendPayloadFromControllerToClient(pw::span<uint8_t> payload);

 private:
  static constexpr uint16_t kMaxValidConnectionHandle = 0x0EFF;

  // Returns false if payload should be forwarded to host instead.
  bool SendPayloadToClient(pw::span<uint8_t> payload,
                           OptionalPayloadReceiveCallback& callback);

  // Helper for move constructor and move assignment.
  void MoveFields(L2capChannel& other);

  L2capChannelManager& l2cap_channel_manager_;

  // ACL connection handle.
  uint16_t connection_handle_;

  AclTransportType transport_;

  // L2CAP channel ID of local endpoint.
  uint16_t local_cid_;

  // L2CAP channel ID of remote endpoint.
  uint16_t remote_cid_;

  // Reserve an L2CAP packet over ACL over H4 packet.
  pw::Result<H4PacketWithH4> PopulateL2capPacket(uint16_t data_length);

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
